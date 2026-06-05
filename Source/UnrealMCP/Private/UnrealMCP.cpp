#include "UnrealMCP.h"
#include "Core/McpJsonRpcServer.h"
#include "Core/McpProtocolAdapter.h"
#include "MCPCommandServer.h"
#include "LogCaptureDevice.h"

DEFINE_LOG_CATEGORY(LogUnrealMCP);

#define LOCTEXT_NAMESPACE "FUnrealMCPModule"

static int32 GetConfigInt(const TCHAR* Section, const TCHAR* Key, int32 DefaultValue)
{
    int32 Value = DefaultValue;
    if (GConfig)
    {
        GConfig->GetInt(Section, Key, Value, GEngineIni);
    }
    return Value;
}

void FUnrealMCPModule::StartupModule()
{
    UE_LOG(LogUnrealMCP, Log, TEXT("UnrealMCP module starting up..."));

    const int32 JsonRpcPort = GetConfigInt(TEXT("UnrealMCP"), TEXT("JsonRpcServerPort"), 13379);
    const int32 CommandPort = GetConfigInt(TEXT("UnrealMCP"), TEXT("CommandServerPort"), 13377);

    JsonRpcServer = new FMcpJsonRpcServer();

    ProtocolAdapter = MakeShareable(new FMcpProtocolAdapter());
    JsonRpcServer->SetProtocolAdapter(ProtocolAdapter);

    if (JsonRpcServer->StartServer(JsonRpcPort))
    {
        UE_LOG(LogUnrealMCP, Log, TEXT("MCP JSON-RPC Server started on port %d"), JsonRpcPort);
    }
    else
    {
        UE_LOG(LogUnrealMCP, Error, TEXT("Failed to start MCP JSON-RPC Server"));
    }

#if WITH_EDITOR
    CommandServer = new FMCPCommandServer();
    if (CommandServer->StartServer(CommandPort))
    {
        UE_LOG(LogUnrealMCP, Log, TEXT("MCP Command Server started on port %d"), CommandPort);
    }
    else
    {
        UE_LOG(LogUnrealMCP, Error, TEXT("Failed to start MCP Command Server"));
    }
#endif

    FLogCaptureDevice::Get().Start();
}

void FUnrealMCPModule::ShutdownModule()
{
    UE_LOG(LogUnrealMCP, Log, TEXT("UnrealMCP module shutting down..."));

    FLogCaptureDevice::Get().Stop();

#if WITH_EDITOR
    if (CommandServer)
    {
        CommandServer->StopServer();
        delete CommandServer;
        CommandServer = nullptr;
    }
#endif

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
