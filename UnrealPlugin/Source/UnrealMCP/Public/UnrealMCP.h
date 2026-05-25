#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class FUnrealMCPModule : public IModuleInterface
{
public:
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;

private:
    class FMCPCommandServer* CommandServer;
};
