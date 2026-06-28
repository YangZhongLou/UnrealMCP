#if WITH_EDITOR

#include "CoreMinimal.h"

#include "Editor.h"

#include "Selection.h"

#include "EngineUtils.h"

#include "FileHelpers.h"

#include "Framework/Application/SlateApplication.h"

#include "InputCoreTypes.h"

#include "Components/LightComponent.h"

#include "Components/SkyLightComponent.h"

#include "LevelEditor.h"

#include "LevelEditorViewport.h"

#include "IAssetViewport.h"

#include "Slate/SceneViewport.h"

#include "Components/Button.h"

#include "Components/Widget.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Blueprint/WidgetBlueprintLibrary.h"

#include "Dom/JsonObject.h"

#include "Serialization/JsonSerializer.h"

#include "Misc/FileHelper.h"

#include "Misc/Paths.h"

#include "HAL/PlatformFileManager.h"

#include "Engine/Engine.h"

#include "Kismet/GameplayStatics.h"

#include "Camera/PlayerCameraManager.h"
#include "GameFramework/SpringArmComponent.h"

#include "ImageUtils.h"
#include "IImageWrapperModule.h"
#include "IImageWrapper.h"
#include "Modules/ModuleManager.h"

#include "RenderingThread.h"

#include "Components/SceneCaptureComponent2D.h"

#include "Engine/TextureRenderTarget2D.h"

#include "CineCameraComponent.h"

#include "LogCaptureDevice.h"

#include "HAL/IConsoleManager.h"

#include "Framework/Docking/TabManager.h"

#include "LevelEditorSubsystem.h"

#include "Settings/LevelEditorPlaySettings.h"

#include "Async/Async.h"

#include "Misc/PackageName.h"

#include "HAL/FileManager.h"

#include "Containers/Ticker.h"

#include "UnrealMCP.h"

static UWorld* GetActiveWorldForMCP()

{

    if (GEditor && GEditor->IsPlaySessionInProgress())

    {

        for (const FWorldContext& Context : GEngine->GetWorldContexts())

        {

            if (Context.WorldType == EWorldType::PIE)

            {

                return Context.World();

            }

        }

    }

    return GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;

}

FString HandleRunConsoleCommand(const TSharedPtr<FJsonObject>& Params)

{

    FString Command = Params->GetStringField(TEXT("command"));

    FEvent* DoneEvent = FPlatformProcess::GetSynchEventFromPool();

    AsyncTask(ENamedThreads::GameThread, [&]()

    {

        UWorld* World = GetActiveWorldForMCP();

        if (World)

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

            ULevel* CurrentLevel = World->GetCurrentLevel();

            if (CurrentLevel)

            {

                UPackage* LevelPackage = CurrentLevel->GetPackage();

                FString PackageName = LevelPackage ? LevelPackage->GetName() : TEXT("");

                if (FPackageName::IsTempPackage(PackageName))

                {

                    FString DefaultPath = FString::Printf(TEXT("/Game/Maps/AutoSave_%s"),

                        *FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S")));

                    FEditorFileUtils::SaveLevelAs(CurrentLevel, &DefaultPath);

                }

                else

                {

                    FEditorFileUtils::SaveLevel(CurrentLevel);

                }

            }

            bSaved = true;

        }

        DoneEvent->Trigger();

    });

    bool bCompleted = DoneEvent->Wait(5000);

    FPlatformProcess::ReturnSynchEventToPool(DoneEvent);

    if (!bCompleted)

    {

        return TEXT("{\"success\":false,\"error\":\"Save level timed out (5s)\"}");

    }

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

    // Check if level already exists — skip creation and return success

    FString PackagePath = Path.StartsWith(TEXT("/Game/")) ? Path : TEXT("/Game/") + Path;

    FString FilePath = FPackageName::LongPackageNameToFilename(PackagePath, FPackageName::GetMapPackageExtension());

    bool bAlreadyExists = IFileManager::Get().FileExists(*FilePath);

    if (bAlreadyExists)

    {

        TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);

        Result->SetStringField(TEXT("path"), PackagePath);

        Result->SetBoolField(TEXT("created"), false);

        Result->SetBoolField(TEXT("alreadyExists"), true);

        FString ResultStr;

        TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ResultStr);

        FJsonSerializer::Serialize(Result.ToSharedRef(), Writer);

        Writer->Close();

        return FString::Printf(TEXT("{\"success\":true,\"result\":%s}"), *ResultStr);

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

        if (GEditor)

        {

            FRequestPlaySessionParams RequestParams;

            // Force PIE to run inside the active editor viewport, not a separate window.

            ULevelEditorPlaySettings* PlaySettings = NewObject<ULevelEditorPlaySettings>();

            PlaySettings->LastExecutedPlayModeType = PlayMode_InViewPort;

            RequestParams.EditorPlaySettings = PlaySettings;

            // Bind the session to the current level-editing viewport (Selected Viewport).

            FLevelEditorModule& LevelEditorModule = FModuleManager::LoadModuleChecked<FLevelEditorModule>("LevelEditor");

            TSharedPtr<IAssetViewport> ActiveViewport = LevelEditorModule.GetFirstActiveViewport();

            if (ActiveViewport.IsValid())

            {

                RequestParams.DestinationSlateViewport = TWeakPtr<IAssetViewport>(ActiveViewport);

            }

            GEditor->RequestPlaySession(RequestParams);

            bSuccess = true;

        }

        DoneEvent->Trigger();

    });

    DoneEvent->Wait();

    FPlatformProcess::ReturnSynchEventToPool(DoneEvent);

    if (!bSuccess) return TEXT("{\"success\":false,\"error\":\"Editor not available\"}");

    return TEXT("{\"success\":true,\"result\":{\"playing\":true}}");

}

FString HandleStopPlayInEditor(const TSharedPtr<FJsonObject>& Params)

{

    if (!GEditor)

    {

        return TEXT("{\"success\":false,\"error\":\"Editor not available\"}");

    }

    bool bWasPlaying = GEditor->IsPlaySessionInProgress();

    if (bWasPlaying)

    {

        // Delay EndPlayMap to the next engine tick via FTSTicker.

        // Using AsyncTask dispatches into the TaskGraph queue; when EndPlayMap

        // tears down the PIE world it destroys UObjects whose destructors

        // trigger further TaskGraph work while the queue RecursionGuard is

        // already held, causing an assertion failure. FTSTicker runs outside

        // of TaskGraph processing (in FEngineLoop::Tick) so it is safe.

        FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([](float) -> bool

        {

            if (GEditor && GEditor->IsPlaySessionInProgress())

            {

                GEditor->EndPlayMap();

            }

            return false; // Execute once only

        }), 0.0f);

    }

    if (bWasPlaying)

        return TEXT("{\"success\":true,\"result\":{\"stopped\":true}}");

    return TEXT("{\"success\":false,\"error\":\"No active play session\"}");

}

FString HandleTakeScreenshot(const TSharedPtr<FJsonObject>& Params)
{
    FString Filename = Params->HasField(TEXT("filename"))
        ? Params->GetStringField(TEXT("filename"))
        : TEXT("screenshot");

    int32 Width = Params->HasField(TEXT("width")) ? Params->GetIntegerField(TEXT("width")) : 1920;
    int32 Height = Params->HasField(TEXT("height")) ? Params->GetIntegerField(TEXT("height")) : 1080;
    bool bForceSceneCapture = Params->HasField(TEXT("force_scene_capture")) ? Params->GetBoolField(TEXT("force_scene_capture")) : false;

    bool bSuccess = false;
    FString FullPath;
    FEvent* DoneEvent = FPlatformProcess::GetSynchEventFromPool();
    AsyncTask(ENamedThreads::GameThread, [&]()
    {
        FString Directory = Params->HasField(TEXT("directory"))
            ? Params->GetStringField(TEXT("directory"))
            : FPaths::ScreenShotDir();
        FPaths::RemoveDuplicateSlashes(Directory);
        FPaths::NormalizeDirectoryName(Directory);
        if (!Directory.IsEmpty())
        {
            IFileManager::Get().MakeDirectory(*Directory, true);
        }
        FullPath = Directory / (Filename + TEXT(".png"));

        bool bIsPIE = GEditor && GEditor->IsPlaySessionInProgress();

        // Resolve the active viewport. In PIE prefer the PIE world context's viewport;
        // GEngine->GameViewport can point at the editor viewport after a seamless/map
        // travel, which renders the old world or the default sky view.
        FViewport* Viewport = nullptr;
        if (bIsPIE && GEditor)
        {
            if (FWorldContext* PIEContext = GEditor->GetPIEWorldContext())
            {
                if (PIEContext->GameViewport)
                {
                    Viewport = PIEContext->GameViewport->Viewport;
                }
            }
        }
        if (!Viewport && GEngine && GEngine->GameViewport)
        {
            Viewport = GEngine->GameViewport->Viewport;
        }
        if (!Viewport && GEditor)
        {
            Viewport = GEditor->GetActiveViewport();
        }

        // Primary PIE path: synchronously read the game viewport's rendered pixels.
        // This matches exactly what the player sees and avoids scene-capture transform issues.
        // If the caller forces scene capture (e.g. after seamless travel when the viewport can
        // be stale/editor-only), skip this path entirely.
        if (bIsPIE && Viewport && !bSuccess && !bForceSceneCapture)
        {
            // Force a fresh render so we don't read back a stale frame captured before
            // the latest game-state change.
            if (GEditor)
            {
                GEditor->RedrawLevelEditingViewports(true);
            }
            // Draw one PIE frame explicitly; after seamless travel the viewport can be
            // stuck showing the previous world's frame or the default sky.
            Viewport->Draw(true);

            FIntPoint ViewSize = Viewport->GetSizeXY();
            if (ViewSize.X > 0 && ViewSize.Y > 0)
            {
                TArray<FColor> Bitmap;
                if (Viewport->ReadPixels(Bitmap, FReadSurfaceDataFlags()))
                {
                    for (FColor& Pixel : Bitmap)
                    {
                        Pixel.A = 255;
                    }
                    IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>(FName("ImageWrapper"));
                    TSharedPtr<IImageWrapper> ImageWrapper = ImageWrapperModule.CreateImageWrapper(EImageFormat::PNG);
                    if (ImageWrapper.IsValid() && ImageWrapper->SetRaw(Bitmap.GetData(), Bitmap.Num() * sizeof(FColor), ViewSize.X, ViewSize.Y, ERGBFormat::BGRA, 8))
                    {
                        const TArray64<uint8>& Compressed = ImageWrapper->GetCompressed();
                        if (FFileHelper::SaveArrayToFile(Compressed, *FullPath))
                        {
                            bSuccess = true;
                            UE_LOG(LogUnrealMCP, Log, TEXT("[MCP Screenshot] Saved viewport readback to %s (%dx%d)"), *FullPath, ViewSize.X, ViewSize.Y);
                        }
                        else
                        {
                            UE_LOG(LogUnrealMCP, Warning, TEXT("[MCP Screenshot] Failed to save viewport readback to %s"), *FullPath);
                        }
                    }
                    else
                    {
                        UE_LOG(LogUnrealMCP, Warning, TEXT("[MCP Screenshot] Failed to encode viewport readback as PNG"));
                    }
                }
                else
                {
                    UE_LOG(LogUnrealMCP, Warning, TEXT("[MCP Screenshot] Viewport ReadPixels failed"));
                }
            }
        }

        // Resolve the world to spawn the capture camera in (fallback only).
        UWorld* World = nullptr;
        if (bIsPIE && GEditor)
        {
            for (const FWorldContext& Context : GEngine->GetWorldContexts())
            {
                if (Context.WorldType == EWorldType::PIE && Context.World())
                {
                    World = Context.World();
                    break;
                }
            }
        }
        if (!World)
        {
            World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
        }

        // Determine the camera transform for logging / fallback scene capture.
        FVector CameraLocation = FVector::ZeroVector;
        FRotator CameraRotation = FRotator::ZeroRotator;
        float CameraFOV = 90.0f;
        AActor* ViewTarget = nullptr;
        FVector PCMLocation = FVector::ZeroVector;
        FRotator PCMRotation = FRotator::ZeroRotator;
        float PCMFOV = 90.0f;

        if (bIsPIE && World)
        {
            if (APlayerController* PC = UGameplayStatics::GetPlayerController(World, 0))
            {
                ViewTarget = PC->GetViewTarget();

                // Prefer the active camera component transform. The IsometricCameraPawn uses
                // absolute rotation and points the camera at the spring-arm root, so this is the
                // most reliable source for the actual rendered view.
                UCameraComponent* ActiveCamComp = ViewTarget ? ViewTarget->FindComponentByClass<UCameraComponent>() : nullptr;
                if (ActiveCamComp)
                {
                    CameraLocation = ActiveCamComp->GetComponentLocation();
                    CameraRotation = ActiveCamComp->GetComponentRotation();
                    if (UCineCameraComponent* CineCam = Cast<UCineCameraComponent>(ActiveCamComp))
                    {
                        CameraFOV = CineCam->GetHorizontalFieldOfView();
                    }
                    else
                    {
                        CameraFOV = ActiveCamComp->FieldOfView;
                    }
                }

                // Fallback to the PlayerCameraManager if the camera component is unavailable.
                if (CameraLocation.IsNearlyZero())
                {
                    if (APlayerCameraManager* PCM = PC->PlayerCameraManager)
                    {
                        CameraLocation = PCM->GetCameraLocation();
                        CameraRotation = PCM->GetCameraRotation();
                        CameraFOV = PCM->GetFOVAngle();
                    }
                }

                if (APlayerCameraManager* PCM = PC->PlayerCameraManager)
                {
                    PCMLocation = PCM->GetCameraLocation();
                    PCMRotation = PCM->GetCameraRotation();
                    PCMFOV = PCM->GetFOVAngle();
                }
            }
        }

        if (!bIsPIE && CameraLocation.IsNearlyZero() && GEditor)
        {
            if (FViewportClient* ViewportClient = Viewport->GetClient())
            {
                if (FEditorViewportClient* EditorViewportClient = static_cast<FEditorViewportClient*>(ViewportClient))
                {
                    CameraLocation = EditorViewportClient->GetViewLocation();
                    CameraRotation = EditorViewportClient->GetViewRotation();
                    CameraFOV = EditorViewportClient->ViewFOV;
                }
            }
        }

        UE_LOG(LogUnrealMCP, Display, TEXT("[MCP Screenshot] bIsPIE=%d Viewport=%s ViewTarget=%s Capture_Loc=%s Capture_Rot=%s Capture_FOV=%.1f PCM_Loc=%s PCM_Rot=%s PCM_FOV=%.1f"),
            bIsPIE ? 1 : 0,
            Viewport ? TEXT("valid") : TEXT("null"),
            ViewTarget ? *ViewTarget->GetName() : TEXT("None"),
            *CameraLocation.ToString(), *CameraRotation.ToString(), CameraFOV,
            *PCMLocation.ToString(), *PCMRotation.ToString(), PCMFOV);

        // Fallback PIE capture: render a fresh frame via scene capture. For the project's
        // IsometricCameraPawn we use a top-down view centred on the pawn (the automation code
        // moves the pawn over the hex grid), which avoids fighting spring-arm transform
        // bookkeeping and is much faster than FScreenshotRequest.
        if (!bSuccess && bIsPIE && World && ViewTarget)
        {
            // Prefer the PlayerCameraManager transform: it is the authoritative rendered view and
            // is unaffected by stale camera-component bookkeeping after seamless travel.
            FVector CaptureLocation = !PCMLocation.IsNearlyZero() ? PCMLocation : CameraLocation;
            FRotator CaptureRotation = !PCMLocation.IsNearlyZero() ? PCMRotation : CameraRotation;
            float CaptureFOV = PCMFOV > 0.0f ? PCMFOV : CameraFOV;

            // Mirror the source camera's projection mode / ortho width so the scene capture
            // matches the player's view.
            UCameraComponent* ActiveCamComp = ViewTarget->FindComponentByClass<UCameraComponent>();
            ECameraProjectionMode::Type CaptureProjection = ActiveCamComp ? (ECameraProjectionMode::Type)ActiveCamComp->ProjectionMode : ECameraProjectionMode::Perspective;
            float CaptureOrthoWidth = ActiveCamComp ? ActiveCamComp->OrthoWidth : 512.0f;

            if (!CaptureLocation.IsNearlyZero())
            {
                UE_LOG(LogUnrealMCP, Log, TEXT("[MCP Screenshot] Scene capture at %s rot %s fov=%.1f proj=%d ortho=%.1f"), *CaptureLocation.ToString(), *CaptureRotation.ToString(), CaptureFOV, (int32)CaptureProjection, CaptureOrthoWidth);

                FActorSpawnParameters SpawnParams;
                SpawnParams.bNoFail = true;
                SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

                AActor* CaptureActor = World->SpawnActor<AActor>(AActor::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);
                if (CaptureActor)
                {
                    USceneCaptureComponent2D* SceneCapture = NewObject<USceneCaptureComponent2D>(CaptureActor);
                    SceneCapture->RegisterComponent();
                    if (USceneComponent* Root = CaptureActor->GetRootComponent())
                    {
                        SceneCapture->AttachToComponent(Root, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
                    }

                    // Set the capture component's world transform explicitly so it matches the
                    // player's camera. UpdateComponentToWorld() forces the render transform to be
                    // applied before CaptureScene() runs on the same frame.
                    SceneCapture->SetWorldLocation(CaptureLocation);
                    SceneCapture->SetWorldRotation(CaptureRotation);
                    SceneCapture->UpdateComponentToWorld();
                    SceneCapture->ProjectionType = CaptureProjection;
                    SceneCapture->OrthoWidth = CaptureOrthoWidth;
                    SceneCapture->FOVAngle = CaptureFOV;
                    SceneCapture->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;
                    SceneCapture->bCaptureEveryFrame = false;
                    SceneCapture->bCaptureOnMovement = false;
                    SceneCapture->ShowFlags.SetPostProcessing(true);
                    SceneCapture->ShowFlags.SetEyeAdaptation(false);
                    SceneCapture->ShowFlags.SetTonemapper(false);
                    SceneCapture->PostProcessSettings.bOverride_AutoExposureMethod = true;
                    SceneCapture->PostProcessSettings.AutoExposureMethod = EAutoExposureMethod::AEM_Manual;
                    SceneCapture->PostProcessSettings.bOverride_AutoExposureBias = true;
                    SceneCapture->PostProcessSettings.AutoExposureBias = 2.0f;

                    UTextureRenderTarget2D* RenderTarget = NewObject<UTextureRenderTarget2D>(CaptureActor);
                    RenderTarget->InitCustomFormat(Width, Height, PF_B8G8R8A8, false);
                    RenderTarget->UpdateResourceImmediate(true);

                    SceneCapture->TextureTarget = RenderTarget;
                    SceneCapture->CaptureScene();
                    FlushRenderingCommands();

                    FArchive* FileWriter = IFileManager::Get().CreateFileWriter(*FullPath);
                    if (FileWriter)
                    {
                        bSuccess = FImageUtils::ExportRenderTarget2DAsPNG(RenderTarget, *FileWriter);
                        FileWriter->Close();
                        delete FileWriter;
                    }
                    else
                    {
                        UE_LOG(LogUnrealMCP, Warning, TEXT("[MCP Screenshot] Failed to create file writer for %s"), *FullPath);
                    }

                    SceneCapture->UnregisterComponent();
                    CaptureActor->Destroy();
                }
            }
        }

        // Final fallback to UE's screenshot request.
        if (!bSuccess)
        {
            FScreenshotRequest::RequestScreenshot(*FullPath, false, false);
            double Timeout = 20.0;
            double StartTime = FPlatformTime::Seconds();
            while (!IFileManager::Get().FileExists(*FullPath))
            {
                if (FPlatformTime::Seconds() - StartTime > Timeout) break;
                FPlatformProcess::Sleep(0.05f);
            }
            bSuccess = IFileManager::Get().FileExists(*FullPath);
        }

        DoneEvent->Trigger();
    });

    DoneEvent->Wait();
    FPlatformProcess::ReturnSynchEventToPool(DoneEvent);

    TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
    Result->SetStringField(TEXT("path"), FullPath);
    Result->SetBoolField(TEXT("saved"), bSuccess);

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

    // Optional output directory. When omitted, files are written into the project's

    // Source/<Module>/<ClassName> directory (which may trigger a compile prompt).

    FString OutputDir;

    if (Params->HasField(TEXT("output_dir")))

    {

        OutputDir = Params->GetStringField(TEXT("output_dir"));

    }

    FString ProjectDir = FPaths::ProjectDir();

    FString ClassDir = OutputDir.IsEmpty()

        ? FPaths::Combine(ProjectDir, TEXT("Source"), ModuleName, ClassName)

        : FPaths::Combine(OutputDir, ClassName);

    // Derive the correct class-name prefix from the parent class. This makes

    // generated AActor-derived classes compile correctly (UHT requires A* for

    // AActor subclasses). Defaults to A if the parent prefix is unknown.

    TCHAR DesiredPrefix = (ParentClass.Len() > 0) ? ParentClass[0] : TEXT('A');

    if (DesiredPrefix != TEXT('A') && DesiredPrefix != TEXT('U'))

    {

        DesiredPrefix = TEXT('A');

    }

    FString PrefixedClassName = FString::Printf(TEXT("%c%s"), DesiredPrefix, *ClassName);

    // Parent-class include path. These are the canonical headers for the most

    // common bases; fall back to "<ParentClass>.h" for anything else.

    FString ParentInclude;

    if (ParentClass == TEXT("AActor"))

    {

        ParentInclude = TEXT("GameFramework/Actor.h");

    }

    else if (ParentClass == TEXT("UObject"))

    {

        ParentInclude = TEXT("UObject/Object.h");

    }

    else if (ParentClass == TEXT("UActorComponent"))

    {

        ParentInclude = TEXT("Components/ActorComponent.h");

    }

    else if (ParentClass == TEXT("USceneComponent"))

    {

        ParentInclude = TEXT("Components/SceneComponent.h");

    }

    else

    {

        ParentInclude = ParentClass + TEXT(".h");

    }

    // Tick setup appropriate to the inheritance hierarchy.

    FString TickLine;

    if (DesiredPrefix == TEXT('A'))

    {

        TickLine = TEXT("\tPrimaryActorTick.bCanEverTick = true;\n");

    }

    else if (ParentClass.EndsWith(TEXT("Component")))

    {

        TickLine = TEXT("\tPrimaryComponentTick.bCanEverTick = true;\n");

    }

    else

    {

        TickLine = TEXT("");

    }

    IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

    // Suppress any file dialogs while creating directories.

    bool bPrevUnattended = GIsRunningUnattendedScript;

    GIsRunningUnattendedScript = true;

    if (!PlatformFile.DirectoryExists(*ClassDir))

    {

        PlatformFile.CreateDirectoryTree(*ClassDir);

    }

    GIsRunningUnattendedScript = bPrevUnattended;

    FString HeaderContent = FString::Printf(TEXT(

        "#pragma once\n\n"

        "#include \"CoreMinimal.h\"\n"

        "#include \"%s\"\n"

        "#include \"%s.generated.h\"\n\n"

        "UCLASS()\n"

        "class %s_API %s : public %s\n"

        "{\n"

        "\tGENERATED_BODY()\n\n"

        "public:\n"

        "\t%s();\n\n"

        "protected:\n"

        "\tvirtual void BeginPlay() override;\n\n"

        "public:\n"

        "\tvirtual void Tick(float DeltaTime) override;\n"

        "};\n"),

        *ParentInclude, *ClassName,

        *ModuleName.ToUpper(), *PrefixedClassName, *ParentClass,

        *PrefixedClassName);

    FString CppContent = FString::Printf(TEXT(

        "#include \"%s.h\"\n\n"

        "%s::%s()\n"

        "{\n"

        "%s"

        "}\n\n"

        "void %s::BeginPlay()\n"

        "{\n"

        "\tSuper::BeginPlay();\n"

        "}\n\n"

        "void %s::Tick(float DeltaTime)\n"

        "{\n"

        "\tSuper::Tick(DeltaTime);\n"

        "}\n"),

        *ClassName,

        *PrefixedClassName, *PrefixedClassName,

        *TickLine,

        *PrefixedClassName,

        *PrefixedClassName);

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

        // Fall back to any available viewport if the current one is null

        FLevelEditorViewportClient* ViewportClient = GCurrentLevelEditingViewportClient;

        if (!ViewportClient && GEditor)

        {

            const TArray<FLevelEditorViewportClient*>& Clients = GEditor->GetLevelViewportClients();

            for (FLevelEditorViewportClient* Client : Clients)

            {

                if (Client) { ViewportClient = Client; break; }

            }

        }

        if (!ViewportClient)

        {

            ErrorMsg = TEXT("No viewport camera available");

            DoneEvent->Trigger();

            return;

        }

        FVector Location = ViewportClient->GetViewLocation();

        FRotator Rotation = ViewportClient->GetViewRotation();

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

FString HandleSetViewportCamera(const TSharedPtr<FJsonObject>& Params)

{

    if (!GEditor)

    {

        return TEXT("{\"success\":false,\"error\":\"Editor not available\"}");

    }

    FVector TargetLocation = FVector::ZeroVector;

    FRotator TargetRotation = FRotator::ZeroRotator;

    if (Params->HasField(TEXT("location")))

    {

        const TArray<TSharedPtr<FJsonValue>>& Arr = Params->GetArrayField(TEXT("location"));

        if (Arr.Num() >= 3)

        {

            TargetLocation.X = Arr[0]->AsNumber();

            TargetLocation.Y = Arr[1]->AsNumber();

            TargetLocation.Z = Arr[2]->AsNumber();

        }

    }

    if (Params->HasField(TEXT("rotation")))

    {

        const TArray<TSharedPtr<FJsonValue>>& Arr = Params->GetArrayField(TEXT("rotation"));

        if (Arr.Num() >= 3)

        {

            TargetRotation.Pitch = Arr[0]->AsNumber();

            TargetRotation.Yaw   = Arr[1]->AsNumber();

            TargetRotation.Roll  = Arr[2]->AsNumber();

        }

    }

    FString TypeStr = Params->HasField(TEXT("type"))

        ? Params->GetStringField(TEXT("type"))

        : TEXT("");

    const float FOV = Params->HasField(TEXT("fov"))

        ? static_cast<float>(Params->GetNumberField(TEXT("fov")))

        : -1.0f;

    const float OrthoZoom = Params->HasField(TEXT("ortho_zoom"))

        ? static_cast<float>(Params->GetNumberField(TEXT("ortho_zoom")))

        : -1.0f;

    FEvent* DoneEvent = FPlatformProcess::GetSynchEventFromPool();

    AsyncTask(ENamedThreads::GameThread, [&]()

    {

        FLevelEditorViewportClient* ViewportClient = GCurrentLevelEditingViewportClient;

        if (!ViewportClient && GEditor)

        {

            const TArray<FLevelEditorViewportClient*>& Clients = GEditor->GetLevelViewportClients();

            for (FLevelEditorViewportClient* Client : Clients)

            {

                if (Client) { ViewportClient = Client; break; }

            }

        }

        if (ViewportClient)

        {

            if (TypeStr.Equals(TEXT("perspective"), ESearchCase::IgnoreCase))

            {

                ViewportClient->SetViewportType(LVT_Perspective);

            }

            else if (TypeStr.Equals(TEXT("top"), ESearchCase::IgnoreCase))

            {

                ViewportClient->SetViewportType(LVT_OrthoXY);

            }

            else if (TypeStr.Equals(TEXT("front"), ESearchCase::IgnoreCase))

            {

                ViewportClient->SetViewportType(LVT_OrthoXZ);

            }

            else if (TypeStr.Equals(TEXT("side"), ESearchCase::IgnoreCase))

            {

                ViewportClient->SetViewportType(LVT_OrthoYZ);

            }

            ViewportClient->SetViewLocation(TargetLocation);

            ViewportClient->SetViewRotation(TargetRotation);

            if (FOV > 0.0f)

            {

                ViewportClient->ViewFOV = FOV;

            }

            if (OrthoZoom > 0.0f)

            {

                ViewportClient->SetOrthoZoom(OrthoZoom);

            }

            ViewportClient->Invalidate();

        }

        DoneEvent->Trigger();

    });

    DoneEvent->Wait();

    FPlatformProcess::ReturnSynchEventToPool(DoneEvent);

    TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);

    Result->SetStringField(TEXT("location"), FString::Printf(TEXT("[%.1f, %.1f, %.1f]"), TargetLocation.X, TargetLocation.Y, TargetLocation.Z));

    Result->SetStringField(TEXT("rotation"), FString::Printf(TEXT("[%.1f, %.1f, %.1f]"), TargetRotation.Pitch, TargetRotation.Yaw, TargetRotation.Roll));

    Result->SetStringField(TEXT("type"), TypeStr);

    FString ResultStr;

    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ResultStr);

    FJsonSerializer::Serialize(Result.ToSharedRef(), Writer);

    Writer->Close();

    return FString::Printf(TEXT("{\"success\":true,\"result\":%s}"), *ResultStr);

}

FString HandleSetSkyLightParameters(const TSharedPtr<FJsonObject>& Params)

{

    FString ActorName = Params->GetStringField(TEXT("actorName"));

    bool bHasIntensity = Params->HasField(TEXT("intensity"));

    double Intensity = bHasIntensity ? Params->GetNumberField(TEXT("intensity")) : 0;

    bool bHasSourceType = Params->HasField(TEXT("sourceType"));

    int32 SourceType = bHasSourceType ? static_cast<int32>(Params->GetNumberField(TEXT("sourceType"))) : 0;

    bool bHasRealTimeCapture = Params->HasField(TEXT("realTimeCapture"));

    bool bRealTimeCapture = bHasRealTimeCapture ? Params->GetBoolField(TEXT("realTimeCapture")) : false;

    bool bHasLowerHemisphereColor = Params->HasField(TEXT("lowerHemisphereColor"));

    FLinearColor LowerHemisphereColor(FLinearColor::Black);

    if (bHasLowerHemisphereColor)

    {

        const TArray<TSharedPtr<FJsonValue>>& Arr = Params->GetArrayField(TEXT("lowerHemisphereColor"));

        if (Arr.Num() >= 3)

            LowerHemisphereColor = FLinearColor(Arr[0]->AsNumber(), Arr[1]->AsNumber(), Arr[2]->AsNumber());

    }

    FLinearColor LightColor(FLinearColor::White);

    if (Params->HasField(TEXT("color")))

    {

        const TArray<TSharedPtr<FJsonValue>>& Arr = Params->GetArrayField(TEXT("color"));

        if (Arr.Num() >= 3)

            LightColor = FLinearColor(Arr[0]->AsNumber(), Arr[1]->AsNumber(), Arr[2]->AsNumber());

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

        USkyLightComponent* SkyComp = Actor->FindComponentByClass<USkyLightComponent>();

        if (!SkyComp) { ErrorMsg = TEXT("No SkyLightComponent found on actor"); DoneEvent->Trigger(); return; }

        if (bHasSourceType) SkyComp->SourceType = static_cast<ESkyLightSourceType>(SourceType);

        if (bHasRealTimeCapture) SkyComp->bRealTimeCapture = bRealTimeCapture;

        if (bHasIntensity) SkyComp->SetIntensity(Intensity);

        SkyComp->SetLightColor(LightColor);

        if (bHasLowerHemisphereColor) SkyComp->LowerHemisphereColor = LowerHemisphereColor;

        DoneEvent->Trigger();

    });

    DoneEvent->Wait();

    FPlatformProcess::ReturnSynchEventToPool(DoneEvent);

    if (!ErrorMsg.IsEmpty())

        return FString::Printf(TEXT("{\"success\":false,\"error\":\"%s\"}"), *ErrorMsg);

    return FString::Printf(TEXT("{\"success\":true,\"result\":{\"actor\":\"%s\"}}"), *ActorName);

}

FString HandleSimulateInputKey(const TSharedPtr<FJsonObject>& Params)

{

    FString KeyName;

    if (Params->HasField(TEXT("key")))

    {

        KeyName = Params->GetStringField(TEXT("key"));

    }

    if (KeyName.IsEmpty())

    {

        return TEXT("{\"success\":false,\"error\":\"Missing 'key' parameter\"}");

    }



    bool bSuccess = false;

    FEvent* DoneEvent = FPlatformProcess::GetSynchEventFromPool();

    AsyncTask(ENamedThreads::GameThread, [&]()

    {

        FKey Key(*KeyName);

        if (!Key.IsValid())

        {

            if (KeyName.Equals(TEXT("Space"), ESearchCase::IgnoreCase) || KeyName.Equals(TEXT("SpaceBar"), ESearchCase::IgnoreCase))

            {

                Key = EKeys::SpaceBar;

            }

            else if (KeyName.Equals(TEXT("Enter"), ESearchCase::IgnoreCase) || KeyName.Equals(TEXT("Return"), ESearchCase::IgnoreCase))

            {

                Key = EKeys::Enter;

            }

            else if (KeyName.Equals(TEXT("Escape"), ESearchCase::IgnoreCase) || KeyName.Equals(TEXT("Esc"), ESearchCase::IgnoreCase))

            {

                Key = EKeys::Escape;

            }

        }



        if (Key.IsValid())

        {

            FModifierKeysState ModifierKeys;

            FKeyEvent KeyDownEvent(Key, ModifierKeys, 0, false, 0, 0);

            FSlateApplication::Get().ProcessKeyDownEvent(KeyDownEvent);

            FKeyEvent KeyUpEvent(Key, ModifierKeys, 0, false, 0, 0);

            FSlateApplication::Get().ProcessKeyUpEvent(KeyUpEvent);

            bSuccess = true;

        }

        DoneEvent->Trigger();

    });

    DoneEvent->Wait();

    FPlatformProcess::ReturnSynchEventToPool(DoneEvent);



    if (!bSuccess)

    {

        return FString::Printf(TEXT("{\"success\":false,\"error\":\"Invalid key: %s\"}"), *KeyName);

    }

    return TEXT("{\"success\":true,\"result\":{\"simulated\":true}}");

}

FString HandleClickWidget(const TSharedPtr<FJsonObject>& Params)

{

    FString WidgetName;

    if (Params->HasField(TEXT("name")))

    {

        WidgetName = Params->GetStringField(TEXT("name"));

    }

    if (WidgetName.IsEmpty())

    {

        return TEXT("{\"success\":false,\"error\":\"Missing 'name' parameter\"}");

    }



    bool bClicked = false;

    FString ErrorMsg;

    FEvent* DoneEvent = FPlatformProcess::GetSynchEventFromPool();

    AsyncTask(ENamedThreads::GameThread, [&]()

    {

        UWorld* TargetWorld = nullptr;

        if (GEditor)

        {

            FWorldContext* PIEContext = GEditor->GetPIEWorldContext();

            TargetWorld = PIEContext ? PIEContext->World() : nullptr;

        }

        if (!TargetWorld && GEngine && GEngine->GameViewport)

        {

            TargetWorld = GEngine->GameViewport->GetWorld();

        }

        if (!TargetWorld)

        {

            ErrorMsg = TEXT("No active play world");

            DoneEvent->Trigger();

            return;

        }



        // Search only the active user-widget trees for the named widget. Iterating
        // all UWidget UObjects can be extremely slow and block the game thread long
        // enough for the MCP connection to time out.
        TArray<UUserWidget*> AllUserWidgets;
        UWidgetBlueprintLibrary::GetAllWidgetsOfClass(TargetWorld, AllUserWidgets, UUserWidget::StaticClass(), false);
        for (UUserWidget* UserWidget : AllUserWidgets)
        {
            if (!UserWidget || !UserWidget->WidgetTree)
            {
                continue;
            }
            if (UWidget* Found = UserWidget->WidgetTree->FindWidget(FName(*WidgetName)))
            {
                if (UButton* Button = Cast<UButton>(Found))
                {
                    Button->OnClicked.Broadcast();
                    bClicked = true;
                }
                break;
            }
        }

        if (!bClicked && ErrorMsg.IsEmpty())
        {
            ErrorMsg = FString::Printf(TEXT("Widget not found or not clickable: %s"), *WidgetName);
        }

        DoneEvent->Trigger();

    });

    DoneEvent->Wait();

    FPlatformProcess::ReturnSynchEventToPool(DoneEvent);



    if (!bClicked)

    {

        return FString::Printf(TEXT("{\"success\":false,\"error\":\"%s\"}"), *ErrorMsg);

    }

    return TEXT("{\"success\":true,\"result\":{\"clicked\":true}}");

}

#endif