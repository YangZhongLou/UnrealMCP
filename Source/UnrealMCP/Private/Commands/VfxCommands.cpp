#if WITH_EDITOR
#include "CoreMinimal.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/Texture.h"
#include "EngineUtils.h"
#include "Components/StaticMeshComponent.h"
#include "Components/MeshComponent.h"
#include "Materials/MaterialInstance.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Editor.h"
#include "EditorAssetLibrary.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "AssetImportTask.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "NiagaraActor.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "NiagaraCommon.h"
#include "NiagaraParameterStore.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Misc/Paths.h"
#include "Misc/Guid.h"
#include "Misc/FileHelper.h"
#include "Misc/MonitoredProcess.h"
#include "HAL/PlatformFileManager.h"
#include "HAL/PlatformProcess.h"
#include "HAL/CriticalSection.h"
#include "Async/Async.h"
#include "UObject/SavePackage.h"

DEFINE_LOG_CATEGORY_STATIC(LogMCPVfxCommands, Log, All);

// ---------------------------------------------------------------------------
// RAII guard for GIsRunningUnattendedScript
// ---------------------------------------------------------------------------
struct FUnattendedScriptGuard
{
    bool bPrevious;
    FUnattendedScriptGuard()
        : bPrevious(GIsRunningUnattendedScript)
    {
        GIsRunningUnattendedScript = true;
    }
    ~FUnattendedScriptGuard()
    {
        GIsRunningUnattendedScript = bPrevious;
    }
};

// ---------------------------------------------------------------------------
// Helper: JSON response builders
// ---------------------------------------------------------------------------
static FString VfxBuildError(const FString& Error)
{
    TSharedPtr<FJsonObject> Response = MakeShareable(new FJsonObject);
    Response->SetBoolField(TEXT("success"), false);
    Response->SetStringField(TEXT("error"), Error);

    FString Out;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Out);
    FJsonSerializer::Serialize(Response.ToSharedRef(), Writer);
    return Out;
}

static FString VfxBuildSuccess(const TSharedPtr<FJsonObject>& Result)
{
    TSharedPtr<FJsonObject> Response = MakeShareable(new FJsonObject);
    Response->SetBoolField(TEXT("success"), true);
    Response->SetObjectField(TEXT("result"), Result);

    FString Out;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Out);
    FJsonSerializer::Serialize(Response.ToSharedRef(), Writer);
    return Out;
}

// ---------------------------------------------------------------------------
// Helper: Vector parsing / serialization
// ---------------------------------------------------------------------------
static FVector ParseVector3(const TSharedPtr<FJsonObject>& Params, const FString& Field, const FVector& Default = FVector::ZeroVector)
{
    if (!Params.IsValid() || !Params->HasField(Field))
    {
        return Default;
    }

    const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
    if (!Params->TryGetArrayField(Field, Arr) || !Arr || Arr->Num() < 3)
    {
        return Default;
    }

    double X = 0.0;
    double Y = 0.0;
    double Z = 0.0;
    if (!(*Arr)[0]->TryGetNumber(X) || !(*Arr)[1]->TryGetNumber(Y) || !(*Arr)[2]->TryGetNumber(Z))
    {
        return Default;
    }
    return FVector(X, Y, Z);
}

static TSharedPtr<FJsonValueArray> VectorToJsonArray(const FVector& V)
{
    TArray<TSharedPtr<FJsonValue>> Arr;
    Arr.Add(MakeShareable(new FJsonValueNumber(V.X)));
    Arr.Add(MakeShareable(new FJsonValueNumber(V.Y)));
    Arr.Add(MakeShareable(new FJsonValueNumber(V.Z)));
    return MakeShareable(new FJsonValueArray(Arr));
}

// ---------------------------------------------------------------------------
// Helper: Actor lookup
// ---------------------------------------------------------------------------
static AActor* FindActorByName(UWorld* World, const FString& Name)
{
    if (!World)
    {
        return nullptr;
    }
    for (TActorIterator<AActor> It(World); It; ++It)
    {
        if (It->GetName() == Name)
        {
            return *It;
        }
    }
    return nullptr;
}

// ---------------------------------------------------------------------------
// generate_and_import_3d: asynchronous generation job bookkeeping
// ---------------------------------------------------------------------------
namespace
{
    enum class EGenStatus
    {
        Pending,
        Running,
        Completed,
        Failed
    };

    struct FGenerationJob
    {
        FString JobId;
        EGenStatus Status;
        double Progress;
        FString Message;
        FString MeshFile;
        FString AssetPath;
        FString ActorName;

        explicit FGenerationJob(const FString& InJobId)
            : JobId(InJobId)
            , Status(EGenStatus::Pending)
            , Progress(0.0)
        {
        }
    };

    static FCriticalSection GenerationJobLock;
    static TMap<FString, TSharedPtr<FGenerationJob>> GenerationJobs;
}

// ---------------------------------------------------------------------------
// Helper: Import a mesh file and spawn a StaticMeshActor in the scene.
// ---------------------------------------------------------------------------
static bool ImportAndSpawnStaticMesh(
    const FString& MeshFile,
    const FString& DestinationPath,
    const FString& InActorName,
    const FVector& Location,
    const FRotator& Rotation,
    const FVector& Scale,
    FString& OutAssetPath,
    FString& OutActorName,
    FString& OutError)
{
    bool bSuccess = false;
    FEvent* DoneEvent = FPlatformProcess::GetSynchEventFromPool();

    AsyncTask(ENamedThreads::GameThread, [&]()
    {
        FUnattendedScriptGuard UnattendedGuard;

        UAssetImportTask* ImportTask = NewObject<UAssetImportTask>();
        ImportTask->Filename = MeshFile;
        ImportTask->DestinationPath = DestinationPath;
        ImportTask->bAutomatedImportProcedure = true;
        ImportTask->bAsync = false;
        ImportTask->bReplaceExisting = true;
        ImportTask->bSave = false;

        FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");
        TArray<UAssetImportTask*> ImportTasks;
        ImportTasks.Add(ImportTask);
        AssetToolsModule.Get().ImportAssetTasks(ImportTasks);

        UStaticMesh* StaticMesh = nullptr;
        if (ImportTask->ImportedObjectPaths.Num() > 0)
        {
            FSoftObjectPath ImportedPath = ImportTask->ImportedObjectPaths[0];
            if (UObject* ImportedObject = ImportedPath.ResolveObject())
            {
                StaticMesh = Cast<UStaticMesh>(ImportedObject);
            }
            if (!StaticMesh)
            {
                StaticMesh = Cast<UStaticMesh>(ImportedPath.TryLoad());
            }
        }

        if (!StaticMesh)
        {
            FString BaseName = FPaths::GetBaseFilename(MeshFile);
            StaticMesh = LoadObject<UStaticMesh>(nullptr, *FString::Printf(TEXT("%s/%s.%s"), *DestinationPath, *BaseName, *BaseName));
        }

        if (!StaticMesh)
        {
            OutError = FString::Printf(TEXT("Failed to import static mesh from: %s"), *MeshFile);
            DoneEvent->Trigger();
            return;
        }

        OutAssetPath = StaticMesh->GetPathName();

        UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
        if (!World)
        {
            OutError = TEXT("No world available");
            DoneEvent->Trigger();
            return;
        }

        FString SpawnName = InActorName.IsEmpty()
            ? FString::Printf(TEXT("AIGenerated_%s"), *StaticMesh->GetName())
            : InActorName;

        FActorSpawnParameters SpawnParams;
        SpawnParams.Name = FName(*SpawnName);
        SpawnParams.NameMode = FActorSpawnParameters::ESpawnActorNameMode::Requested;

        AStaticMeshActor* Actor = World->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), Location, Rotation, SpawnParams);
        if (!Actor)
        {
            OutError = TEXT("Failed to spawn StaticMeshActor");
            DoneEvent->Trigger();
            return;
        }

        if (UStaticMeshComponent* SMC = Actor->GetStaticMeshComponent())
        {
            SMC->SetStaticMesh(StaticMesh);
        }
        Actor->SetActorScale3D(Scale);
        Actor->MarkPackageDirty();

        OutActorName = Actor->GetName();
        bSuccess = true;

        DoneEvent->Trigger();
    });

    DoneEvent->Wait();
    FPlatformProcess::ReturnSynchEventToPool(DoneEvent);
    return bSuccess;
}

// ---------------------------------------------------------------------------
// generate_and_import_3d: background generation worker
// ---------------------------------------------------------------------------
static TFuture<void> LaunchGenerationJob(
    TSharedPtr<FGenerationJob> Job,
    const FString& PythonExe,
    const FString& ScriptPath,
    const FString& Args,
    const FString& DestinationPath,
    const FString& InActorName,
    const FVector& Location,
    const FRotator& Rotation,
    const FVector& Scale)
{
    return Async(EAsyncExecution::Thread, [=]() mutable
    {
        {
            FScopeLock Lock(&GenerationJobLock);
            Job->Status = EGenStatus::Running;
            Job->Progress = 0.0;
            Job->Message = TEXT("Generating mesh...");
        }

        FString Params = FString::Printf(TEXT("\"%s\" %s"), *ScriptPath, *Args);
        TSharedPtr<FMonitoredProcess> Process = MakeShareable(new FMonitoredProcess(PythonExe, Params, true, true));
        if (!Process->Launch())
        {
            FScopeLock Lock(&GenerationJobLock);
            Job->Status = EGenStatus::Failed;
            Job->Progress = 1.0;
            Job->Message = TEXT("Failed to launch generation subprocess");
            return;
        }

        while (Process->Update())
        {
            FPlatformProcess::Sleep(0.1f);
        }

        const FString Output = Process->GetFullOutputWithoutDelegate();
        if (Process->GetReturnCode() != 0)
        {
            FScopeLock Lock(&GenerationJobLock);
            Job->Status = EGenStatus::Failed;
            Job->Progress = 1.0;
            Job->Message = FString::Printf(TEXT("Generation subprocess failed: %s"), *Output.Left(256));
            return;
        }

        FString JsonText = Output;
        const int32 Start = Output.Find(TEXT("{"));
        if (Start != INDEX_NONE)
        {
            JsonText = Output.Mid(Start);
        }

        TSharedPtr<FJsonObject> ResultObj;
        TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonText);
        if (!FJsonSerializer::Deserialize(Reader, ResultObj) || !ResultObj.IsValid())
        {
            FScopeLock Lock(&GenerationJobLock);
            Job->Status = EGenStatus::Failed;
            Job->Progress = 1.0;
            Job->Message = FString::Printf(TEXT("Failed to parse generation output: %s"), *Output.Left(256));
            return;
        }

        FString MeshFile;
        if (!ResultObj->TryGetStringField(TEXT("mesh_path"), MeshFile) || MeshFile.IsEmpty())
        {
            FScopeLock Lock(&GenerationJobLock);
            Job->Status = EGenStatus::Failed;
            Job->Progress = 1.0;
            Job->Message = TEXT("Generation result missing mesh_path");
            return;
        }

        FString AssetPath;
        FString SpawnedActorName;
        FString Error;
        if (!ImportAndSpawnStaticMesh(MeshFile, DestinationPath, InActorName, Location, Rotation, Scale, AssetPath, SpawnedActorName, Error))
        {
            FScopeLock Lock(&GenerationJobLock);
            Job->Status = EGenStatus::Failed;
            Job->Progress = 1.0;
            Job->Message = Error;
            return;
        }

        {
            FScopeLock Lock(&GenerationJobLock);
            Job->Status = EGenStatus::Completed;
            Job->Progress = 1.0;
            Job->MeshFile = MeshFile;
            Job->AssetPath = AssetPath;
            Job->ActorName = SpawnedActorName;
            Job->Message = TEXT("Generation and import completed");
        }
    });
}

// ---------------------------------------------------------------------------
// Handler: generate_and_import_3d
// ---------------------------------------------------------------------------
FString HandleGenerateAndImport3D(const TSharedPtr<FJsonObject>& Params)
{
    FString DestinationPath;
    if (!Params->TryGetStringField(TEXT("destinationPath"), DestinationPath) || DestinationPath.IsEmpty())
    {
        return VfxBuildError(TEXT("Missing required parameter: destinationPath"));
    }

    FString MeshFile;
    Params->TryGetStringField(TEXT("meshFile"), MeshFile);

    FString ReferenceImage;
    Params->TryGetStringField(TEXT("referenceImage"), ReferenceImage);

    FString Prompt;
    Params->TryGetStringField(TEXT("prompt"), Prompt);

    FString ActorName;
    Params->TryGetStringField(TEXT("actorName"), ActorName);

    const FVector Location = ParseVector3(Params, TEXT("location"), FVector::ZeroVector);
    const FVector RotationVec = ParseVector3(Params, TEXT("rotation"), FVector::ZeroVector);
    const FRotator Rotation(RotationVec.X, RotationVec.Y, RotationVec.Z);
    const FVector Scale = ParseVector3(Params, TEXT("scale"), FVector::OneVector);

    const bool bWaitForCompletion = Params->HasField(TEXT("waitForCompletion")) ? Params->GetBoolField(TEXT("waitForCompletion")) : false;

    // Direct import path: meshFile is provided.
    if (!MeshFile.IsEmpty())
    {
        FString AssetPath;
        FString SpawnedActorName;
        FString Error;
        if (!ImportAndSpawnStaticMesh(MeshFile, DestinationPath, ActorName, Location, Rotation, Scale, AssetPath, SpawnedActorName, Error))
        {
            return VfxBuildError(Error);
        }

        TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
        Result->SetStringField(TEXT("assetPath"), AssetPath);
        Result->SetStringField(TEXT("actorName"), SpawnedActorName);
        Result->SetStringField(TEXT("meshFile"), MeshFile);
        Result->SetArrayField(TEXT("location"), VectorToJsonArray(Location)->AsArray());
        Result->SetArrayField(TEXT("rotation"), VectorToJsonArray(RotationVec)->AsArray());
        Result->SetArrayField(TEXT("scale"), VectorToJsonArray(Scale)->AsArray());
        Result->SetStringField(TEXT("status"), TEXT("completed"));
        return VfxBuildSuccess(Result);
    }

    // Generation path: referenceImage is required in this MVP.
    if (ReferenceImage.IsEmpty())
    {
        return VfxBuildError(TEXT("Generation requires referenceImage; prompt-only generation is not supported in this MVP"));
    }

    FString JobId = FString::Printf(TEXT("gen-3d-%s"), *FGuid::NewGuid().ToString());
    FString OutputRoot = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / TEXT("hunyuan/output") / JobId);
    FString ScriptPath = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / TEXT("hunyuan/vfx_generation_service.py"));
    FString PythonExe = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / TEXT("hunyuan/venv/Scripts/python.exe"));
    if (!FPaths::FileExists(PythonExe))
    {
        PythonExe = TEXT("python");
    }

    // Build command-line arguments for vfx_generation_service.py
    FString Args = FString::Printf(TEXT("\"%s\" --output-root \"%s\""), *ReferenceImage, *OutputRoot);

    TSharedPtr<FJsonObject> GenParams = Params->HasField(TEXT("generationParams")) ? Params->GetObjectField(TEXT("generationParams")) : nullptr;
    if (GenParams.IsValid())
    {
        int32 Seed = 0;
        if (GenParams->TryGetNumberField(TEXT("seed"), Seed))
        {
            Args += FString::Printf(TEXT(" --seed %d"), Seed);
        }

        int32 Steps = 0;
        if (GenParams->TryGetNumberField(TEXT("steps"), Steps))
        {
            Args += FString::Printf(TEXT(" --num-inference-steps %d"), Steps);
        }

        double GuidanceScale = 0.0;
        if (GenParams->TryGetNumberField(TEXT("guidanceScale"), GuidanceScale))
        {
            Args += FString::Printf(TEXT(" --guidance-scale %f"), GuidanceScale);
        }

        int32 OctreeResolution = 0;
        if (GenParams->TryGetNumberField(TEXT("octreeResolution"), OctreeResolution))
        {
            Args += FString::Printf(TEXT(" --octree-resolution %d"), OctreeResolution);
        }

        bool bTurbo = false;
        if (GenParams->TryGetBoolField(TEXT("turbo"), bTurbo) && bTurbo)
        {
            Args += TEXT(" --low-vram-mode");
        }

        FString CustomOutputDir;
        if (GenParams->TryGetStringField(TEXT("outputDir"), CustomOutputDir) && !CustomOutputDir.IsEmpty())
        {
            OutputRoot = FPaths::ConvertRelativePathToFull(CustomOutputDir);
            // Rebuild the output-root argument at the front.
            Args = FString::Printf(TEXT("\"%s\" --output-root \"%s\""), *ReferenceImage, *OutputRoot);

            if (GenParams->TryGetNumberField(TEXT("seed"), Seed))
            {
                Args += FString::Printf(TEXT(" --seed %d"), Seed);
            }
            if (GenParams->TryGetNumberField(TEXT("steps"), Steps))
            {
                Args += FString::Printf(TEXT(" --num-inference-steps %d"), Steps);
            }
            if (GenParams->TryGetNumberField(TEXT("guidanceScale"), GuidanceScale))
            {
                Args += FString::Printf(TEXT(" --guidance-scale %f"), GuidanceScale);
            }
            if (GenParams->TryGetNumberField(TEXT("octreeResolution"), OctreeResolution))
            {
                Args += FString::Printf(TEXT(" --octree-resolution %d"), OctreeResolution);
            }
            if (GenParams->TryGetBoolField(TEXT("turbo"), bTurbo) && bTurbo)
            {
                Args += TEXT(" --low-vram-mode");
            }
        }
    }

    TSharedPtr<FGenerationJob> Job = MakeShareable(new FGenerationJob(JobId));
    {
        FScopeLock Lock(&GenerationJobLock);
        GenerationJobs.Add(JobId, Job);
    }

    TFuture<void> Future = LaunchGenerationJob(Job, PythonExe, ScriptPath, Args, DestinationPath, ActorName, Location, Rotation, Scale);

    if (bWaitForCompletion)
    {
        Future.Wait();

        FScopeLock Lock(&GenerationJobLock);
        if (Job->Status == EGenStatus::Completed)
        {
            TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
            Result->SetStringField(TEXT("assetPath"), Job->AssetPath);
            Result->SetStringField(TEXT("actorName"), Job->ActorName);
            Result->SetStringField(TEXT("meshFile"), Job->MeshFile);
            Result->SetArrayField(TEXT("location"), VectorToJsonArray(Location)->AsArray());
            Result->SetArrayField(TEXT("rotation"), VectorToJsonArray(RotationVec)->AsArray());
            Result->SetArrayField(TEXT("scale"), VectorToJsonArray(Scale)->AsArray());
            Result->SetStringField(TEXT("status"), TEXT("completed"));
            return VfxBuildSuccess(Result);
        }
        else
        {
            return VfxBuildError(Job->Message.IsEmpty() ? TEXT("Generation failed") : Job->Message);
        }
    }

    TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
    Result->SetStringField(TEXT("jobId"), JobId);
    Result->SetStringField(TEXT("status"), TEXT("pending"));
    Result->SetStringField(TEXT("message"), TEXT("Hunyuan3D generation started, use get_generate_and_import_3d_status to poll."));
    return VfxBuildSuccess(Result);
}

// ---------------------------------------------------------------------------
// Handler: get_generate_and_import_3d_status
// ---------------------------------------------------------------------------
FString HandleGetGenerateAndImport3DStatus(const TSharedPtr<FJsonObject>& Params)
{
    FString JobId;
    if (!Params->TryGetStringField(TEXT("jobId"), JobId) || JobId.IsEmpty())
    {
        return VfxBuildError(TEXT("Missing required parameter: jobId"));
    }

    EGenStatus Status = EGenStatus::Failed;
    double Progress = 0.0;
    FString Message;
    FString AssetPath;
    FString ActorName;
    bool bFound = false;

    {
        FScopeLock Lock(&GenerationJobLock);
        TSharedPtr<FGenerationJob>* Found = GenerationJobs.Find(JobId);
        if (Found && Found->IsValid())
        {
            bFound = true;
            TSharedPtr<FGenerationJob> Job = *Found;
            Status = Job->Status;
            Progress = Job->Progress;
            Message = Job->Message;
            AssetPath = Job->AssetPath;
            ActorName = Job->ActorName;

            // Clean up terminal jobs after returning their final status.
            if (Status == EGenStatus::Completed || Status == EGenStatus::Failed)
            {
                GenerationJobs.Remove(JobId);
            }
        }
    }

    if (!bFound)
    {
        return VfxBuildError(FString::Printf(TEXT("Job not found: %s"), *JobId));
    }

    TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
    Result->SetStringField(TEXT("jobId"), JobId);

    FString StatusStr;
    switch (Status)
    {
        case EGenStatus::Pending: StatusStr = TEXT("pending"); break;
        case EGenStatus::Running: StatusStr = TEXT("running"); break;
        case EGenStatus::Completed: StatusStr = TEXT("completed"); break;
        case EGenStatus::Failed: StatusStr = TEXT("failed"); break;
    }
    Result->SetStringField(TEXT("status"), StatusStr);
    Result->SetNumberField(TEXT("progress"), Progress);
    Result->SetStringField(TEXT("message"), Message);

    if (Status == EGenStatus::Completed)
    {
        Result->SetStringField(TEXT("assetPath"), AssetPath);
        Result->SetStringField(TEXT("actorName"), ActorName);
    }
    else
    {
        Result->SetField(TEXT("assetPath"), MakeShareable(new FJsonValueNull()));
        Result->SetField(TEXT("actorName"), MakeShareable(new FJsonValueNull()));
    }

    return VfxBuildSuccess(Result);
}

// ---------------------------------------------------------------------------
// Handler: create_material_from_textures
// Delegates material assembly to Content/Python/import_generated_asset.py.
// ---------------------------------------------------------------------------
FString HandleCreateMaterialFromTextures(const TSharedPtr<FJsonObject>& Params)
{
    FString Path;
    if (!Params->TryGetStringField(TEXT("path"), Path) || Path.IsEmpty())
    {
        return VfxBuildError(TEXT("Missing required parameter: path"));
    }

    FString ParentPath;
    if (!Params->TryGetStringField(TEXT("parentPath"), ParentPath) || ParentPath.IsEmpty())
    {
        return VfxBuildError(TEXT("Missing required parameter: parentPath"));
    }

    if (!Params->HasField(TEXT("maps")))
    {
        return VfxBuildError(TEXT("Missing required parameter: maps"));
    }

    const TSharedPtr<FJsonObject>* MapsObjPtr = nullptr;
    if (!Params->TryGetObjectField(TEXT("maps"), MapsObjPtr) || !MapsObjPtr || !MapsObjPtr->IsValid())
    {
        return VfxBuildError(TEXT("maps must be an object"));
    }
    TSharedPtr<FJsonObject> MapsObj = *MapsObjPtr;

    const bool bReuse = Params->HasField(TEXT("reuse")) ? Params->GetBoolField(TEXT("reuse")) : true;

    TSharedPtr<FJsonObject> ScalarObj;
    if (Params->HasField(TEXT("scalarParameters")))
    {
        const TSharedPtr<FJsonObject>* ScalarObjPtr = nullptr;
        if (!Params->TryGetObjectField(TEXT("scalarParameters"), ScalarObjPtr) || !ScalarObjPtr || !ScalarObjPtr->IsValid())
        {
            return VfxBuildError(TEXT("scalarParameters must be an object"));
        }
        ScalarObj = *ScalarObjPtr;
    }

    TSharedPtr<FJsonObject> VectorObj;
    if (Params->HasField(TEXT("vectorParameters")))
    {
        const TSharedPtr<FJsonObject>* VectorObjPtr = nullptr;
        if (!Params->TryGetObjectField(TEXT("vectorParameters"), VectorObjPtr) || !VectorObjPtr || !VectorObjPtr->IsValid())
        {
            return VfxBuildError(TEXT("vectorParameters must be an object"));
        }
        VectorObj = *VectorObjPtr;
    }

    // Build a parameter file so we do not have to quote JSON through a shell.
    TSharedPtr<FJsonObject> ParamPayload = MakeShareable(new FJsonObject);
    ParamPayload->SetStringField(TEXT("name"), FPaths::GetBaseFilename(Path));
    ParamPayload->SetStringField(TEXT("parent_path"), ParentPath);
    ParamPayload->SetStringField(TEXT("destination"), FPaths::GetPath(Path));
    ParamPayload->SetObjectField(TEXT("maps"), MapsObj);
    if (ScalarObj.IsValid())
    {
        ParamPayload->SetObjectField(TEXT("scalar_parameters"), ScalarObj);
    }
    if (VectorObj.IsValid())
    {
        ParamPayload->SetObjectField(TEXT("vector_parameters"), VectorObj);
    }
    ParamPayload->SetBoolField(TEXT("reuse"), bReuse);

    FString ParamJson;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ParamJson);
    FJsonSerializer::Serialize(ParamPayload.ToSharedRef(), Writer);

    const FString ParamsFile = FPaths::ProjectSavedDir() / TEXT("Temp") / FString::Printf(TEXT("vfx_create_mat_params_%s.json"), *FGuid::NewGuid().ToString());
    const FString ResultFile = FPaths::ProjectSavedDir() / TEXT("Temp") / FString::Printf(TEXT("vfx_create_mat_result_%s.json"), *FGuid::NewGuid().ToString());

    struct FTempFileGuard
    {
        FString ParamsFile;
        FString ResultFile;
        ~FTempFileGuard()
        {
            IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
            PlatformFile.DeleteFile(*ParamsFile);
            PlatformFile.DeleteFile(*ResultFile);
        }
    };
    FTempFileGuard TempGuard;
    TempGuard.ParamsFile = ParamsFile;
    TempGuard.ResultFile = ResultFile;

    IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
    PlatformFile.CreateDirectoryTree(*FPaths::GetPath(ParamsFile));
    FFileHelper::SaveStringToFile(ParamJson, *ParamsFile, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);

    const FString ScriptPath = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / TEXT("Content/Python/import_generated_asset.py"));
    const FString Cmd = FString::Printf(TEXT("py \"%s\" create_material --params-file \"%s\" --result-file \"%s\""), *ScriptPath, *ParamsFile, *ResultFile);

    bool bExecSuccess = false;
    FEvent* DoneEvent = FPlatformProcess::GetSynchEventFromPool();
    AsyncTask(ENamedThreads::GameThread, [&]()
    {
        if (GEngine)
        {
            bExecSuccess = GEngine->Exec(GEditor ? GEditor->GetEditorWorldContext().World() : nullptr, *Cmd, *GLog);
        }
        DoneEvent->Trigger();
    });
    DoneEvent->Wait();
    FPlatformProcess::ReturnSynchEventToPool(DoneEvent);

    if (!bExecSuccess)
    {
        return VfxBuildError(TEXT("Failed to execute Python material creation script"));
    }

    FString ResultStr;
    if (!FFileHelper::LoadFileToString(ResultStr, *ResultFile))
    {
        return VfxBuildError(TEXT("Material creation result file not found"));
    }

    TSharedPtr<FJsonObject> ResultObj;
    TSharedRef<TJsonReader<>> Reader2 = TJsonReaderFactory<>::Create(ResultStr);
    if (!FJsonSerializer::Deserialize(Reader2, ResultObj) || !ResultObj.IsValid())
    {
        return VfxBuildError(TEXT("Failed to parse material creation result"));
    }

    bool bResultSuccess = false;
    if (ResultObj->TryGetBoolField(TEXT("success"), bResultSuccess) && !bResultSuccess)
    {
        FString ErrorMsg = ResultObj->HasField(TEXT("error")) ? ResultObj->GetStringField(TEXT("error")) : TEXT("Unknown Python error");
        return VfxBuildError(ErrorMsg);
    }

    TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
    FString ResultPath;
    if (ResultObj->TryGetStringField(TEXT("path"), ResultPath))
    {
        Result->SetStringField(TEXT("path"), ResultPath);
    }
    Result->SetStringField(TEXT("parentPath"), ParentPath);
    Result->SetObjectField(TEXT("maps"), MapsObj);
    bool bReused = false;
    ResultObj->TryGetBoolField(TEXT("reused"), bReused);
    Result->SetBoolField(TEXT("reused"), bReused);

    return VfxBuildSuccess(Result);
}

// ---------------------------------------------------------------------------
// Handler: set_texture_parameter
// ---------------------------------------------------------------------------
FString HandleSetTextureParameter(const TSharedPtr<FJsonObject>& Params)
{
    FString ActorName;
    if (!Params->TryGetStringField(TEXT("actorName"), ActorName) || ActorName.IsEmpty())
    {
        return VfxBuildError(TEXT("Missing required parameter: actorName"));
    }

    FString ParameterName;
    if (!Params->TryGetStringField(TEXT("parameterName"), ParameterName) || ParameterName.IsEmpty())
    {
        return VfxBuildError(TEXT("Missing required parameter: parameterName"));
    }

    FString TexturePath;
    if (!Params->TryGetStringField(TEXT("texturePath"), TexturePath) || TexturePath.IsEmpty())
    {
        return VfxBuildError(TEXT("Missing required parameter: texturePath"));
    }

    FString ComponentName;
    Params->TryGetStringField(TEXT("componentName"), ComponentName);
    const int32 SlotIndex = Params->HasField(TEXT("slotIndex")) ? FMath::RoundToInt(Params->GetNumberField(TEXT("slotIndex"))) : 0;

    FString ErrorMsg;
    FString ResultComponentName;
    FEvent* DoneEvent = FPlatformProcess::GetSynchEventFromPool();

    AsyncTask(ENamedThreads::GameThread, [&]()
    {
        UTexture* Texture = LoadObject<UTexture>(nullptr, *TexturePath);
        if (!Texture)
        {
            ErrorMsg = FString::Printf(TEXT("Texture not found: %s"), *TexturePath);
            DoneEvent->Trigger();
            return;
        }

        UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
        if (!World)
        {
            ErrorMsg = TEXT("No world available");
            DoneEvent->Trigger();
            return;
        }

        AActor* Actor = FindActorByName(World, ActorName);
        if (!Actor)
        {
            ErrorMsg = FString::Printf(TEXT("Actor not found: %s"), *ActorName);
            DoneEvent->Trigger();
            return;
        }

        UMeshComponent* MeshComp = nullptr;
        if (!ComponentName.IsEmpty())
        {
            TSet<UActorComponent*> Components = Actor->GetComponents();
            for (UActorComponent* Comp : Components)
            {
                if (Comp->GetName() == ComponentName)
                {
                    MeshComp = Cast<UMeshComponent>(Comp);
                    break;
                }
            }
        }
        else
        {
            MeshComp = Actor->FindComponentByClass<UMeshComponent>();
        }

        if (!MeshComp)
        {
            ErrorMsg = TEXT("No mesh component found on actor");
            DoneEvent->Trigger();
            return;
        }

        UMaterialInterface* MatInterface = MeshComp->GetMaterial(SlotIndex);
        if (!MatInterface)
        {
            ErrorMsg = TEXT("No material in slot");
            DoneEvent->Trigger();
            return;
        }

        UMaterialInstanceDynamic* MID = Cast<UMaterialInstanceDynamic>(MatInterface);
        if (!MID)
        {
            MID = MeshComp->CreateDynamicMaterialInstance(SlotIndex, MatInterface);
        }
        if (!MID)
        {
            ErrorMsg = TEXT("Cannot create dynamic material instance");
            DoneEvent->Trigger();
            return;
        }

        MID->SetTextureParameterValue(FName(*ParameterName), Texture);
        MeshComp->MarkRenderStateDirty();
        ResultComponentName = MeshComp->GetName();
        DoneEvent->Trigger();
    });

    DoneEvent->Wait();
    FPlatformProcess::ReturnSynchEventToPool(DoneEvent);

    if (!ErrorMsg.IsEmpty())
    {
        return VfxBuildError(ErrorMsg);
    }

    TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
    Result->SetStringField(TEXT("actorName"), ActorName);
    Result->SetStringField(TEXT("componentName"), ResultComponentName);
    Result->SetNumberField(TEXT("slotIndex"), SlotIndex);
    Result->SetStringField(TEXT("parameterName"), ParameterName);
    Result->SetStringField(TEXT("texturePath"), TexturePath);
    return VfxBuildSuccess(Result);
}

// ---------------------------------------------------------------------------
// Niagara helpers
// ---------------------------------------------------------------------------
namespace
{
    enum class ENiagaraInferredType
    {
        Float,
        Int,
        Bool,
        Vector3,
        Vector4,
        Unknown
    };

    ENiagaraInferredType InferNiagaraValueType(const TSharedPtr<FJsonValue>& Value, const FNiagaraVariable* ExistingVar = nullptr)
    {
        if (!Value.IsValid())
        {
            return ENiagaraInferredType::Unknown;
        }

        if (Value->Type == EJson::Boolean)
        {
            return ENiagaraInferredType::Bool;
        }

        if (Value->Type == EJson::Array)
        {
            const TArray<TSharedPtr<FJsonValue>>& Arr = Value->AsArray();
            if (Arr.Num() == 4)
            {
                return ENiagaraInferredType::Vector4;
            }
            if (Arr.Num() == 3)
            {
                return ENiagaraInferredType::Vector3;
            }
            return ENiagaraInferredType::Unknown;
        }

        if (Value->Type == EJson::Number)
        {
            if (ExistingVar)
            {
                if (ExistingVar->GetType() == FNiagaraTypeDefinition::GetIntDef())
                {
                    return ENiagaraInferredType::Int;
                }
                if (ExistingVar->GetType() == FNiagaraTypeDefinition::GetFloatDef())
                {
                    return ENiagaraInferredType::Float;
                }
            }
            return ENiagaraInferredType::Float;
        }

        return ENiagaraInferredType::Unknown;
    }

    FString NiagaraTypeToString(ENiagaraInferredType Type)
    {
        switch (Type)
        {
            case ENiagaraInferredType::Float:   return TEXT("Float");
            case ENiagaraInferredType::Int:     return TEXT("Int");
            case ENiagaraInferredType::Bool:    return TEXT("Bool");
            case ENiagaraInferredType::Vector3: return TEXT("Vector3");
            case ENiagaraInferredType::Vector4: return TEXT("LinearColor");
            default:                            return TEXT("Unknown");
        }
    }

    bool SetNiagaraSystemUserParameter(UNiagaraSystem* System, FName ParamName, const TSharedPtr<FJsonValue>& Value, FString& OutError, FString* OutTypeStr = nullptr)
    {
        if (!System)
        {
            OutError = TEXT("System is null");
            return false;
        }

        FNiagaraUserRedirectionParameterStore& Exposed = System->GetExposedParameters();
        const FNiagaraVariable* ExistingVar = nullptr;
        TArray<FNiagaraVariable> Parameters;
        Exposed.GetParameters(Parameters);
        for (const FNiagaraVariable& Var : Parameters)
        {
            if (Var.GetName() == ParamName)
            {
                ExistingVar = &Var;
                break;
            }
        }

        ENiagaraInferredType Type = InferNiagaraValueType(Value, ExistingVar);
        if (Type == ENiagaraInferredType::Unknown)
        {
            OutError = TEXT("Unsupported value type for Niagara parameter");
            return false;
        }

        FNiagaraVariable Variable = ExistingVar ? *ExistingVar : FNiagaraVariable();
        if (!ExistingVar)
        {
            switch (Type)
            {
                case ENiagaraInferredType::Float:   Variable = FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), ParamName); break;
                case ENiagaraInferredType::Int:     Variable = FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), ParamName); break;
                case ENiagaraInferredType::Bool:    Variable = FNiagaraVariable(FNiagaraTypeDefinition::GetBoolDef(), ParamName); break;
                case ENiagaraInferredType::Vector3: Variable = FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), ParamName); break;
                case ENiagaraInferredType::Vector4: Variable = FNiagaraVariable(FNiagaraTypeDefinition::GetColorDef(), ParamName); break;
                default: break;
            }
        }

        bool bSet = false;
        const TArray<TSharedPtr<FJsonValue>>& Arr = Value->AsArray();
        switch (Type)
        {
            case ENiagaraInferredType::Float:
                bSet = Exposed.SetParameterValue((float)Value->AsNumber(), Variable, true);
                break;
            case ENiagaraInferredType::Int:
                bSet = Exposed.SetParameterValue((int32)Value->AsNumber(), Variable, true);
                break;
            case ENiagaraInferredType::Bool:
                bSet = Exposed.SetParameterValue(Value->AsBool(), Variable, true);
                break;
            case ENiagaraInferredType::Vector3:
                bSet = Exposed.SetParameterValue(FVector(Arr[0]->AsNumber(), Arr[1]->AsNumber(), Arr[2]->AsNumber()), Variable, true);
                break;
            case ENiagaraInferredType::Vector4:
                bSet = Exposed.SetParameterValue(FLinearColor(Arr[0]->AsNumber(), Arr[1]->AsNumber(), Arr[2]->AsNumber(), Arr[3]->AsNumber()), Variable, true);
                break;
            default:
                break;
        }

        // Whole-number JSON values may actually be intended as integers.
        if (!bSet && Type == ENiagaraInferredType::Float)
        {
            FNiagaraVariable IntVar(FNiagaraTypeDefinition::GetIntDef(), Variable.GetName());
            bSet = Exposed.SetParameterValue((int32)Value->AsNumber(), IntVar, true);
            if (bSet)
            {
                Type = ENiagaraInferredType::Int;
            }
        }

        if (OutTypeStr)
        {
            *OutTypeStr = NiagaraTypeToString(Type);
        }

        if (!bSet)
        {
            OutError = FString::Printf(TEXT("Failed to set Niagara parameter %s (type mismatch?)"), *ParamName.ToString());
            return false;
        }
        return true;
    }

    bool SetNiagaraComponentVariable(UNiagaraComponent* Comp, FName ParamName, const TSharedPtr<FJsonValue>& Value, FString& OutTypeStr, FString& OutError)
    {
        if (!Comp)
        {
            OutError = TEXT("Niagara component is null");
            return false;
        }

        UNiagaraSystem* System = Comp->GetAsset();
        const FNiagaraVariable* ExistingVar = nullptr;
        if (System)
        {
            FNiagaraUserRedirectionParameterStore& Exposed = System->GetExposedParameters();
            TArray<FNiagaraVariable> Parameters;
            Exposed.GetParameters(Parameters);
            for (const FNiagaraVariable& Var : Parameters)
            {
                if (Var.GetName() == ParamName)
                {
                    ExistingVar = &Var;
                    break;
                }
            }
        }

        const ENiagaraInferredType Type = InferNiagaraValueType(Value, ExistingVar);
        if (Type == ENiagaraInferredType::Unknown)
        {
            OutError = TEXT("Unsupported value type for Niagara parameter");
            return false;
        }

        const TArray<TSharedPtr<FJsonValue>>& Arr = Value->AsArray();
        switch (Type)
        {
            case ENiagaraInferredType::Float:
                Comp->SetVariableFloat(ParamName, (float)Value->AsNumber());
                OutTypeStr = TEXT("Float");
                break;
            case ENiagaraInferredType::Int:
                Comp->SetVariableInt(ParamName, (int32)Value->AsNumber());
                OutTypeStr = TEXT("Int");
                break;
            case ENiagaraInferredType::Bool:
                Comp->SetVariableBool(ParamName, Value->AsBool());
                OutTypeStr = TEXT("Bool");
                break;
            case ENiagaraInferredType::Vector3:
                Comp->SetVariableVec3(ParamName, FVector(Arr[0]->AsNumber(), Arr[1]->AsNumber(), Arr[2]->AsNumber()));
                OutTypeStr = TEXT("Vector3");
                break;
            case ENiagaraInferredType::Vector4:
                Comp->SetVariableLinearColor(ParamName, FLinearColor(Arr[0]->AsNumber(), Arr[1]->AsNumber(), Arr[2]->AsNumber(), Arr[3]->AsNumber()));
                OutTypeStr = TEXT("LinearColor");
                break;
            default:
                break;
        }
        return true;
    }
}

// ---------------------------------------------------------------------------
// Handler: duplicate_niagara_system
// ---------------------------------------------------------------------------
FString HandleDuplicateNiagaraSystem(const TSharedPtr<FJsonObject>& Params)
{
    FString TemplatePath;
    if (!Params->TryGetStringField(TEXT("templatePath"), TemplatePath) || TemplatePath.IsEmpty())
    {
        return VfxBuildError(TEXT("Missing required parameter: templatePath"));
    }

    FString NewPath;
    if (!Params->TryGetStringField(TEXT("newPath"), NewPath) || NewPath.IsEmpty())
    {
        return VfxBuildError(TEXT("Missing required parameter: newPath"));
    }

    const bool bSpawnActor = Params->HasField(TEXT("spawnActor")) ? Params->GetBoolField(TEXT("spawnActor")) : false;
    FString ActorName;
    Params->TryGetStringField(TEXT("actorName"), ActorName);
    const FVector Location = ParseVector3(Params, TEXT("location"), FVector::ZeroVector);
    const FVector RotationVec = ParseVector3(Params, TEXT("rotation"), FVector::ZeroVector);
    const FRotator Rotation(RotationVec.X, RotationVec.Y, RotationVec.Z);

    TSharedPtr<FJsonObject> InitialParams = Params->HasField(TEXT("initialParameters")) ? Params->GetObjectField(TEXT("initialParameters")) : nullptr;

    FString ErrorMsg;
    FString ResultNewPath;
    FString ResultActorName;
    TArray<TSharedPtr<FJsonValue>> ParametersSet;
    FEvent* DoneEvent = FPlatformProcess::GetSynchEventFromPool();

    AsyncTask(ENamedThreads::GameThread, [&]()
    {
        UNiagaraSystem* TemplateSystem = LoadObject<UNiagaraSystem>(nullptr, *TemplatePath);
        if (!TemplateSystem)
        {
            ErrorMsg = FString::Printf(TEXT("Niagara system not found: %s"), *TemplatePath);
            DoneEvent->Trigger();
            return;
        }

        if (!UEditorAssetLibrary::DuplicateAsset(TemplatePath, NewPath))
        {
            ErrorMsg = FString::Printf(TEXT("Failed to duplicate Niagara system to: %s"), *NewPath);
            DoneEvent->Trigger();
            return;
        }

        UNiagaraSystem* NewSystem = LoadObject<UNiagaraSystem>(nullptr, *NewPath);
        if (!NewSystem)
        {
            ErrorMsg = FString::Printf(TEXT("Failed to load duplicated Niagara system: %s"), *NewPath);
            DoneEvent->Trigger();
            return;
        }
        ResultNewPath = NewSystem->GetPathName();

        if (InitialParams.IsValid())
        {
            for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : InitialParams->Values)
            {
                FString Err;
                if (SetNiagaraSystemUserParameter(NewSystem, FName(*Pair.Key), Pair.Value, Err))
                {
                    ParametersSet.Add(MakeShareable(new FJsonValueString(Pair.Key)));
                }
                else
                {
                    UE_LOG(LogMCPVfxCommands, Warning, TEXT("Failed to set initial parameter %s: %s"), *Pair.Key, *Err);
                }
            }
        }

        if (UPackage* Package = NewSystem->GetOutermost())
        {
            Package->MarkPackageDirty();
            FString PackageFile = FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension());
            FSavePackageArgs SaveArgs;
            SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
            UPackage::SavePackage(Package, NewSystem, *PackageFile, SaveArgs);
        }

        if (bSpawnActor)
        {
            UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
            if (!World)
            {
                ErrorMsg = TEXT("No world available for Niagara actor spawn");
                DoneEvent->Trigger();
                return;
            }

            FString SpawnName = ActorName.IsEmpty()
                ? FString::Printf(TEXT("%s_Actor"), *FPaths::GetBaseFilename(NewPath))
                : ActorName;
            FActorSpawnParameters SpawnParams;
            SpawnParams.Name = FName(*SpawnName);
            SpawnParams.NameMode = FActorSpawnParameters::ESpawnActorNameMode::Requested;

            ANiagaraActor* Actor = World->SpawnActor<ANiagaraActor>(ANiagaraActor::StaticClass(), Location, Rotation, SpawnParams);
            if (!Actor)
            {
                ErrorMsg = TEXT("Failed to spawn Niagara actor");
                DoneEvent->Trigger();
                return;
            }

            if (UNiagaraComponent* NiagaraComp = Actor->GetNiagaraComponent())
            {
                NiagaraComp->SetAsset(NewSystem);
                if (InitialParams.IsValid())
                {
                    for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : InitialParams->Values)
                    {
                        FString TypeStr;
                        FString Err;
                        SetNiagaraComponentVariable(NiagaraComp, FName(*Pair.Key), Pair.Value, TypeStr, Err);
                    }
                }
                NiagaraComp->Activate();
            }
            ResultActorName = Actor->GetName();
        }

        DoneEvent->Trigger();
    });

    DoneEvent->Wait();
    FPlatformProcess::ReturnSynchEventToPool(DoneEvent);

    if (!ErrorMsg.IsEmpty())
    {
        return VfxBuildError(ErrorMsg);
    }

    TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
    Result->SetStringField(TEXT("templatePath"), TemplatePath);
    Result->SetStringField(TEXT("newPath"), ResultNewPath);
    Result->SetStringField(TEXT("actorName"), ResultActorName);
    Result->SetArrayField(TEXT("location"), VectorToJsonArray(Location)->AsArray());
    Result->SetArrayField(TEXT("parametersSet"), ParametersSet);
    return VfxBuildSuccess(Result);
}

// ---------------------------------------------------------------------------
// Handler: set_niagara_parameter
// ---------------------------------------------------------------------------
FString HandleSetNiagaraParameter(const TSharedPtr<FJsonObject>& Params)
{
    FString ParameterName;
    if (!Params->TryGetStringField(TEXT("parameterName"), ParameterName) || ParameterName.IsEmpty())
    {
        return VfxBuildError(TEXT("Missing required parameter: parameterName"));
    }

    if (!Params->HasField(TEXT("value")))
    {
        return VfxBuildError(TEXT("Missing required parameter: value"));
    }

    TSharedPtr<FJsonValue> Value = Params->TryGetField(TEXT("value"));
    if (!Value.IsValid())
    {
        return VfxBuildError(TEXT("value must be a valid JSON value"));
    }

    FString ActorName;
    const bool bHasActor = Params->TryGetStringField(TEXT("actorName"), ActorName) && !ActorName.IsEmpty();
    FString SystemPath;
    const bool bHasSystem = Params->TryGetStringField(TEXT("systemPath"), SystemPath) && !SystemPath.IsEmpty();
    FString ComponentName;
    Params->TryGetStringField(TEXT("componentName"), ComponentName);

    if (!bHasActor && !bHasSystem)
    {
        return VfxBuildError(TEXT("Either actorName or systemPath is required"));
    }

    FString ErrorMsg;
    FString ValueTypeStr;
    FEvent* DoneEvent = FPlatformProcess::GetSynchEventFromPool();

    AsyncTask(ENamedThreads::GameThread, [&]()
    {
        if (bHasActor)
        {
            UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
            if (!World)
            {
                ErrorMsg = TEXT("No world available");
                DoneEvent->Trigger();
                return;
            }

            AActor* Actor = FindActorByName(World, ActorName);
            if (!Actor)
            {
                ErrorMsg = FString::Printf(TEXT("Actor not found: %s"), *ActorName);
                DoneEvent->Trigger();
                return;
            }

            UNiagaraComponent* NiagaraComp = nullptr;
            if (!ComponentName.IsEmpty())
            {
                TSet<UActorComponent*> Components = Actor->GetComponents();
                for (UActorComponent* Comp : Components)
                {
                    if (Comp->GetName() == ComponentName)
                    {
                        NiagaraComp = Cast<UNiagaraComponent>(Comp);
                        break;
                    }
                }
            }
            else
            {
                NiagaraComp = Actor->FindComponentByClass<UNiagaraComponent>();
            }

            if (!NiagaraComp)
            {
                ErrorMsg = TEXT("No Niagara component found on actor");
                DoneEvent->Trigger();
                return;
            }

            if (!SetNiagaraComponentVariable(NiagaraComp, FName(*ParameterName), Value, ValueTypeStr, ErrorMsg))
            {
                DoneEvent->Trigger();
                return;
            }
        }
        else
        {
            UNiagaraSystem* System = LoadObject<UNiagaraSystem>(nullptr, *SystemPath);
            if (!System)
            {
                ErrorMsg = FString::Printf(TEXT("Niagara system not found: %s"), *SystemPath);
                DoneEvent->Trigger();
                return;
            }

            if (!SetNiagaraSystemUserParameter(System, FName(*ParameterName), Value, ErrorMsg, &ValueTypeStr))
            {
                DoneEvent->Trigger();
                return;
            }

            if (UPackage* Package = System->GetOutermost())
            {
                Package->MarkPackageDirty();
                FString PackageFile = FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension());
                FSavePackageArgs SaveArgs;
                SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
                UPackage::SavePackage(Package, System, *PackageFile, SaveArgs);
            }
        }

        DoneEvent->Trigger();
    });

    DoneEvent->Wait();
    FPlatformProcess::ReturnSynchEventToPool(DoneEvent);

    if (!ErrorMsg.IsEmpty())
    {
        return VfxBuildError(ErrorMsg);
    }

    TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
    Result->SetStringField(TEXT("actorName"), ActorName);
    Result->SetStringField(TEXT("parameterName"), ParameterName);
    Result->SetStringField(TEXT("valueType"), ValueTypeStr);
    Result->SetField(TEXT("value"), Value);
    return VfxBuildSuccess(Result);
}

#endif // WITH_EDITOR
