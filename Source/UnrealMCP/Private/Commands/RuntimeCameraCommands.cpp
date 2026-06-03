#if WITH_EDITOR
#include "CoreMinimal.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Kismet/GameplayStatics.h"
#include "Editor.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Async/Async.h"
#include "GameFramework/SpringArmComponent.h"
#include "CameraPlayground/IsometricCameraPawn.h"
#include "CameraPlayground/CameraRigActor.h"
#include "CameraPlayground/CameraSwitcher.h"

static UWorld* GetPlayWorld()
{
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
    return GEngine ? GEngine->GetCurrentPlayWorld() : nullptr;
}

static AIsometricCameraPawn* FindRuntimeCameraPawn(UWorld* World)
{
    if (!World)
    {
        return nullptr;
    }

    // Try player controller first
    if (APlayerController* PC = UGameplayStatics::GetPlayerController(World, 0))
    {
        if (AIsometricCameraPawn* Pawn = Cast<AIsometricCameraPawn>(PC->GetPawn()))
        {
            return Pawn;
        }
    }

    // Fallback: iterate all pawns
    for (TActorIterator<AIsometricCameraPawn> It(World); It; ++It)
    {
        return *It;
    }

    return nullptr;
}

static ACameraSwitcher* FindCameraSwitcher(UWorld* World)
{
    if (!World)
    {
        return nullptr;
    }
    for (TActorIterator<ACameraSwitcher> It(World); It; ++It)
    {
        return *It;
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
        AIsometricCameraPawn* CameraPawn = FindRuntimeCameraPawn(World);

        if (!CameraPawn)
        {
            ResultStr = BuildErrorResponse(TEXT("IsometricCameraPawn not found"), RequestId);
            DoneEvent->Trigger();
            return;
        }

        TSharedPtr<FJsonObject> ResultObj = MakeShareable(new FJsonObject);

        // Location
        FVector Loc = CameraPawn->GetActorLocation();
        TArray<TSharedPtr<FJsonValue>> LocArr;
        LocArr.Add(MakeShareable(new FJsonValueNumber(Loc.X)));
        LocArr.Add(MakeShareable(new FJsonValueNumber(Loc.Y)));
        LocArr.Add(MakeShareable(new FJsonValueNumber(Loc.Z)));
        ResultObj->SetArrayField(TEXT("location"), LocArr);

        // Rotation
        FRotator Rot = CameraPawn->GetActorRotation();
        TArray<TSharedPtr<FJsonValue>> RotArr;
        RotArr.Add(MakeShareable(new FJsonValueNumber(Rot.Pitch)));
        RotArr.Add(MakeShareable(new FJsonValueNumber(Rot.Yaw)));
        RotArr.Add(MakeShareable(new FJsonValueNumber(Rot.Roll)));
        ResultObj->SetArrayField(TEXT("rotation"), RotArr);

        // Zoom (arm length)
        if (USpringArmComponent* Boom = CameraPawn->GetCameraBoom())
        {
            ResultObj->SetNumberField(TEXT("zoom"), Boom->TargetArmLength);
        }

        // FOV
        ResultObj->SetNumberField(TEXT("fov"), CameraPawn->GetCameraFOV());

        // DOF
        ResultObj->SetNumberField(TEXT("dof_focal_distance"), CameraPawn->GetDOFFocalDistance());
        ResultObj->SetNumberField(TEXT("dof_focal_region"), CameraPawn->GetDOFFocalRegion());

        // Post-process
        ResultObj->SetNumberField(TEXT("exposure"), CameraPawn->GetPostProcessExposure());
        ResultObj->SetNumberField(TEXT("bloom"), CameraPawn->GetPostProcessBloom());

        // CineCamera
        ResultObj->SetNumberField(TEXT("focalLength"), CameraPawn->GetCameraFocalLength());
        ResultObj->SetNumberField(TEXT("aperture"), CameraPawn->GetCameraAperture());
        ResultObj->SetNumberField(TEXT("focusDistance"), CameraPawn->GetCameraFocusDistance());

        // Advanced Post-process
        ResultObj->SetNumberField(TEXT("motionBlur"), CameraPawn->GetPostProcessMotionBlur());
        ResultObj->SetNumberField(TEXT("vignette"), CameraPawn->GetPostProcessVignette());
        ResultObj->SetNumberField(TEXT("chromaticAberration"), CameraPawn->GetPostProcessChromaticAberration());

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
        AIsometricCameraPawn* CameraPawn = FindRuntimeCameraPawn(GetPlayWorld());
        if (!CameraPawn)
        {
            ResultStr = BuildErrorResponse(TEXT("IsometricCameraPawn not found"), RequestId);
            DoneEvent->Trigger();
            return;
        }

        CameraPawn->SetCameraTargetFOV(static_cast<float>(FOV));

        TSharedPtr<FJsonObject> ResultObj = MakeShareable(new FJsonObject);
        ResultObj->SetNumberField(TEXT("fov"), CameraPawn->GetCameraFOV());
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
    double FocalRegion = 0.0;
    if (Params->HasField(TEXT("focalRegion")))
    {
        FocalRegion = Params->GetNumberField(TEXT("focalRegion"));
    }

    FEvent* DoneEvent = FPlatformProcess::GetSynchEventFromPool();
    FString ResultStr;

    AsyncTask(ENamedThreads::GameThread, [&]()
    {
        AIsometricCameraPawn* CameraPawn = FindRuntimeCameraPawn(GetPlayWorld());
        if (!CameraPawn)
        {
            ResultStr = BuildErrorResponse(TEXT("IsometricCameraPawn not found"), RequestId);
            DoneEvent->Trigger();
            return;
        }

        CameraPawn->SetCameraTargetDOF(static_cast<float>(FocalDistance), static_cast<float>(FocalRegion));

        TSharedPtr<FJsonObject> ResultObj = MakeShareable(new FJsonObject);
        ResultObj->SetNumberField(TEXT("focal_distance"), CameraPawn->GetDOFFocalDistance());
        ResultObj->SetNumberField(TEXT("focal_region"), CameraPawn->GetDOFFocalRegion());
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
        AIsometricCameraPawn* CameraPawn = FindRuntimeCameraPawn(GetPlayWorld());
        if (!CameraPawn)
        {
            ResultStr = BuildErrorResponse(TEXT("IsometricCameraPawn not found"), RequestId);
            DoneEvent->Trigger();
            return;
        }

        TSharedPtr<FJsonObject> ResultObj = MakeShareable(new FJsonObject);

        if (Params->HasField(TEXT("exposure")))
        {
            double Exposure = Params->GetNumberField(TEXT("exposure"));
            CameraPawn->SetPostProcessExposure(static_cast<float>(Exposure));
            ResultObj->SetNumberField(TEXT("exposure"), CameraPawn->GetPostProcessExposure());
        }

        if (Params->HasField(TEXT("bloom")))
        {
            double Bloom = Params->GetNumberField(TEXT("bloom"));
            CameraPawn->SetPostProcessBloom(static_cast<float>(Bloom));
            ResultObj->SetNumberField(TEXT("bloom"), CameraPawn->GetPostProcessBloom());
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
        AIsometricCameraPawn* CameraPawn = FindRuntimeCameraPawn(GetPlayWorld());
        if (!CameraPawn)
        {
            ResultStr = BuildErrorResponse(TEXT("IsometricCameraPawn not found"), RequestId);
            DoneEvent->Trigger();
            return;
        }

        if (Params->HasField(TEXT("location")))
        {
            const TArray<TSharedPtr<FJsonValue>>& Arr = Params->GetArrayField(TEXT("location"));
            if (Arr.Num() >= 3)
            {
                FVector NewLocation(
                    Arr[0]->AsNumber(),
                    Arr[1]->AsNumber(),
                    Arr[2]->AsNumber()
                );
                CameraPawn->SetCameraTargetLocation(NewLocation);
            }
        }

        if (Params->HasField(TEXT("zoom")))
        {
            double Zoom = Params->GetNumberField(TEXT("zoom"));
            CameraPawn->SetCameraTargetZoom(static_cast<float>(Zoom));
        }

        TSharedPtr<FJsonObject> ResultObj = MakeShareable(new FJsonObject);
        FVector Loc = CameraPawn->GetActorLocation();
        TArray<TSharedPtr<FJsonValue>> LocArr;
        LocArr.Add(MakeShareable(new FJsonValueNumber(Loc.X)));
        LocArr.Add(MakeShareable(new FJsonValueNumber(Loc.Y)));
        LocArr.Add(MakeShareable(new FJsonValueNumber(Loc.Z)));
        ResultObj->SetArrayField(TEXT("location"), LocArr);

        if (USpringArmComponent* Boom = CameraPawn->GetCameraBoom())
        {
            ResultObj->SetNumberField(TEXT("zoom"), Boom->TargetArmLength);
        }

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
        AIsometricCameraPawn* CameraPawn = FindRuntimeCameraPawn(World);
        if (!CameraPawn)
        {
            ResultStr = BuildErrorResponse(TEXT("IsometricCameraPawn not found"), RequestId);
            DoneEvent->Trigger();
            return;
        }

        // Find target actor
        AActor* TargetActor = nullptr;
        for (TActorIterator<AActor> It(World); It; ++It)
        {
            if (It->GetName() == ActorName)
            {
                TargetActor = *It;
                break;
            }
        }

        if (!TargetActor)
        {
            ResultStr = BuildErrorResponse(FString::Printf(TEXT("Actor not found: %s"), *ActorName), RequestId);
            DoneEvent->Trigger();
            return;
        }

        FVector TargetLocation = TargetActor->GetActorLocation();
        CameraPawn->SetCameraTargetLocation(TargetLocation);

        TSharedPtr<FJsonObject> ResultObj = MakeShareable(new FJsonObject);
        ResultObj->SetStringField(TEXT("actor_name"), ActorName);
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
        AIsometricCameraPawn* CameraPawn = FindRuntimeCameraPawn(GetPlayWorld());
        if (!CameraPawn)
        {
            ResultStr = BuildErrorResponse(TEXT("IsometricCameraPawn not found"), RequestId);
            DoneEvent->Trigger();
            return;
        }

        CameraPawn->SetCameraFocalLength(static_cast<float>(FocalLength));

        TSharedPtr<FJsonObject> ResultObj = MakeShareable(new FJsonObject);
        ResultObj->SetNumberField(TEXT("focalLength"), CameraPawn->GetCameraFocalLength());
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
        AIsometricCameraPawn* CameraPawn = FindRuntimeCameraPawn(GetPlayWorld());
        if (!CameraPawn)
        {
            ResultStr = BuildErrorResponse(TEXT("IsometricCameraPawn not found"), RequestId);
            DoneEvent->Trigger();
            return;
        }

        CameraPawn->SetCameraAperture(static_cast<float>(Aperture));

        TSharedPtr<FJsonObject> ResultObj = MakeShareable(new FJsonObject);
        ResultObj->SetNumberField(TEXT("aperture"), CameraPawn->GetCameraAperture());
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
        AIsometricCameraPawn* CameraPawn = FindRuntimeCameraPawn(GetPlayWorld());
        if (!CameraPawn)
        {
            ResultStr = BuildErrorResponse(TEXT("IsometricCameraPawn not found"), RequestId);
            DoneEvent->Trigger();
            return;
        }

        CameraPawn->SetCameraFocusDistance(static_cast<float>(FocusDistance));

        TSharedPtr<FJsonObject> ResultObj = MakeShareable(new FJsonObject);
        ResultObj->SetNumberField(TEXT("focusDistance"), CameraPawn->GetCameraFocusDistance());
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
        AIsometricCameraPawn* CameraPawn = FindRuntimeCameraPawn(World);
        if (!CameraPawn)
        {
            ResultStr = BuildErrorResponse(TEXT("IsometricCameraPawn not found"), RequestId);
            DoneEvent->Trigger();
            return;
        }

        // Find rig actor
        ACameraRigActor* RigActor = nullptr;
        for (TActorIterator<ACameraRigActor> It(World); It; ++It)
        {
            if (It->GetName() == RigName)
            {
                RigActor = *It;
                break;
            }
        }

        if (!RigActor)
        {
            ResultStr = BuildErrorResponse(FString::Printf(TEXT("CameraRigActor not found: %s"), *RigName), RequestId);
            DoneEvent->Trigger();
            return;
        }

        RigActor->StartPlayback(CameraPawn);

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
        ACameraRigActor* RigActor = nullptr;
        for (TActorIterator<ACameraRigActor> It(World); It; ++It)
        {
            if (It->GetName() == RigName)
            {
                RigActor = *It;
                break;
            }
        }

        if (!RigActor)
        {
            ResultStr = BuildErrorResponse(FString::Printf(TEXT("CameraRigActor not found: %s"), *RigName), RequestId);
            DoneEvent->Trigger();
            return;
        }

        RigActor->StopPlayback();

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
        ACameraRigActor* RigActor = nullptr;
        for (TActorIterator<ACameraRigActor> It(World); It; ++It)
        {
            if (It->GetName() == RigName)
            {
                RigActor = *It;
                break;
            }
        }

        if (!RigActor)
        {
            ResultStr = BuildErrorResponse(FString::Printf(TEXT("CameraRigActor not found: %s"), *RigName), RequestId);
            DoneEvent->Trigger();
            return;
        }

        RigActor->SetPlaybackSpeed(static_cast<float>(Speed));

        TSharedPtr<FJsonObject> ResultObj = MakeShareable(new FJsonObject);
        ResultObj->SetStringField(TEXT("rig_name"), RigName);
        ResultObj->SetNumberField(TEXT("speed"), RigActor->GetPlaybackSpeed());
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
        ACameraSwitcher* Switcher = FindCameraSwitcher(GetPlayWorld());
        if (!Switcher)
        {
            ResultStr = BuildErrorResponse(TEXT("CameraSwitcher not found"), RequestId);
            DoneEvent->Trigger();
            return;
        }

        Switcher->SwitchToCamera(CameraName, static_cast<float>(BlendTime));

        TSharedPtr<FJsonObject> ResultObj = MakeShareable(new FJsonObject);
        ResultObj->SetStringField(TEXT("camera_name"), CameraName);
        ResultObj->SetNumberField(TEXT("blend_time"), BlendTime);
        ResultObj->SetStringField(TEXT("current_camera"), Switcher->GetCurrentCameraName());
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
        ACameraSwitcher* Switcher = FindCameraSwitcher(GetPlayWorld());
        if (!Switcher)
        {
            ResultStr = BuildErrorResponse(TEXT("CameraSwitcher not found"), RequestId);
            DoneEvent->Trigger();
            return;
        }

        Switcher->NextCamera(static_cast<float>(BlendTime));

        TSharedPtr<FJsonObject> ResultObj = MakeShareable(new FJsonObject);
        ResultObj->SetStringField(TEXT("current_camera"), Switcher->GetCurrentCameraName());
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
        ACameraSwitcher* Switcher = FindCameraSwitcher(GetPlayWorld());
        if (!Switcher)
        {
            ResultStr = BuildErrorResponse(TEXT("CameraSwitcher not found"), RequestId);
            DoneEvent->Trigger();
            return;
        }

        Switcher->PreviousCamera(static_cast<float>(BlendTime));

        TSharedPtr<FJsonObject> ResultObj = MakeShareable(new FJsonObject);
        ResultObj->SetStringField(TEXT("current_camera"), Switcher->GetCurrentCameraName());
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
        ACameraSwitcher* Switcher = FindCameraSwitcher(GetPlayWorld());
        if (!Switcher)
        {
            ResultStr = BuildErrorResponse(TEXT("CameraSwitcher not found"), RequestId);
            DoneEvent->Trigger();
            return;
        }

        TSharedPtr<FJsonObject> ResultObj = MakeShareable(new FJsonObject);
        TArray<FString> Names = Switcher->GetCameraNames();
        TArray<TSharedPtr<FJsonValue>> CameraArr;
        for (const FString& Name : Names)
        {
            CameraArr.Add(MakeShareable(new FJsonValueString(Name)));
        }
        ResultObj->SetArrayField(TEXT("cameras"), CameraArr);
        ResultObj->SetStringField(TEXT("current_camera"), Switcher->GetCurrentCameraName());
        ResultObj->SetNumberField(TEXT("count"), Switcher->GetCameraCount());
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

    AsyncTask(ENamedThreads::GameThread, [&, Amount]()
    {
        AIsometricCameraPawn* CameraPawn = FindRuntimeCameraPawn(GetPlayWorld());
        if (!CameraPawn)
        {
            ResultStr = BuildErrorResponse(TEXT("IsometricCameraPawn not found"), RequestId);
            DoneEvent->Trigger();
            return;
        }

        CameraPawn->SetPostProcessMotionBlur(static_cast<float>(Amount));

        TSharedPtr<FJsonObject> ResultObj = MakeShareable(new FJsonObject);
        ResultObj->SetNumberField(TEXT("motionBlur"), CameraPawn->GetPostProcessMotionBlur());
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

    AsyncTask(ENamedThreads::GameThread, [&, Intensity]()
    {
        AIsometricCameraPawn* CameraPawn = FindRuntimeCameraPawn(GetPlayWorld());
        if (!CameraPawn)
        {
            ResultStr = BuildErrorResponse(TEXT("IsometricCameraPawn not found"), RequestId);
            DoneEvent->Trigger();
            return;
        }

        CameraPawn->SetPostProcessVignette(static_cast<float>(Intensity));

        TSharedPtr<FJsonObject> ResultObj = MakeShareable(new FJsonObject);
        ResultObj->SetNumberField(TEXT("vignette"), CameraPawn->GetPostProcessVignette());
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

    AsyncTask(ENamedThreads::GameThread, [&, Intensity]()
    {
        AIsometricCameraPawn* CameraPawn = FindRuntimeCameraPawn(GetPlayWorld());
        if (!CameraPawn)
        {
            ResultStr = BuildErrorResponse(TEXT("IsometricCameraPawn not found"), RequestId);
            DoneEvent->Trigger();
            return;
        }

        CameraPawn->SetPostProcessChromaticAberration(static_cast<float>(Intensity));

        TSharedPtr<FJsonObject> ResultObj = MakeShareable(new FJsonObject);
        ResultObj->SetNumberField(TEXT("chromaticAberration"), CameraPawn->GetPostProcessChromaticAberration());
        ResultStr = BuildSuccessResponse(ResultObj, RequestId);
        DoneEvent->Trigger();
    });

    DoneEvent->Wait();
    FPlatformProcess::ReturnSynchEventToPool(DoneEvent);
    return ResultStr;
}

#endif
