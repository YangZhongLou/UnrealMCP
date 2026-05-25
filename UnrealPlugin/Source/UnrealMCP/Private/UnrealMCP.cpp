#include "UnrealMCP.h"
#include "MCPCommandServer.h"

DEFINE_LOG_CATEGORY(LogUnrealMCP);

#define LOCTEXT_NAMESPACE "FUnrealMCPModule"

void FUnrealMCPModule::StartupModule()
{
    UE_LOG(LogUnrealMCP, Log, TEXT("UnrealMCP module starting up..."));

    CommandServer = new FMCPCommandServer();
    if (CommandServer->StartServer(13377))
    {
        UE_LOG(LogUnrealMCP, Log, TEXT("MCP Command Server started on port 13377"));
    }
    else
    {
        UE_LOG(LogUnrealMCP, Error, TEXT("Failed to start MCP Command Server"));
    }
}

void FUnrealMCPModule::ShutdownModule()
{
    UE_LOG(LogUnrealMCP, Log, TEXT("UnrealMCP module shutting down..."));

    if (CommandServer)
    {
        CommandServer->StopServer();
        delete CommandServer;
        CommandServer = nullptr;
    }
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FUnrealMCPModule, UnrealMCP)
