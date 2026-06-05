#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class FMcpJsonRpcServer;
class FMcpProtocolAdapter;
class FMCPCommandServer;
class ACameraRig_Rail;
class AActor;

DECLARE_LOG_CATEGORY_EXTERN(LogUnrealMCP, Log, All);

/** Playback state for a camera rig rail (used by MCP camera-rig tools). */
struct FCameraRigPlayback
{
    TWeakObjectPtr<ACameraRig_Rail> Rail;
    TWeakObjectPtr<AActor> AttachedActor;
    float Speed = 100.0f;
    bool bIsPlaying = false;
};

/** Active rig playbacks updated every tick. */
extern TArray<FCameraRigPlayback> GActiveCameraRigPlaybacks;

/** Tick update for camera-rig playback (called from module tick). */
void UpdateCameraRigPlaybacks(float DeltaTime);

class UNREALMCP_API FUnrealMCPModule : public IModuleInterface
{
public:
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;

private:
    FMcpJsonRpcServer* JsonRpcServer;
    TSharedPtr<FMcpProtocolAdapter> ProtocolAdapter;
#if WITH_EDITOR
    FMCPCommandServer* CommandServer;
#endif
    FTSTicker::FDelegateHandle RigTickerHandle;
};
