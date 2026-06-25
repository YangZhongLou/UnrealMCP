#include "CoreMinimal.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Kismet/GameplayStatics.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Async/Async.h"
#include "EngineUtils.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "CameraRig_Rail.h"
#include "CineCameraComponent.h"
#include "Components/SplineComponent.h"
#include "UnrealMCP.h"

static UWorld* GetPlayWorld()
{
#if WITH_EDITOR
    if (GEditor)
    {
        for (const FWorldContext& Context : GEditor->GetWorldContexts())
        {
            if (Context.WorldType == EWorldType::PIE && Context.World())
            {
                return Context.World();
            }
        }
        return GEditor->GetEditorWorldContext().World();
    }
#endif
    return GEngine ? GEngine->GetCurrentPlayWorld() : nullptr;
}

/** Find the current runtime camera target: PlayerController ViewTarget, or the first ACameraActor in the scene. */
static AActor* FindRuntimeCameraTarget(UWorld* World)
{
    if (!World)
    {
        return nullptr;
    }

    if (APlayerController* PC = UGameplayStatics::GetPlayerController(World, 0))
    {
        if (AActor* ViewTarget = PC->GetViewTarget())
        {
            return ViewTarget;
        }
    }

    for (TActorIterator<ACameraActor> It(World); It; ++It)
    {
        return *It;
    }

    return nullptr;
}

static UCameraComponent* GetCameraComponent(AActor* Actor)
{
    if (!Actor)
    {
        return nullptr;
    }
    if (UCineCameraComponent* CineCam = Actor->FindComponentByClass<UCineCameraComponent>())
    {
        return CineCam;
    }
    return Actor->FindComponentByClass<UCameraComponent>();
}

static USpringArmComponent* GetSpringArmComponent(AActor* Actor)
{
    if (!Actor)
    {
        return nullptr;
    }
    return Actor->FindComponentByClass<USpringArmComponent>();
}

static AActor* FindActorByName(UWorld* World, const FString& Name)
{
    if (!World)
    {
        return nullptr;
    }
    for (TActorIterator<AActor> It(World); It; ++It)
    {
        if (It->GetActorNameOrLabel() == Name || It->GetName() == Name)
        {
            return *It;
        }
    }
    return nullptr;
}

static ACameraActor* FindCameraActorByName(UWorld* World, const FString& Name)
{
    if (!World)
    {
        return nullptr;
    }
    for (TActorIterator<ACameraActor> It(World); It; ++It)
    {
        if (It->GetActorNameOrLabel() == Name || It->GetName() == Name)
        {
            return *It;
        }
    }
    return nullptr;
}

static TArray<ACameraActor*> FindAllCameraActors(UWorld* World)
{
    TArray<ACameraActor*> Cameras;
    if (!World)
    {
        return Cameras;
    }
    for (TActorIterator<ACameraActor> It(World); It; ++It)
    {
        Cameras.Add(*It);
    }
    return Cameras;
}

static ACameraRig_Rail* FindCameraRigRailByName(UWorld* World, const FString& Name)
{
    if (!World)
    {
        return nullptr;
    }
    for (TActorIterator<ACameraRig_Rail> It(World); It; ++It)
    {
        if (It->GetActorNameOrLabel() == Name || It->GetName() == Name)
        {
            return *It;
        }
    }
    return nullptr;
}

static FString BuildSuccessResponse(const TSharedPtr<FJsonObject>& ResultObj, const FString& RequestId)
{
    TSharedPtr<FJsonObject> Response = MakeShareable(new FJsonObject);
    Response->SetStringField(TEXT("id"), RequestId);
    Response->SetBoolField(TEXT("success"), true);
    Response->SetObjectField(TEXT("result"), ResultObj);

    FString ResultStr;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ResultStr);
    FJsonSerializer::Serialize(Response.ToSharedRef(), Writer);
    return ResultStr;
}

static FString BuildErrorResponse(const FString& Error, const FString& RequestId)
{
    TSharedPtr<FJsonObject> Response = MakeShareable(new FJsonObject);
    Response->SetStringField(TEXT("id"), RequestId);
    Response->SetBoolField(TEXT("success"), false);
    Response->SetStringField(TEXT("error"), Error);

    FString ResultStr;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ResultStr);
    FJsonSerializer::Serialize(Response.ToSharedRef(), Writer);
    return ResultStr;
}

// ---------- get_runtime_camera_state ----------

FString HandleGetRuntimeCameraState(const TSharedPtr<FJsonObject>& Params)
{
    FString RequestId = Params->GetStringField(TEXT("id"));

    FEvent* DoneEvent = FPlatformProcess::GetSynchEventFromPool();
    FString ResultStr;

    AsyncTask(ENamedThreads::GameThread, [&]()
    {
        UWorld* World = GetPlayWorld();
        AActor* Target = FindRuntimeCameraTarget(World);
        UCameraComponent* CamComp = GetCameraComponent(Target);
        USpringArmComponent* SpringArm = GetSpringArmComponent(Target);

        if (!Target)
        {
            ResultStr = BuildErrorResponse(TEXT("No runtime camera target found"), RequestId);
            DoneEvent->Trigger();
            return;
        }

        TSharedPtr<FJsonObject> ResultObj = MakeShareable(new FJsonObject);

        // Location
        FVector Loc = Target->GetActorLocation();
        TArray<TSharedPtr<FJsonValue>> LocArr;
        LocArr.Add(MakeShareable(new FJsonValueNumber(Loc.X)));
        LocArr.Add(MakeShareable(new FJsonValueNumber(Loc.Y)));
        LocArr.Add(MakeShareable(new FJsonValueNumber(Loc.Z)));
        ResultObj->SetArrayField(TEXT("location"), LocArr);

        // Rotation
        FRotator Rot = Target->GetActorRotation();
        TArray<TSharedPtr<FJsonValue>> RotArr;
        RotArr.Add(MakeShareable(new FJsonValueNumber(Rot.Pitch)));
        RotArr.Add(MakeShareable(new FJsonValueNumber(Rot.Yaw)));
        RotArr.Add(MakeShareable(new FJsonValueNumber(Rot.Roll)));
        ResultObj->SetArrayField(TEXT("rotation"), RotArr);

        // Zoom (SpringArm target arm length)
        ResultObj->SetNumberField(TEXT("zoom"), SpringArm ? SpringArm->TargetArmLength : 0.0);

        // FOV
        ResultObj->SetNumberField(TEXT("fov"), CamComp ? CamComp->FieldOfView : 90.0);

        // Post-process
        ResultObj->SetNumberField(TEXT("exposure"), CamComp ? CamComp->PostProcessSettings.AutoExposureBias : 0.0);
        ResultObj->SetNumberField(TEXT("bloom"), CamComp ? CamComp->PostProcessSettings.BloomIntensity : 0.0);

        // CineCamera (includes DOF & focus)
        if (UCineCameraComponent* CineCam = Cast<UCineCameraComponent>(CamComp))
        {
            ResultObj->SetNumberField(TEXT("focalLength"), CineCam->CurrentFocalLength);
            ResultObj->SetNumberField(TEXT("aperture"), CineCam->CurrentAperture);
            ResultObj->SetNumberField(TEXT("dof_focal_distance"), CineCam->FocusSettings.ManualFocusDistance);
            ResultObj->SetNumberField(TEXT("focusDistance"), CineCam->FocusSettings.ManualFocusDistance);
        }
        else
        {
            ResultObj->SetNumberField(TEXT("focalLength"), 0.0);
            ResultObj->SetNumberField(TEXT("aperture"), 0.0);
            ResultObj->SetNumberField(TEXT("dof_focal_distance"), 0.0);
            ResultObj->SetNumberField(TEXT("focusDistance"), 0.0);
        }

        // Advanced Post-process
        ResultObj->SetNumberField(TEXT("motionBlur"), CamComp ? CamComp->PostProcessSettings.MotionBlurAmount : 0.0);
        ResultObj->SetNumberField(TEXT("vignette"), CamComp ? CamComp->PostProcessSettings.VignetteIntensity : 0.0);
        ResultObj->SetNumberField(TEXT("chromaticAberration"), CamComp ? CamComp->PostProcessSettings.SceneFringeIntensity : 0.0);

        ResultStr = BuildSuccessResponse(ResultObj, RequestId);
        DoneEvent->Trigger();
    });

    DoneEvent->Wait();
    FPlatformProcess::ReturnSynchEventToPool(DoneEvent);
    return ResultStr;
}

// ---------- set_runtime_camera_fov ----------

FString HandleSetRuntimeCameraFOV(const TSharedPtr<FJsonObject>& Params)
{
    FString RequestId = Params->GetStringField(TEXT("id"));

    if (!Params->HasField(TEXT("fov")))
    {
        return BuildErrorResponse(TEXT("Missing 'fov' parameter"), RequestId);
    }

    double FOV = Params->GetNumberField(TEXT("fov"));

    FEvent* DoneEvent = FPlatformProcess::GetSynchEventFromPool();
    FString ResultStr;

    AsyncTask(ENamedThreads::GameThread, [&]()
    {
        AActor* Target = FindRuntimeCameraTarget(GetPlayWorld());
        UCameraComponent* CamComp = GetCameraComponent(Target);

        if (!Target)
        {
            ResultStr = BuildErrorResponse(TEXT("No runtime camera target found"), RequestId);
            DoneEvent->Trigger();
            return;
        }
        if (!CamComp)
        {
            ResultStr = BuildErrorResponse(TEXT("No CameraComponent found on target"), RequestId);
            DoneEvent->Trigger();
            return;
        }

        CamComp->SetFieldOfView(static_cast<float>(FOV));

        TSharedPtr<FJsonObject> ResultObj = MakeShareable(new FJsonObject);
        ResultObj->SetNumberField(TEXT("fov"), CamComp->FieldOfView);
        ResultStr = BuildSuccessResponse(ResultObj, RequestId);
        DoneEvent->Trigger();
    });

    DoneEvent->Wait();
    FPlatformProcess::ReturnSynchEventToPool(DoneEvent);
    return ResultStr;
}

// ---------- set_runtime_camera_dof ----------

FString HandleSetRuntimeCameraDOF(const TSharedPtr<FJsonObject>& Params)
{
    FString RequestId = Params->GetStringField(TEXT("id"));

    if (!Params->HasField(TEXT("focalDistance")))
    {
        return BuildErrorResponse(TEXT("Missing 'focalDistance' parameter"), RequestId);
    }

    double FocalDistance = Params->GetNumberField(TEXT("focalDistance"));

    FEvent* DoneEvent = FPlatformProcess::GetSynchEventFromPool();
    FString ResultStr;

    AsyncTask(ENamedThreads::GameThread, [&]()
    {
        AActor* Target = FindRuntimeCameraTarget(GetPlayWorld());
        UCameraComponent* CamComp = GetCameraComponent(Target);

        if (!Target)
        {
            ResultStr = BuildErrorResponse(TEXT("No runtime camera target found"), RequestId);
            DoneEvent->Trigger();
            return;
        }
        UCineCameraComponent* CineCam = Cast<UCineCameraComponent>(CamComp);
        if (!CineCam)
        {
            ResultStr = BuildErrorResponse(TEXT("CineCameraComponent required for DOF"), RequestId);
            DoneEvent->Trigger();
            return;
        }

        CineCam->FocusSettings.FocusMethod = ECameraFocusMethod::Manual;
        CineCam->FocusSettings.ManualFocusDistance = static_cast<float>(FocalDistance);

        TSharedPtr<FJsonObject> ResultObj = MakeShareable(new FJsonObject);
        ResultObj->SetNumberField(TEXT("focal_distance"), CineCam->FocusSettings.ManualFocusDistance);
        ResultStr = BuildSuccessResponse(ResultObj, RequestId);
        DoneEvent->Trigger();
    });

    DoneEvent->Wait();
    FPlatformProcess::ReturnSynchEventToPool(DoneEvent);
    return ResultStr;
}

// ---------- set_runtime_camera_post_process ----------

FString HandleSetRuntimeCameraPostProcess(const TSharedPtr<FJsonObject>& Params)
{
    FString RequestId = Params->GetStringField(TEXT("id"));

    FEvent* DoneEvent = FPlatformProcess::GetSynchEventFromPool();
    FString ResultStr;

    AsyncTask(ENamedThreads::GameThread, [&]()
    {
        AActor* Target = FindRuntimeCameraTarget(GetPlayWorld());
        UCameraComponent* CamComp = GetCameraComponent(Target);

        if (!Target)
        {
            ResultStr = BuildErrorResponse(TEXT("No runtime camera target found"), RequestId);
            DoneEvent->Trigger();
            return;
        }
        if (!CamComp)
        {
            ResultStr = BuildErrorResponse(TEXT("No CameraComponent found on target"), RequestId);
            DoneEvent->Trigger();
            return;
        }

        TSharedPtr<FJsonObject> ResultObj = MakeShareable(new FJsonObject);

        if (Params->HasField(TEXT("exposure")))
        {
            double Exposure = Params->GetNumberField(TEXT("exposure"));
            CamComp->PostProcessSettings.bOverride_AutoExposureBias = true;
            CamComp->PostProcessSettings.AutoExposureBias = static_cast<float>(Exposure);
            ResultObj->SetNumberField(TEXT("exposure"), CamComp->PostProcessSettings.AutoExposureBias);
        }

        if (Params->HasField(TEXT("bloom")))
        {
            double Bloom = Params->GetNumberField(TEXT("bloom"));
            CamComp->PostProcessSettings.bOverride_BloomIntensity = true;
            CamComp->PostProcessSettings.BloomIntensity = static_cast<float>(Bloom);
            ResultObj->SetNumberField(TEXT("bloom"), CamComp->PostProcessSettings.BloomIntensity);
        }

        ResultStr = BuildSuccessResponse(ResultObj, RequestId);
        DoneEvent->Trigger();
    });

    DoneEvent->Wait();
    FPlatformProcess::ReturnSynchEventToPool(DoneEvent);
    return ResultStr;
}

// ---------- set_runtime_camera_transform ----------

FString HandleSetRuntimeCameraTransform(const TSharedPtr<FJsonObject>& Params)
{
    FString RequestId = Params->GetStringField(TEXT("id"));

    FEvent* DoneEvent = FPlatformProcess::GetSynchEventFromPool();
    FString ResultStr;

    AsyncTask(ENamedThreads::GameThread, [&]()
    {
        AActor* Target = FindRuntimeCameraTarget(GetPlayWorld());
        USpringArmComponent* SpringArm = GetSpringArmComponent(Target);

        if (!Target)
        {
            ResultStr = BuildErrorResponse(TEXT("No runtime camera target found"), RequestId);
            DoneEvent->Trigger();
            return;
        }

        // In PIE, spawn a dedicated camera actor and make the player look through it.
        // This avoids fighting DefaultPawn's rotation constraints and spring-arm offsets.
        UWorld* World = GetPlayWorld();
        APlayerController* PC = (World ? UGameplayStatics::GetPlayerController(World, 0) : nullptr);
        ACameraActor* PinnedCamera = nullptr;
        if (PC)
        {
            for (TActorIterator<ACameraActor> It(World); It; ++It)
            {
                if (It->GetActorNameOrLabel().StartsWith(TEXT("MCP_PIE_Camera")))
                {
                    PinnedCamera = *It;
                    break;
                }
            }
            if (!PinnedCamera)
            {
                FActorSpawnParameters SpawnParams;
                SpawnParams.Name = FName(TEXT("MCP_PIE_Camera"));
                SpawnParams.bNoFail = true;
                SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
                PinnedCamera = World->SpawnActor<ACameraActor>(SpawnParams);
                if (PinnedCamera)
                {
                    PinnedCamera->SetActorLabel(TEXT("MCP_PIE_Camera"));
                }
            }
            if (PinnedCamera)
            {
                PC->SetViewTargetWithBlend(PinnedCamera, 0.0f);
                Target = PinnedCamera;
                SpringArm = nullptr;
            }
        }

        if (Params->HasField(TEXT("location")))
        {
            const TArray<TSharedPtr<FJsonValue>>& Arr = Params->GetArrayField(TEXT("location"));
            if (Arr.Num() >= 3)
            {
                FVector NewLocation(Arr[0]->AsNumber(), Arr[1]->AsNumber(), Arr[2]->AsNumber());
                Target->SetActorLocation(NewLocation);
            }
        }

        if (Params->HasField(TEXT("rotation")))
        {
            const TArray<TSharedPtr<FJsonValue>>& Arr = Params->GetArrayField(TEXT("rotation"));
            if (Arr.Num() >= 3)
            {
                FRotator NewRotation(Arr[0]->AsNumber(), Arr[1]->AsNumber(), Arr[2]->AsNumber());
                Target->SetActorRotation(NewRotation);
                if (UCameraComponent* CamComp = GetCameraComponent(Target))
                {
                    CamComp->SetWorldRotation(NewRotation);
                }
                if (PC)
                {
                    PC->SetControlRotation(NewRotation);
                }
            }
        }

        if (Params->HasField(TEXT("fov")))
        {
            double NewFOV = Params->GetNumberField(TEXT("fov"));
            if (UCameraComponent* CamComp = GetCameraComponent(Target))
            {
                CamComp->SetFieldOfView(static_cast<float>(NewFOV));
            }
        }

        if (Params->HasField(TEXT("zoom")))
        {
            double Zoom = Params->GetNumberField(TEXT("zoom"));
            if (SpringArm)
            {
                SpringArm->TargetArmLength = static_cast<float>(Zoom);
            }
        }

        TSharedPtr<FJsonObject> ResultObj = MakeShareable(new FJsonObject);
        FVector Loc = Target->GetActorLocation();
        FRotator Rot = Target->GetActorRotation();
        TArray<TSharedPtr<FJsonValue>> LocArr;
        LocArr.Add(MakeShareable(new FJsonValueNumber(Loc.X)));
        LocArr.Add(MakeShareable(new FJsonValueNumber(Loc.Y)));
        LocArr.Add(MakeShareable(new FJsonValueNumber(Loc.Z)));
        ResultObj->SetArrayField(TEXT("location"), LocArr);
        TArray<TSharedPtr<FJsonValue>> RotArr;
        RotArr.Add(MakeShareable(new FJsonValueNumber(Rot.Pitch)));
        RotArr.Add(MakeShareable(new FJsonValueNumber(Rot.Yaw)));
        RotArr.Add(MakeShareable(new FJsonValueNumber(Rot.Roll)));
        ResultObj->SetArrayField(TEXT("rotation"), RotArr);
        ResultObj->SetNumberField(TEXT("zoom"), SpringArm ? SpringArm->TargetArmLength : 0.0);

        ResultStr = BuildSuccessResponse(ResultObj, RequestId);
        DoneEvent->Trigger();
    });

    DoneEvent->Wait();
    FPlatformProcess::ReturnSynchEventToPool(DoneEvent);
    return ResultStr;
}

// ---------- focus_runtime_camera_on_actor ----------

FString HandleFocusRuntimeCameraOnActor(const TSharedPtr<FJsonObject>& Params)
{
    FString RequestId = Params->GetStringField(TEXT("id"));

    if (!Params->HasField(TEXT("actorName")))
    {
        return BuildErrorResponse(TEXT("Missing 'actorName' parameter"), RequestId);
    }

    FString ActorName = Params->GetStringField(TEXT("actorName"));

    FEvent* DoneEvent = FPlatformProcess::GetSynchEventFromPool();
    FString ResultStr;

    AsyncTask(ENamedThreads::GameThread, [&]()
    {
        UWorld* World = GetPlayWorld();
        AActor* Target = FindRuntimeCameraTarget(World);

        if (!Target)
        {
            ResultStr = BuildErrorResponse(TEXT("No runtime camera target found"), RequestId);
            DoneEvent->Trigger();
            return;
        }

        AActor* TargetActor = FindActorByName(World, ActorName);
        if (!TargetActor)
        {
            ResultStr = BuildErrorResponse(FString::Printf(TEXT("Actor not found: %s"), *ActorName), RequestId);
            DoneEvent->Trigger();
            return;
        }

        Target->SetActorLocation(TargetActor->GetActorLocation());

        TSharedPtr<FJsonObject> ResultObj = MakeShareable(new FJsonObject);
        ResultObj->SetStringField(TEXT("actor_name"), ActorName);
        FVector TargetLocation = TargetActor->GetActorLocation();
        TArray<TSharedPtr<FJsonValue>> LocArr;
        LocArr.Add(MakeShareable(new FJsonValueNumber(TargetLocation.X)));
        LocArr.Add(MakeShareable(new FJsonValueNumber(TargetLocation.Y)));
        LocArr.Add(MakeShareable(new FJsonValueNumber(TargetLocation.Z)));
        ResultObj->SetArrayField(TEXT("target_location"), LocArr);

        ResultStr = BuildSuccessResponse(ResultObj, RequestId);
        DoneEvent->Trigger();
    });

    DoneEvent->Wait();
    FPlatformProcess::ReturnSynchEventToPool(DoneEvent);
    return ResultStr;
}

// ---------- set_runtime_camera_focal_length ----------

FString HandleSetRuntimeCameraFocalLength(const TSharedPtr<FJsonObject>& Params)
{
    FString RequestId = Params->GetStringField(TEXT("id"));

    if (!Params->HasField(TEXT("focalLength")))
    {
        return BuildErrorResponse(TEXT("Missing 'focalLength' parameter"), RequestId);
    }

    double FocalLength = Params->GetNumberField(TEXT("focalLength"));

    FEvent* DoneEvent = FPlatformProcess::GetSynchEventFromPool();
    FString ResultStr;

    AsyncTask(ENamedThreads::GameThread, [&]()
    {
        AActor* Target = FindRuntimeCameraTarget(GetPlayWorld());
        UCameraComponent* CamComp = GetCameraComponent(Target);

        if (!Target)
        {
            ResultStr = BuildErrorResponse(TEXT("No runtime camera target found"), RequestId);
            DoneEvent->Trigger();
            return;
        }

        UCineCameraComponent* CineCam = Cast<UCineCameraComponent>(CamComp);
        if (!CineCam)
        {
            ResultStr = BuildErrorResponse(TEXT("CineCameraComponent required for focal length"), RequestId);
            DoneEvent->Trigger();
            return;
        }

        CineCam->CurrentFocalLength = static_cast<float>(FocalLength);

        TSharedPtr<FJsonObject> ResultObj = MakeShareable(new FJsonObject);
        ResultObj->SetNumberField(TEXT("focalLength"), CineCam->CurrentFocalLength);
        ResultStr = BuildSuccessResponse(ResultObj, RequestId);
        DoneEvent->Trigger();
    });

    DoneEvent->Wait();
    FPlatformProcess::ReturnSynchEventToPool(DoneEvent);
    return ResultStr;
}

// ---------- set_runtime_camera_aperture ----------

FString HandleSetRuntimeCameraAperture(const TSharedPtr<FJsonObject>& Params)
{
    FString RequestId = Params->GetStringField(TEXT("id"));

    if (!Params->HasField(TEXT("aperture")))
    {
        return BuildErrorResponse(TEXT("Missing 'aperture' parameter"), RequestId);
    }

    double Aperture = Params->GetNumberField(TEXT("aperture"));

    FEvent* DoneEvent = FPlatformProcess::GetSynchEventFromPool();
    FString ResultStr;

    AsyncTask(ENamedThreads::GameThread, [&]()
    {
        AActor* Target = FindRuntimeCameraTarget(GetPlayWorld());
        UCameraComponent* CamComp = GetCameraComponent(Target);

        if (!Target)
        {
            ResultStr = BuildErrorResponse(TEXT("No runtime camera target found"), RequestId);
            DoneEvent->Trigger();
            return;
        }

        UCineCameraComponent* CineCam = Cast<UCineCameraComponent>(CamComp);
        if (!CineCam)
        {
            ResultStr = BuildErrorResponse(TEXT("CineCameraComponent required for aperture"), RequestId);
            DoneEvent->Trigger();
            return;
        }

        CineCam->CurrentAperture = static_cast<float>(Aperture);

        TSharedPtr<FJsonObject> ResultObj = MakeShareable(new FJsonObject);
        ResultObj->SetNumberField(TEXT("aperture"), CineCam->CurrentAperture);
        ResultStr = BuildSuccessResponse(ResultObj, RequestId);
        DoneEvent->Trigger();
    });

    DoneEvent->Wait();
    FPlatformProcess::ReturnSynchEventToPool(DoneEvent);
    return ResultStr;
}

// ---------- set_runtime_camera_focus_distance ----------

FString HandleSetRuntimeCameraFocusDistance(const TSharedPtr<FJsonObject>& Params)
{
    FString RequestId = Params->GetStringField(TEXT("id"));

    if (!Params->HasField(TEXT("focusDistance")))
    {
        return BuildErrorResponse(TEXT("Missing 'focusDistance' parameter"), RequestId);
    }

    double FocusDistance = Params->GetNumberField(TEXT("focusDistance"));

    FEvent* DoneEvent = FPlatformProcess::GetSynchEventFromPool();
    FString ResultStr;

    AsyncTask(ENamedThreads::GameThread, [&]()
    {
        AActor* Target = FindRuntimeCameraTarget(GetPlayWorld());
        UCameraComponent* CamComp = GetCameraComponent(Target);

        if (!Target)
        {
            ResultStr = BuildErrorResponse(TEXT("No runtime camera target found"), RequestId);
            DoneEvent->Trigger();
            return;
        }
        UCineCameraComponent* CineCam = Cast<UCineCameraComponent>(CamComp);
        if (!CineCam)
        {
            ResultStr = BuildErrorResponse(TEXT("CineCameraComponent required for focus distance"), RequestId);
            DoneEvent->Trigger();
            return;
        }

        CineCam->FocusSettings.FocusMethod = ECameraFocusMethod::Manual;
        CineCam->FocusSettings.ManualFocusDistance = static_cast<float>(FocusDistance);

        TSharedPtr<FJsonObject> ResultObj = MakeShareable(new FJsonObject);
        ResultObj->SetNumberField(TEXT("focusDistance"), CineCam->FocusSettings.ManualFocusDistance);
        ResultStr = BuildSuccessResponse(ResultObj, RequestId);
        DoneEvent->Trigger();
    });

    DoneEvent->Wait();
    FPlatformProcess::ReturnSynchEventToPool(DoneEvent);
    return ResultStr;
}

// ---------- start_camera_rig ----------

FString HandleStartCameraRig(const TSharedPtr<FJsonObject>& Params)
{
    FString RequestId = Params->GetStringField(TEXT("id"));

    if (!Params->HasField(TEXT("rigName")))
    {
        return BuildErrorResponse(TEXT("Missing 'rigName' parameter"), RequestId);
    }

    FString RigName = Params->GetStringField(TEXT("rigName"));

    FEvent* DoneEvent = FPlatformProcess::GetSynchEventFromPool();
    FString ResultStr;

    AsyncTask(ENamedThreads::GameThread, [&]()
    {
        UWorld* World = GetPlayWorld();
        AActor* Target = FindRuntimeCameraTarget(World);
        ACameraRig_Rail* Rail = FindCameraRigRailByName(World, RigName);

        if (!Target)
        {
            ResultStr = BuildErrorResponse(TEXT("No runtime camera target found"), RequestId);
            DoneEvent->Trigger();
            return;
        }
        if (!Rail)
        {
            ResultStr = BuildErrorResponse(FString::Printf(TEXT("CameraRig_Rail not found: %s"), *RigName), RequestId);
            DoneEvent->Trigger();
            return;
        }

        bool bFound = false;
        for (FCameraRigPlayback& State : GActiveCameraRigPlaybacks)
        {
            if (State.Rail.Get() == Rail)
            {
                State.AttachedActor = Target;
                State.bIsPlaying = true;
                bFound = true;
                break;
            }
        }
        if (!bFound)
        {
            FCameraRigPlayback NewState;
            NewState.Rail = Rail;
            NewState.AttachedActor = Target;
            NewState.bIsPlaying = true;
            GActiveCameraRigPlaybacks.Add(NewState);
        }

        TSharedPtr<FJsonObject> ResultObj = MakeShareable(new FJsonObject);
        ResultObj->SetStringField(TEXT("rig_name"), RigName);
        ResultObj->SetBoolField(TEXT("playing"), true);
        ResultStr = BuildSuccessResponse(ResultObj, RequestId);
        DoneEvent->Trigger();
    });

    DoneEvent->Wait();
    FPlatformProcess::ReturnSynchEventToPool(DoneEvent);
    return ResultStr;
}

// ---------- stop_camera_rig ----------

FString HandleStopCameraRig(const TSharedPtr<FJsonObject>& Params)
{
    FString RequestId = Params->GetStringField(TEXT("id"));

    if (!Params->HasField(TEXT("rigName")))
    {
        return BuildErrorResponse(TEXT("Missing 'rigName' parameter"), RequestId);
    }

    FString RigName = Params->GetStringField(TEXT("rigName"));

    FEvent* DoneEvent = FPlatformProcess::GetSynchEventFromPool();
    FString ResultStr;

    AsyncTask(ENamedThreads::GameThread, [&]()
    {
        UWorld* World = GetPlayWorld();
        ACameraRig_Rail* Rail = FindCameraRigRailByName(World, RigName);

        if (!Rail)
        {
            ResultStr = BuildErrorResponse(FString::Printf(TEXT("CameraRig_Rail not found: %s"), *RigName), RequestId);
            DoneEvent->Trigger();
            return;
        }

        for (FCameraRigPlayback& State : GActiveCameraRigPlaybacks)
        {
            if (State.Rail.Get() == Rail)
            {
                State.bIsPlaying = false;
                break;
            }
        }

        TSharedPtr<FJsonObject> ResultObj = MakeShareable(new FJsonObject);
        ResultObj->SetStringField(TEXT("rig_name"), RigName);
        ResultObj->SetBoolField(TEXT("playing"), false);
        ResultStr = BuildSuccessResponse(ResultObj, RequestId);
        DoneEvent->Trigger();
    });

    DoneEvent->Wait();
    FPlatformProcess::ReturnSynchEventToPool(DoneEvent);
    return ResultStr;
}

// ---------- set_camera_rig_speed ----------

FString HandleSetCameraRigSpeed(const TSharedPtr<FJsonObject>& Params)
{
    FString RequestId = Params->GetStringField(TEXT("id"));

    if (!Params->HasField(TEXT("rigName")))
    {
        return BuildErrorResponse(TEXT("Missing 'rigName' parameter"), RequestId);
    }
    if (!Params->HasField(TEXT("speed")))
    {
        return BuildErrorResponse(TEXT("Missing 'speed' parameter"), RequestId);
    }

    FString RigName = Params->GetStringField(TEXT("rigName"));
    double Speed = Params->GetNumberField(TEXT("speed"));

    FEvent* DoneEvent = FPlatformProcess::GetSynchEventFromPool();
    FString ResultStr;

    AsyncTask(ENamedThreads::GameThread, [&]()
    {
        UWorld* World = GetPlayWorld();
        ACameraRig_Rail* Rail = FindCameraRigRailByName(World, RigName);

        if (!Rail)
        {
            ResultStr = BuildErrorResponse(FString::Printf(TEXT("CameraRig_Rail not found: %s"), *RigName), RequestId);
            DoneEvent->Trigger();
            return;
        }

        float CurrentSpeed = 0.0f;
        bool bFound = false;
        for (FCameraRigPlayback& State : GActiveCameraRigPlaybacks)
        {
            if (State.Rail.Get() == Rail)
            {
                State.Speed = static_cast<float>(Speed);
                CurrentSpeed = State.Speed;
                bFound = true;
                break;
            }
        }
        if (!bFound)
        {
            // Create a stopped entry so speed is recorded for next start
            FCameraRigPlayback NewState;
            NewState.Rail = Rail;
            NewState.Speed = static_cast<float>(Speed);
            NewState.bIsPlaying = false;
            GActiveCameraRigPlaybacks.Add(NewState);
            CurrentSpeed = NewState.Speed;
        }

        TSharedPtr<FJsonObject> ResultObj = MakeShareable(new FJsonObject);
        ResultObj->SetStringField(TEXT("rig_name"), RigName);
        ResultObj->SetNumberField(TEXT("speed"), CurrentSpeed);
        ResultStr = BuildSuccessResponse(ResultObj, RequestId);
        DoneEvent->Trigger();
    });

    DoneEvent->Wait();
    FPlatformProcess::ReturnSynchEventToPool(DoneEvent);
    return ResultStr;
}

// ---------- switch_camera ----------

FString HandleSwitchCamera(const TSharedPtr<FJsonObject>& Params)
{
    FString RequestId = Params->GetStringField(TEXT("id"));

    if (!Params->HasField(TEXT("cameraName")))
    {
        return BuildErrorResponse(TEXT("Missing 'cameraName' parameter"), RequestId);
    }

    FString CameraName = Params->GetStringField(TEXT("cameraName"));
    double BlendTime = 1.0;
    if (Params->HasField(TEXT("blendTime")))
    {
        BlendTime = Params->GetNumberField(TEXT("blendTime"));
    }

    FEvent* DoneEvent = FPlatformProcess::GetSynchEventFromPool();
    FString ResultStr;

    AsyncTask(ENamedThreads::GameThread, [&]()
    {
        UWorld* World = GetPlayWorld();
        ACameraActor* CameraActor = FindCameraActorByName(World, CameraName);
        APlayerController* PC = UGameplayStatics::GetPlayerController(World, 0);

        if (!CameraActor)
        {
            ResultStr = BuildErrorResponse(FString::Printf(TEXT("CameraActor not found: %s"), *CameraName), RequestId);
            DoneEvent->Trigger();
            return;
        }
        if (!PC)
        {
            ResultStr = BuildErrorResponse(TEXT("No PlayerController found"), RequestId);
            DoneEvent->Trigger();
            return;
        }

        PC->SetViewTargetWithBlend(CameraActor, static_cast<float>(BlendTime));

        TSharedPtr<FJsonObject> ResultObj = MakeShareable(new FJsonObject);
        ResultObj->SetStringField(TEXT("camera_name"), CameraName);
        ResultObj->SetNumberField(TEXT("blend_time"), BlendTime);
        ResultObj->SetStringField(TEXT("current_camera"), CameraActor->GetActorNameOrLabel());
        ResultStr = BuildSuccessResponse(ResultObj, RequestId);
        DoneEvent->Trigger();
    });

    DoneEvent->Wait();
    FPlatformProcess::ReturnSynchEventToPool(DoneEvent);
    return ResultStr;
}

// ---------- next_camera ----------

FString HandleNextCamera(const TSharedPtr<FJsonObject>& Params)
{
    FString RequestId = Params->GetStringField(TEXT("id"));

    double BlendTime = 1.0;
    if (Params->HasField(TEXT("blendTime")))
    {
        BlendTime = Params->GetNumberField(TEXT("blendTime"));
    }

    FEvent* DoneEvent = FPlatformProcess::GetSynchEventFromPool();
    FString ResultStr;

    AsyncTask(ENamedThreads::GameThread, [&]()
    {
        UWorld* World = GetPlayWorld();
        TArray<ACameraActor*> Cameras = FindAllCameraActors(World);
        APlayerController* PC = UGameplayStatics::GetPlayerController(World, 0);

        if (Cameras.Num() == 0)
        {
            ResultStr = BuildErrorResponse(TEXT("No CameraActors found in scene"), RequestId);
            DoneEvent->Trigger();
            return;
        }
        if (!PC)
        {
            ResultStr = BuildErrorResponse(TEXT("No PlayerController found"), RequestId);
            DoneEvent->Trigger();
            return;
        }

        AActor* CurrentViewTarget = PC->GetViewTarget();
        int32 CurrentIndex = -1;
        for (int32 i = 0; i < Cameras.Num(); ++i)
        {
            if (Cameras[i] == CurrentViewTarget)
            {
                CurrentIndex = i;
                break;
            }
        }

        int32 NextIndex = (CurrentIndex + 1) % Cameras.Num();
        PC->SetViewTargetWithBlend(Cameras[NextIndex], static_cast<float>(BlendTime));

        TSharedPtr<FJsonObject> ResultObj = MakeShareable(new FJsonObject);
        ResultObj->SetStringField(TEXT("current_camera"), Cameras[NextIndex]->GetActorNameOrLabel());
        ResultObj->SetNumberField(TEXT("blend_time"), BlendTime);
        ResultStr = BuildSuccessResponse(ResultObj, RequestId);
        DoneEvent->Trigger();
    });

    DoneEvent->Wait();
    FPlatformProcess::ReturnSynchEventToPool(DoneEvent);
    return ResultStr;
}

// ---------- prev_camera ----------

FString HandlePrevCamera(const TSharedPtr<FJsonObject>& Params)
{
    FString RequestId = Params->GetStringField(TEXT("id"));

    double BlendTime = 1.0;
    if (Params->HasField(TEXT("blendTime")))
    {
        BlendTime = Params->GetNumberField(TEXT("blendTime"));
    }

    FEvent* DoneEvent = FPlatformProcess::GetSynchEventFromPool();
    FString ResultStr;

    AsyncTask(ENamedThreads::GameThread, [&]()
    {
        UWorld* World = GetPlayWorld();
        TArray<ACameraActor*> Cameras = FindAllCameraActors(World);
        APlayerController* PC = UGameplayStatics::GetPlayerController(World, 0);

        if (Cameras.Num() == 0)
        {
            ResultStr = BuildErrorResponse(TEXT("No CameraActors found in scene"), RequestId);
            DoneEvent->Trigger();
            return;
        }
        if (!PC)
        {
            ResultStr = BuildErrorResponse(TEXT("No PlayerController found"), RequestId);
            DoneEvent->Trigger();
            return;
        }

        AActor* CurrentViewTarget = PC->GetViewTarget();
        int32 CurrentIndex = -1;
        for (int32 i = 0; i < Cameras.Num(); ++i)
        {
            if (Cameras[i] == CurrentViewTarget)
            {
                CurrentIndex = i;
                break;
            }
        }

        int32 PrevIndex = CurrentIndex <= 0 ? Cameras.Num() - 1 : CurrentIndex - 1;
        PC->SetViewTargetWithBlend(Cameras[PrevIndex], static_cast<float>(BlendTime));

        TSharedPtr<FJsonObject> ResultObj = MakeShareable(new FJsonObject);
        ResultObj->SetStringField(TEXT("current_camera"), Cameras[PrevIndex]->GetActorNameOrLabel());
        ResultObj->SetNumberField(TEXT("blend_time"), BlendTime);
        ResultStr = BuildSuccessResponse(ResultObj, RequestId);
        DoneEvent->Trigger();
    });

    DoneEvent->Wait();
    FPlatformProcess::ReturnSynchEventToPool(DoneEvent);
    return ResultStr;
}

// ---------- get_camera_list ----------

FString HandleGetCameraList(const TSharedPtr<FJsonObject>& Params)
{
    FString RequestId = Params->GetStringField(TEXT("id"));

    FEvent* DoneEvent = FPlatformProcess::GetSynchEventFromPool();
    FString ResultStr;

    AsyncTask(ENamedThreads::GameThread, [&]()
    {
        UWorld* World = GetPlayWorld();
        TArray<ACameraActor*> Cameras = FindAllCameraActors(World);
        APlayerController* PC = UGameplayStatics::GetPlayerController(World, 0);
        AActor* CurrentViewTarget = PC ? PC->GetViewTarget() : nullptr;

        TSharedPtr<FJsonObject> ResultObj = MakeShareable(new FJsonObject);
        TArray<TSharedPtr<FJsonValue>> CameraArr;
        for (ACameraActor* Cam : Cameras)
        {
            CameraArr.Add(MakeShareable(new FJsonValueString(Cam->GetActorNameOrLabel())));
        }
        ResultObj->SetArrayField(TEXT("cameras"), CameraArr);
        ResultObj->SetStringField(TEXT("current_camera"), CurrentViewTarget ? CurrentViewTarget->GetActorNameOrLabel() : TEXT(""));
        ResultObj->SetNumberField(TEXT("count"), Cameras.Num());
        ResultStr = BuildSuccessResponse(ResultObj, RequestId);
        DoneEvent->Trigger();
    });

    DoneEvent->Wait();
    FPlatformProcess::ReturnSynchEventToPool(DoneEvent);
    return ResultStr;
}

// ---------- set_runtime_camera_motion_blur ----------

FString HandleSetRuntimeCameraMotionBlur(const TSharedPtr<FJsonObject>& Params)
{
    FString RequestId = Params->GetStringField(TEXT("id"));

    if (!Params->HasField(TEXT("amount")))
    {
        return BuildErrorResponse(TEXT("Missing 'amount' parameter"), RequestId);
    }

    double Amount = Params->GetNumberField(TEXT("amount"));

    FEvent* DoneEvent = FPlatformProcess::GetSynchEventFromPool();
    FString ResultStr;

    AsyncTask(ENamedThreads::GameThread, [&]()
    {
        AActor* Target = FindRuntimeCameraTarget(GetPlayWorld());
        UCameraComponent* CamComp = GetCameraComponent(Target);

        if (!Target)
        {
            ResultStr = BuildErrorResponse(TEXT("No runtime camera target found"), RequestId);
            DoneEvent->Trigger();
            return;
        }
        if (!CamComp)
        {
            ResultStr = BuildErrorResponse(TEXT("No CameraComponent found on target"), RequestId);
            DoneEvent->Trigger();
            return;
        }

        CamComp->PostProcessSettings.bOverride_MotionBlurAmount = true;
        CamComp->PostProcessSettings.MotionBlurAmount = static_cast<float>(Amount);

        TSharedPtr<FJsonObject> ResultObj = MakeShareable(new FJsonObject);
        ResultObj->SetNumberField(TEXT("motionBlur"), CamComp->PostProcessSettings.MotionBlurAmount);
        ResultStr = BuildSuccessResponse(ResultObj, RequestId);
        DoneEvent->Trigger();
    });

    DoneEvent->Wait();
    FPlatformProcess::ReturnSynchEventToPool(DoneEvent);
    return ResultStr;
}

// ---------- set_runtime_camera_vignette ----------

FString HandleSetRuntimeCameraVignette(const TSharedPtr<FJsonObject>& Params)
{
    FString RequestId = Params->GetStringField(TEXT("id"));

    if (!Params->HasField(TEXT("intensity")))
    {
        return BuildErrorResponse(TEXT("Missing 'intensity' parameter"), RequestId);
    }

    double Intensity = Params->GetNumberField(TEXT("intensity"));

    FEvent* DoneEvent = FPlatformProcess::GetSynchEventFromPool();
    FString ResultStr;

    AsyncTask(ENamedThreads::GameThread, [&]()
    {
        AActor* Target = FindRuntimeCameraTarget(GetPlayWorld());
        UCameraComponent* CamComp = GetCameraComponent(Target);

        if (!Target)
        {
            ResultStr = BuildErrorResponse(TEXT("No runtime camera target found"), RequestId);
            DoneEvent->Trigger();
            return;
        }
        if (!CamComp)
        {
            ResultStr = BuildErrorResponse(TEXT("No CameraComponent found on target"), RequestId);
            DoneEvent->Trigger();
            return;
        }

        CamComp->PostProcessSettings.bOverride_VignetteIntensity = true;
        CamComp->PostProcessSettings.VignetteIntensity = static_cast<float>(Intensity);

        TSharedPtr<FJsonObject> ResultObj = MakeShareable(new FJsonObject);
        ResultObj->SetNumberField(TEXT("vignette"), CamComp->PostProcessSettings.VignetteIntensity);
        ResultStr = BuildSuccessResponse(ResultObj, RequestId);
        DoneEvent->Trigger();
    });

    DoneEvent->Wait();
    FPlatformProcess::ReturnSynchEventToPool(DoneEvent);
    return ResultStr;
}

// ---------- set_runtime_camera_chromatic_aberration ----------

FString HandleSetRuntimeCameraChromaticAberration(const TSharedPtr<FJsonObject>& Params)
{
    FString RequestId = Params->GetStringField(TEXT("id"));

    if (!Params->HasField(TEXT("intensity")))
    {
        return BuildErrorResponse(TEXT("Missing 'intensity' parameter"), RequestId);
    }

    double Intensity = Params->GetNumberField(TEXT("intensity"));

    FEvent* DoneEvent = FPlatformProcess::GetSynchEventFromPool();
    FString ResultStr;

    AsyncTask(ENamedThreads::GameThread, [&]()
    {
        AActor* Target = FindRuntimeCameraTarget(GetPlayWorld());
        UCameraComponent* CamComp = GetCameraComponent(Target);

        if (!Target)
        {
            ResultStr = BuildErrorResponse(TEXT("No runtime camera target found"), RequestId);
            DoneEvent->Trigger();
            return;
        }
        if (!CamComp)
        {
            ResultStr = BuildErrorResponse(TEXT("No CameraComponent found on target"), RequestId);
            DoneEvent->Trigger();
            return;
        }

        CamComp->PostProcessSettings.bOverride_SceneFringeIntensity = true;
        CamComp->PostProcessSettings.SceneFringeIntensity = static_cast<float>(Intensity);

        TSharedPtr<FJsonObject> ResultObj = MakeShareable(new FJsonObject);
        ResultObj->SetNumberField(TEXT("chromaticAberration"), CamComp->PostProcessSettings.SceneFringeIntensity);
        ResultStr = BuildSuccessResponse(ResultObj, RequestId);
        DoneEvent->Trigger();
    });

    DoneEvent->Wait();
    FPlatformProcess::ReturnSynchEventToPool(DoneEvent);
    return ResultStr;
}
