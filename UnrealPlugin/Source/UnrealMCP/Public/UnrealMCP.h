#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class FMcpJsonRpcServer;
class FMcpProtocolAdapter;

DECLARE_LOG_CATEGORY_EXTERN(LogUnrealMCP, Log, All);

class UNREALMCP_API FUnrealMCPModule : public IModuleInterface
{
public:
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;

private:
    FMcpJsonRpcServer* JsonRpcServer;
    TSharedPtr<FMcpProtocolAdapter> ProtocolAdapter;
};
