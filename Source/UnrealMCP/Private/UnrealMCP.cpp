#include "UnrealMCP.h"
#include "Core/McpJsonRpcServer.h"
#include "Core/McpProtocolAdapter.h"
#include "LogCaptureDevice.h"

DEFINE_LOG_CATEGORY(LogUnrealMCP);

#define LOCTEXT_NAMESPACE "FUnrealMCPModule"

void FUnrealMCPModule::StartupModule()
{
    UE_LOG(LogUnrealMCP, Log, TEXT("UnrealMCP module starting up..."));

    JsonRpcServer = new FMcpJsonRpcServer();

    ProtocolAdapter = MakeShareable(new FMcpProtocolAdapter());
    JsonRpcServer->SetProtocolAdapter(ProtocolAdapter);

    if (JsonRpcServer->StartServer(13377))
    {
        UE_LOG(LogUnrealMCP, Log, TEXT("MCP JSON-RPC Server started on port 13377"));
    }
    else
    {
        UE_LOG(LogUnrealMCP, Error, TEXT("Failed to start MCP JSON-RPC Server"));
    }

    FLogCaptureDevice::Get().Start();
}

void FUnrealMCPModule::ShutdownModule()
{
    UE_LOG(LogUnrealMCP, Log, TEXT("UnrealMCP module shutting down..."));

    FLogCaptureDevice::Get().Stop();

    if (JsonRpcServer)
    {
        JsonRpcServer->StopServer();
        delete JsonRpcServer;
        JsonRpcServer = nullptr;
    }

    ProtocolAdapter.Reset();
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FUnrealMCPModule, UnrealMCP)
