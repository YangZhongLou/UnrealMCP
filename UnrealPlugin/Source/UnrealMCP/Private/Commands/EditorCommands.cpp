#include "CoreMinimal.h"
#include "Editor.h"
#include "Selection.h"
#include "EngineUtils.h"
#include "FileHelpers.h"
#include "Framework/Application/SlateApplication.h"
#include "InputCoreTypes.h"
#include "Components/LightComponent.h"
#include "LevelEditorViewport.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/PlatformFileManager.h"
#include "Engine/Engine.h"
#include "LogCaptureDevice.h"
#include "HAL/IConsoleManager.h"
#include "Framework/Docking/TabManager.h"
#include "LevelEditorSubsystem.h"
#include "Async/Async.h"
#include "Misc/PackageName.h"
#include "HAL/FileManager.h"

FString HandleRunConsoleCommand(const TSharedPtr<FJsonObject>& Params)
{
    FString Command = Params->GetStringField(TEXT("command"));

    FEvent* DoneEvent = FPlatformProcess::GetSynchEventFromPool();

    AsyncTask(ENamedThreads::GameThread, [&]()
    {
        UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
        if (World && World->GetGameInstance())
        {
            GEngine->Exec(World, *Command);
        }
        else if (World)
        {
            GEngine->Exec(World, *Command);
        }
        DoneEvent->Trigger();
    });

    DoneEvent->Wait();
    FPlatformProcess::ReturnSynchEventToPool(DoneEvent);

    return TEXT("{\"success\":true,\"result\":{\"executed\":true}}");
}

FString HandleSaveCurrentLevel(const TSharedPtr<FJsonObject>& Params)
{
    if (!GEditor)
    {
        return TEXT("{\"success\":false,\"error\":\"Editor not available\"}");
    }

    bool bSaved = false;
    FEvent* DoneEvent = FPlatformProcess::GetSynchEventFromPool();

    AsyncTask(ENamedThreads::GameThread, [&]()
    {
        UWorld* World = GEditor->GetEditorWorldContext().World();
        if (World)
        {
            FEditorFileUtils::SaveLevel(World->GetCurrentLevel());
            bSaved = true;
        }
        DoneEvent->Trigger();
    });

    DoneEvent->Wait();
    FPlatformProcess::ReturnSynchEventToPool(DoneEvent);

    if (bSaved)
    {
        return TEXT("{\"success\":true,\"result\":{\"saved\":true}}");
    }
    return TEXT("{\"success\":false,\"error\":\"Failed to save level\"}");
}

FString HandleCreateLevel(const TSharedPtr<FJsonObject>& Params)
{
    FString Path = Params->GetStringField(TEXT("path"));

    if (!GEditor)
    {
        return TEXT("{\"success\":false,\"error\":\"Editor not available\"}");
    }

    bool bCreated = false;
    FString LevelPath;
    FEvent* DoneEvent = FPlatformProcess::GetSynchEventFromPool();

    // Dispatch to GameThread
    AsyncTask(ENamedThreads::GameThread, [&]()
    {
        if (GEditor)
        {
            // Suppress all UI dialogs during level creation
            bool bPrevUnattended = GIsRunningUnattendedScript;
            GIsRunningUnattendedScript = true;

            ULevelEditorSubsystem* LevelEditor = GEditor->GetEditorSubsystem<ULevelEditorSubsystem>();
            if (LevelEditor && LevelEditor->NewLevel(Path))
            {
                UWorld* NewWorld = GEditor->GetEditorWorldContext().World();
                if (NewWorld)
                {
                    FEditorFileUtils::SaveLevel(NewWorld->GetCurrentLevel());
                    bCreated = true;
                    LevelPath = NewWorld->GetOutermost()->GetName();
                }
            }

            GIsRunningUnattendedScript = bPrevUnattended;
        }
        DoneEvent->Trigger();
    });

    DoneEvent->Wait();
    FPlatformProcess::ReturnSynchEventToPool(DoneEvent);

    if (bCreated)
    {
        TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
        Result->SetStringField(TEXT("path"), LevelPath);
        Result->SetBoolField(TEXT("created"), true);

        FString ResultStr;
        TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ResultStr);
        FJsonSerializer::Serialize(Result.ToSharedRef(), Writer);
        Writer->Close();

        return FString::Printf(TEXT("{\"success\":true,\"result\":%s}"), *ResultStr);
    }
    return TEXT("{\"success\":false,\"error\":\"Failed to create level\"}");
}

FString HandlePlayInEditor(const TSharedPtr<FJsonObject>& Params)
{
    bool bSuccess = false;
    FEvent* DoneEvent = FPlatformProcess::GetSynchEventFromPool();
    AsyncTask(ENamedThreads::GameThread, [&]()
    {
        if (GEditor) { GEditor->RequestPlaySession(FRequestPlaySessionParams()); bSuccess = true; }
        DoneEvent->Trigger();
    });
    DoneEvent->Wait();
    FPlatformProcess::ReturnSynchEventToPool(DoneEvent);
    if (!bSuccess) return TEXT("{\"success\":false,\"error\":\"Editor not available\"}");
    return TEXT("{\"success\":true,\"result\":{\"playing\":true}}");
}

FString HandleStopPlayInEditor(const TSharedPtr<FJsonObject>& Params)
{
    bool bSuccess = false;
    FEvent* DoneEvent = FPlatformProcess::GetSynchEventFromPool();
    AsyncTask(ENamedThreads::GameThread, [&]()
    {
        if (GEditor) { GEditor->EndPlayMap(); bSuccess = true; }
        DoneEvent->Trigger();
    });
    DoneEvent->Wait();
    FPlatformProcess::ReturnSynchEventToPool(DoneEvent);
    if (!bSuccess) return TEXT("{\"success\":false,\"error\":\"Editor not available\"}");
    return TEXT("{\"success\":true,\"result\":{\"stopped\":true}}");
}

FString HandleTakeScreenshot(const TSharedPtr<FJsonObject>& Params)
{
    FString Filename = Params->HasField(TEXT("filename"))
        ? Params->GetStringField(TEXT("filename"))
        : TEXT("screenshot");

    FEvent* DoneEvent = FPlatformProcess::GetSynchEventFromPool();
    AsyncTask(ENamedThreads::GameThread, [&]()
    {
        FScreenshotRequest::RequestScreenshot(Filename, false, false);
        DoneEvent->Trigger();
    });
    DoneEvent->Wait();
    FPlatformProcess::ReturnSynchEventToPool(DoneEvent);

    TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
    Result->SetStringField(TEXT("path"), FString::Printf(TEXT("%s/Saved/Screenshots/%s.png"), *FPaths::ProjectDir(), *Filename));

    FString ResultStr;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ResultStr);
    FJsonSerializer::Serialize(Result.ToSharedRef(), Writer);

    return FString::Printf(TEXT("{\"success\":true,\"result\":%s}"), *ResultStr);
}

FString HandleGenerateCppClass(const TSharedPtr<FJsonObject>& Params)
{
    FString ClassName = Params->GetStringField(TEXT("className"));
    FString ParentClass = Params->GetStringField(TEXT("parentClass"));

    FString ModuleName = Params->HasField(TEXT("module"))
        ? Params->GetStringField(TEXT("module"))
        : FApp::GetProjectName();

    FString ProjectDir = FPaths::ProjectDir();
    FString SourceDir = FPaths::Combine(ProjectDir, TEXT("Source"), ModuleName);
    FString ClassDir = FPaths::Combine(SourceDir, ClassName);

    IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
    if (!PlatformFile.DirectoryExists(*ClassDir))
    {
        PlatformFile.CreateDirectoryTree(*ClassDir);
    }

    FString HeaderContent = FString::Printf(TEXT(
        "#pragma once\n\n"
        "#include \"CoreMinimal.h\"\n"
        "#include \"%s.h\"\n"
        "#include \"%s.generated.h\"\n\n"
        "UCLASS()\n"
        "class %s_API U%s : public %s\n"
        "{\n"
        "\tGENERATED_BODY()\n\n"
        "public:\n"
        "\tU%s();\n\n"
        "protected:\n"
        "\tvirtual void BeginPlay() override;\n\n"
        "public:\n"
        "\tvirtual void Tick(float DeltaTime) override;\n"
        "};\n"),
        *ParentClass, *ClassName,
        *ModuleName.ToUpper(), *ClassName, *ParentClass,
        *ClassName);

    FString CppContent = FString::Printf(TEXT(
        "#include \"%s.h\"\n\n"
        "U%s::U%s()\n"
        "{\n"
        "\tPrimaryComponentTick.bCanEverTick = true;\n"
        "}\n\n"
        "void U%s::BeginPlay()\n"
        "{\n"
        "\tSuper::BeginPlay();\n"
        "}\n\n"
        "void U%s::Tick(float DeltaTime)\n"
        "{\n"
        "\tSuper::Tick(DeltaTime);\n"
        "}\n"),
        *ClassName,
        *ClassName, *ClassName,
        *ClassName,
        *ClassName);

    FString HeaderPath = FPaths::Combine(ClassDir, ClassName + TEXT(".h"));
    FString CppPath = FPaths::Combine(ClassDir, ClassName + TEXT(".cpp"));

    if (!FFileHelper::SaveStringToFile(HeaderContent, *HeaderPath))
    {
        return FString::Printf(TEXT("{\"success\":false,\"error\":\"Failed to write header: %s\"}"), *HeaderPath);
    }

    if (!FFileHelper::SaveStringToFile(CppContent, *CppPath))
    {
        return FString::Printf(TEXT("{\"success\":false,\"error\":\"Failed to write source: %s\"}"), *CppPath);
    }

    TSharedPtr<FJsonObject> ResultObj = MakeShareable(new FJsonObject);
    ResultObj->SetStringField(TEXT("path"), ClassDir);
    ResultObj->SetStringField(TEXT("header"), HeaderPath);
    ResultObj->SetStringField(TEXT("source"), CppPath);

    FString ResultStr;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ResultStr);
    FJsonSerializer::Serialize(ResultObj.ToSharedRef(), Writer);

    return FString::Printf(TEXT("{\"success\":true,\"result\":%s}"), *ResultStr);
}

FString HandleGetCurrentLevel(const TSharedPtr<FJsonObject>& Params)
{
    if (!GEditor)
    {
        return TEXT("{\"success\":false,\"error\":\"Editor not available\"}");
    }

    UWorld* World = GEditor->GetEditorWorldContext().World();
    if (!World)
    {
        return TEXT("{\"success\":false,\"error\":\"No world available\"}");
    }

    FString LevelName = World->GetMapName();
    FString LevelPath = World->GetOutermost()->GetName();

    int32 ActorCount = 0;
    FEvent* DoneEvent = FPlatformProcess::GetSynchEventFromPool();

    AsyncTask(ENamedThreads::GameThread, [&]()
    {
        for (TActorIterator<AActor> It(World); It; ++It)
        {
            ActorCount++;
        }
        DoneEvent->Trigger();
    });

    DoneEvent->Wait();
    FPlatformProcess::ReturnSynchEventToPool(DoneEvent);

    TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
    Result->SetStringField(TEXT("name"), LevelName);
    Result->SetStringField(TEXT("path"), LevelPath);
    Result->SetNumberField(TEXT("actor_count"), ActorCount);

    FString ResultStr;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ResultStr);
    FJsonSerializer::Serialize(Result.ToSharedRef(), Writer);

    return FString::Printf(TEXT("{\"success\":true,\"result\":%s}"), *ResultStr);
}

FString HandleFocusViewport(const TSharedPtr<FJsonObject>& Params)
{
    if (!GEditor)
    {
        return TEXT("{\"success\":false,\"error\":\"Editor not available\"}");
    }

    UWorld* World = GEditor->GetEditorWorldContext().World();
    if (!World)
    {
        return TEXT("{\"success\":false,\"error\":\"No world available\"}");
    }

    // Pre-parse params outside GameThread
    bool bUseActor = Params->HasField(TEXT("actorName"));
    FString ActorName = bUseActor ? Params->GetStringField(TEXT("actorName")) : TEXT("");
    FVector TargetLocation = FVector::ZeroVector;
    if (!bUseActor && Params->HasField(TEXT("location")))
    {
        const TArray<TSharedPtr<FJsonValue>>& Arr = Params->GetArrayField(TEXT("location"));
        if (Arr.Num() >= 3)
            TargetLocation = FVector(Arr[0]->AsNumber(), Arr[1]->AsNumber(), Arr[2]->AsNumber());
    }
    else if (!bUseActor)
    {
        return TEXT("{\"success\":false,\"error\":\"Provide actorName or location\"}");
    }

    FString ErrorMsg;
    FEvent* DoneEvent = FPlatformProcess::GetSynchEventFromPool();

    AsyncTask(ENamedThreads::GameThread, [&]()
    {
        if (bUseActor)
        {
            AActor* Actor = nullptr;
            for (TActorIterator<AActor> It(World); It; ++It)
            {
                if (It->GetName() == ActorName) { Actor = *It; break; }
            }
            if (!Actor) { ErrorMsg = FString::Printf(TEXT("Actor not found: %s"), *ActorName); DoneEvent->Trigger(); return; }
            TargetLocation = Actor->GetActorLocation();
        }

        if (GCurrentLevelEditingViewportClient)
        {
            GCurrentLevelEditingViewportClient->SetViewLocation(TargetLocation);
            GCurrentLevelEditingViewportClient->Invalidate();
        }

        DoneEvent->Trigger();
    });

    DoneEvent->Wait();
    FPlatformProcess::ReturnSynchEventToPool(DoneEvent);

    if (!ErrorMsg.IsEmpty())
        return FString::Printf(TEXT("{\"success\":false,\"error\":\"%s\"}"), *ErrorMsg);

    TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
    Result->SetStringField(TEXT("focused_at"), FString::Printf(TEXT("[%.0f, %.0f, %.0f]"),
        TargetLocation.X, TargetLocation.Y, TargetLocation.Z));

    FString ResultStr;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ResultStr);
    FJsonSerializer::Serialize(Result.ToSharedRef(), Writer);

    return FString::Printf(TEXT("{\"success\":true,\"result\":%s}"), *ResultStr);
}

FString HandleGetSelectedActors(const TSharedPtr<FJsonObject>& Params)
{
    if (!GEditor)
    {
        return TEXT("{\"success\":false,\"error\":\"Editor not available\"}");
    }

    TArray<TSharedPtr<FJsonValue>> Actors;
    USelection* Selection = GEditor->GetSelectedActors();
    if (Selection)
    {
        for (int32 i = 0; i < Selection->Num(); i++)
        {
            if (AActor* Actor = Cast<AActor>(Selection->GetSelectedObject(i)))
            {
                TSharedPtr<FJsonObject> Obj = MakeShareable(new FJsonObject);
                Obj->SetStringField(TEXT("name"), Actor->GetName());
                Obj->SetStringField(TEXT("class"), Actor->GetClass()->GetName());
                Actors.Add(MakeShareable(new FJsonValueObject(Obj)));
            }
        }
    }

    TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
    Result->SetArrayField(TEXT("actors"), Actors);
    Result->SetNumberField(TEXT("count"), Actors.Num());

    FString ResultStr;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ResultStr);
    FJsonSerializer::Serialize(Result.ToSharedRef(), Writer);

    return FString::Printf(TEXT("{\"success\":true,\"result\":%s}"), *ResultStr);
}

FString HandleSelectActor(const TSharedPtr<FJsonObject>& Params)
{
    FString ActorName = Params->GetStringField(TEXT("actorName"));
    bool bAddToSelection = Params->HasField(TEXT("addToSelection"))
        ? Params->GetBoolField(TEXT("addToSelection"))
        : false;

    if (!GEditor)
    {
        return TEXT("{\"success\":false,\"error\":\"Editor not available\"}");
    }

    FString ErrorMsg;
    FEvent* DoneEvent = FPlatformProcess::GetSynchEventFromPool();

    AsyncTask(ENamedThreads::GameThread, [&]()
    {
        UWorld* World = GEditor->GetEditorWorldContext().World();
        if (!World) { ErrorMsg = TEXT("No world available"); DoneEvent->Trigger(); return; }

        AActor* TargetActor = nullptr;
        for (TActorIterator<AActor> It(World); It; ++It)
        {
            if (It->GetName() == ActorName) { TargetActor = *It; break; }
        }

        if (!TargetActor) { ErrorMsg = FString::Printf(TEXT("Actor not found: %s"), *ActorName); DoneEvent->Trigger(); return; }

        if (!bAddToSelection)
            GEditor->SelectNone(false, true);
        GEditor->SelectActor(TargetActor, true, true, true);
        DoneEvent->Trigger();
    });

    DoneEvent->Wait();
    FPlatformProcess::ReturnSynchEventToPool(DoneEvent);

    if (!ErrorMsg.IsEmpty())
        return FString::Printf(TEXT("{\"success\":false,\"error\":\"%s\"}"), *ErrorMsg);

    return FString::Printf(TEXT("{\"success\":true,\"result\":{\"selected\":\"%s\"}}"), *ActorName);
}

FString HandleSimulateKey(const TSharedPtr<FJsonObject>& Params)
{
    FString KeyName = Params->GetStringField(TEXT("key"));
    FString Action = Params->HasField(TEXT("action"))
        ? Params->GetStringField(TEXT("action"))
        : TEXT("tap");

    FKey Key(*KeyName);
    if (!Key.IsValid())
    {
        return FString::Printf(TEXT("{\"success\":false,\"error\":\"Invalid key: %s\"}"), *KeyName);
    }

    FModifierKeysState ModifierKeys;
    int32 UserIndex = 0;
    uint32 KeyCode = 0;
    uint32 CharCode = 0;

    bool bShouldPress = Action.Equals(TEXT("press"), ESearchCase::IgnoreCase) ||
                        Action.Equals(TEXT("tap"), ESearchCase::IgnoreCase);
    bool bShouldRelease = Action.Equals(TEXT("release"), ESearchCase::IgnoreCase) ||
                          Action.Equals(TEXT("tap"), ESearchCase::IgnoreCase);

    FEvent* DoneEvent = FPlatformProcess::GetSynchEventFromPool();

    AsyncTask(ENamedThreads::GameThread, [&]()
    {
        if (bShouldPress)
        {
            FKeyEvent PressEvent(Key, ModifierKeys, UserIndex, false, KeyCode, CharCode);
            FSlateApplication::Get().ProcessKeyDownEvent(PressEvent);
        }
        if (bShouldRelease)
        {
            FKeyEvent ReleaseEvent(Key, ModifierKeys, UserIndex, true, KeyCode, CharCode);
            FSlateApplication::Get().ProcessKeyUpEvent(ReleaseEvent);
        }
        DoneEvent->Trigger();
    });

    DoneEvent->Wait();
    FPlatformProcess::ReturnSynchEventToPool(DoneEvent);

    return FString::Printf(TEXT("{\"success\":true,\"result\":{\"key\":\"%s\",\"action\":\"%s\"}}"), *KeyName, *Action);
}

FString HandleGetViewportCamera(const TSharedPtr<FJsonObject>& Params)
{
    FString ErrorMsg;
    TSharedPtr<FJsonObject> ResponseJson;
    FEvent* DoneEvent = FPlatformProcess::GetSynchEventFromPool();

    AsyncTask(ENamedThreads::GameThread, [&]()
    {
        if (!GCurrentLevelEditingViewportClient)
        {
            ErrorMsg = TEXT("No viewport camera available");
            DoneEvent->Trigger();
            return;
        }

        FVector Location = GCurrentLevelEditingViewportClient->GetViewLocation();
        FRotator Rotation = GCurrentLevelEditingViewportClient->GetViewRotation();

        TArray<TSharedPtr<FJsonValue>> LocArr;
        LocArr.Add(MakeShareable(new FJsonValueNumber(Location.X)));
        LocArr.Add(MakeShareable(new FJsonValueNumber(Location.Y)));
        LocArr.Add(MakeShareable(new FJsonValueNumber(Location.Z)));

        TArray<TSharedPtr<FJsonValue>> RotArr;
        RotArr.Add(MakeShareable(new FJsonValueNumber(Rotation.Pitch)));
        RotArr.Add(MakeShareable(new FJsonValueNumber(Rotation.Yaw)));
        RotArr.Add(MakeShareable(new FJsonValueNumber(Rotation.Roll)));

        TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
        Result->SetArrayField(TEXT("location"), LocArr);
        Result->SetArrayField(TEXT("rotation"), RotArr);

        ResponseJson = MakeShareable(new FJsonObject);
        ResponseJson->SetBoolField(TEXT("success"), true);
        ResponseJson->SetObjectField(TEXT("result"), Result);

        DoneEvent->Trigger();
    });

    DoneEvent->Wait();
    FPlatformProcess::ReturnSynchEventToPool(DoneEvent);

    if (!ErrorMsg.IsEmpty())
        return FString::Printf(TEXT("{\"success\":false,\"error\":\"%s\"}"), *ErrorMsg);

    FString Out;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Out);
    FJsonSerializer::Serialize(ResponseJson.ToSharedRef(), Writer);
    return Out;
}

FString HandleSetLightParameters(const TSharedPtr<FJsonObject>& Params)
{
    FString ActorName = Params->GetStringField(TEXT("actorName"));

    // Pre-extract params outside GameThread
    double bHasIntensity = Params->HasField(TEXT("intensity"));
    double Intensity = bHasIntensity ? Params->GetNumberField(TEXT("intensity")) : 0;
    bool bHasCastShadows = Params->HasField(TEXT("castShadows"));
    bool bCastShadows = bHasCastShadows ? Params->GetBoolField(TEXT("castShadows")) : false;
    FColor LightColor(FColor::White);
    if (Params->HasField(TEXT("color")))
    {
        const TArray<TSharedPtr<FJsonValue>>& Arr = Params->GetArrayField(TEXT("color"));
        if (Arr.Num() >= 3)
            LightColor = FColor(FMath::RoundToInt(Arr[0]->AsNumber() * 255), FMath::RoundToInt(Arr[1]->AsNumber() * 255), FMath::RoundToInt(Arr[2]->AsNumber() * 255));
    }

    FString ErrorMsg;
    FEvent* DoneEvent = FPlatformProcess::GetSynchEventFromPool();

    AsyncTask(ENamedThreads::GameThread, [&]()
    {
        UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
        if (!World) { ErrorMsg = TEXT("No world available"); DoneEvent->Trigger(); return; }

        AActor* Actor = nullptr;
        for (TActorIterator<AActor> It(World); It; ++It)
        {
            if (It->GetName() == ActorName) { Actor = *It; break; }
        }
        if (!Actor) { ErrorMsg = FString::Printf(TEXT("Actor not found: %s"), *ActorName); DoneEvent->Trigger(); return; }

        ULightComponent* LightComp = Actor->FindComponentByClass<ULightComponent>();
        if (!LightComp) { ErrorMsg = TEXT("No light component found on actor"); DoneEvent->Trigger(); return; }

        if (bHasIntensity) LightComp->SetIntensity(Intensity);
        LightComp->SetLightColor(LightColor, true);
        if (bHasCastShadows) LightComp->SetCastShadows(bCastShadows);
        DoneEvent->Trigger();
    });

    DoneEvent->Wait();
    FPlatformProcess::ReturnSynchEventToPool(DoneEvent);

    if (!ErrorMsg.IsEmpty())
        return FString::Printf(TEXT("{\"success\":false,\"error\":\"%s\"}"), *ErrorMsg);

    return FString::Printf(TEXT("{\"success\":true,\"result\":{\"actor\":\"%s\"}}"), *ActorName);
}

FString HandleSetViewMode(const TSharedPtr<FJsonObject>& Params)
{
    FString Mode = Params->GetStringField(TEXT("mode"));

    FEvent* DoneEvent = FPlatformProcess::GetSynchEventFromPool();
    AsyncTask(ENamedThreads::GameThread, [&]()
    {
        if (UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr)
            GEngine->Exec(World, *FString::Printf(TEXT("viewmode %s"), *Mode));
        DoneEvent->Trigger();
    });
    DoneEvent->Wait();
    FPlatformProcess::ReturnSynchEventToPool(DoneEvent);

    return FString::Printf(TEXT("{\"success\":true,\"result\":{\"view_mode\":\"%s\"}}"), *Mode);
}

FString HandleGetLogs(const TSharedPtr<FJsonObject>& Params)
{
    int32 Count = Params->HasField(TEXT("count"))
        ? FMath::Clamp(FMath::RoundToInt(Params->GetNumberField(TEXT("count"))), 1, 1000)
        : 100;

    FString MinVerbosity = Params->HasField(TEXT("verbosity"))
        ? Params->GetStringField(TEXT("verbosity"))
        : TEXT("Log");

    bool bClearAfter = Params->HasField(TEXT("clearAfter"))
        ? Params->GetBoolField(TEXT("clearAfter"))
        : false;

    TArray<FLogEntry> Logs;
    FLogCaptureDevice::Get().GetLogs(Count, MinVerbosity, Logs, bClearAfter);

    TArray<TSharedPtr<FJsonValue>> LogArray;
    for (const FLogEntry& Entry : Logs)
    {
        TSharedPtr<FJsonObject> LogEntryObj = MakeShareable(new FJsonObject);
        LogEntryObj->SetStringField(TEXT("timestamp"), Entry.Timestamp.ToString());
        LogEntryObj->SetStringField(TEXT("category"), Entry.Category);
        LogEntryObj->SetStringField(TEXT("verbosity"),
            Entry.Verbosity == ELogVerbosity::Error ? TEXT("Error") :
            Entry.Verbosity == ELogVerbosity::Warning ? TEXT("Warning") :
            Entry.Verbosity == ELogVerbosity::Log ? TEXT("Log") :
            Entry.Verbosity == ELogVerbosity::Verbose ? TEXT("Verbose") : TEXT("VeryVerbose"));
        LogEntryObj->SetStringField(TEXT("message"), Entry.Message);
        LogArray.Add(MakeShareable(new FJsonValueObject(LogEntryObj)));
    }

    TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
    Result->SetNumberField(TEXT("count"), LogArray.Num());
    Result->SetArrayField(TEXT("logs"), LogArray);
    Result->SetNumberField(TEXT("bufferSize"), FLogCaptureDevice::Get().GetBufferSize());

    FString ResultStr;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ResultStr);
    FJsonSerializer::Serialize(Result.ToSharedRef(), Writer);

    return FString::Printf(TEXT("{\"success\":true,\"result\":%s}"), *ResultStr);
}

FString HandleShowDebug(const TSharedPtr<FJsonObject>& Params)
{
    FString Flag = Params->GetStringField(TEXT("flag"));
    FString EnableStr;

    if (Params->HasField(TEXT("enable")))
    {
        bool bEnable = Params->GetBoolField(TEXT("enable"));
        EnableStr = bEnable ? TEXT(" 1") : TEXT(" 0");
    }

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    FString Cmd = FString::Printf(TEXT("show %s%s"), *Flag, *EnableStr);
    if (World)
    {
        GEngine->Exec(World, *Cmd);
    }

    return FString::Printf(TEXT("{\"success\":true,\"result\":{\"debug_flag\":\"%s\"}}"), *Flag);
}

FString HandleExecuteEditorCommand(const TSharedPtr<FJsonObject>& Params)
{
    FString Command = Params->GetStringField(TEXT("command"));

    if (!GEditor)
    {
        return TEXT("{\"success\":false,\"error\":\"Editor not available\"}");
    }

    FString FullCmd = FString::Printf(TEXT("editor.%s"), *Command);
    bool bExecuted = false;
    FEvent* DoneEvent = FPlatformProcess::GetSynchEventFromPool();

    AsyncTask(ENamedThreads::GameThread, [&]()
    {
        UWorld* World = GEditor->GetEditorWorldContext().World();
        if (World)
        {
            bExecuted = GEngine->Exec(World, *FullCmd) || GEngine->Exec(World, *Command);
        }
        else
        {
            bExecuted = GEngine->Exec(nullptr, *FullCmd) || GEngine->Exec(nullptr, *Command);
        }
        DoneEvent->Trigger();
    });

    DoneEvent->Wait();
    FPlatformProcess::ReturnSynchEventToPool(DoneEvent);

    if (bExecuted)
        return FString::Printf(TEXT("{\"success\":true,\"result\":{\"command\":\"%s\",\"executed\":true}}"), *Command);
    return FString::Printf(TEXT("{\"success\":false,\"error\":\"Command not recognized: %s\"}"), *Command);
}

FString HandleFocusEditorPanel(const TSharedPtr<FJsonObject>& Params)
{
    FString Panel = Params->GetStringField(TEXT("panel"));

    static TMap<FString, FName> PanelMap;
    if (PanelMap.Num() == 0)
    {
        PanelMap.Add(TEXT("ContentBrowser"), FName(TEXT("ContentBrowserTab1")));
        PanelMap.Add(TEXT("WorldOutliner"), FName(TEXT("WorldOutliner")));
        PanelMap.Add(TEXT("Details"), FName(TEXT("DetailsPanel")));
        PanelMap.Add(TEXT("OutputLog"), FName(TEXT("OutputLog")));
        PanelMap.Add(TEXT("Layers"), FName(TEXT("LayersPanel")));
        PanelMap.Add(TEXT("LevelEditor"), FName(TEXT("LevelEditor")));
        PanelMap.Add(TEXT("Viewport"), FName(TEXT("LevelEditor")));
    }

    FName* TabId = PanelMap.Find(Panel);
    if (!TabId)
    {
        TArray<FString> Known;
        PanelMap.GetKeys(Known);
        FString KnownStr = FString::Join(Known, TEXT(", "));
        return FString::Printf(TEXT("{\"success\":false,\"error\":\"Unknown panel: %s. Known panels: %s\"}"),
            *Panel, *KnownStr);
    }

    FEvent* DoneEvent = FPlatformProcess::GetSynchEventFromPool();
    AsyncTask(ENamedThreads::GameThread, [&, TabId]()
    {
        FGlobalTabmanager::Get()->TryInvokeTab(*TabId);
        DoneEvent->Trigger();
    });
    DoneEvent->Wait();
    FPlatformProcess::ReturnSynchEventToPool(DoneEvent);

    TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
    Result->SetStringField(TEXT("panel"), Panel);
    Result->SetBoolField(TEXT("focused"), true);

    FString ResultStr;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ResultStr);
    FJsonSerializer::Serialize(Result.ToSharedRef(), Writer);

    return FString::Printf(TEXT("{\"success\":true,\"result\":%s}"), *ResultStr);
}

FString HandleGetEditorCommands(const TSharedPtr<FJsonObject>& Params)
{
    FString Prefix = Params->HasField(TEXT("prefix"))
        ? Params->GetStringField(TEXT("prefix"))
        : TEXT("editor.");

    TSharedPtr<FJsonObject> ResponseJson;
    FEvent* DoneEvent = FPlatformProcess::GetSynchEventFromPool();

    AsyncTask(ENamedThreads::GameThread, [&]()
    {
        TArray<TSharedPtr<FJsonValue>> Commands;

        FConsoleObjectVisitor VisitorDelegate;
        VisitorDelegate.BindLambda([&Commands](const TCHAR* Name, IConsoleObject* Obj)
        {
            TSharedPtr<FJsonObject> Cmd = MakeShareable(new FJsonObject);
            Cmd->SetStringField(TEXT("name"), FString(Name));
            Cmd->SetStringField(TEXT("help"), Obj->GetHelp());
            Commands.Add(MakeShareable(new FJsonValueObject(Cmd)));
        });

        IConsoleManager::Get().ForEachConsoleObjectThatStartsWith(VisitorDelegate, *Prefix);

        TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
        Result->SetStringField(TEXT("prefix"), Prefix);
        Result->SetNumberField(TEXT("count"), Commands.Num());
        Result->SetArrayField(TEXT("commands"), Commands);

        ResponseJson = MakeShareable(new FJsonObject);
        ResponseJson->SetBoolField(TEXT("success"), true);
        ResponseJson->SetObjectField(TEXT("result"), Result);

        DoneEvent->Trigger();
    });

    DoneEvent->Wait();
    FPlatformProcess::ReturnSynchEventToPool(DoneEvent);

    FString Out;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Out);
    FJsonSerializer::Serialize(ResponseJson.ToSharedRef(), Writer);
    return Out;
}
