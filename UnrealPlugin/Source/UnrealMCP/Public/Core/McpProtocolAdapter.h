#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

struct FMcpJsonRpcMessage;

/** MCP 2024-11-05 协议适配器 */
class UNREALMCP_API FMcpProtocolAdapter
{
public:
    FMcpProtocolAdapter();

    /** 处理 JSON-RPC Request，返回 Response（nullptr 表示 Notification 无响应） */
    TSharedPtr<FJsonObject> ProcessRequest(const FMcpJsonRpcMessage& Request);

    /** 检查会话是否已完成 initialize */
    bool IsInitialized() const { return bInitialized; }

private:
    TSharedPtr<FJsonObject> HandleInitialize(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleToolsList(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleToolsCall(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleResourcesList(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleResourcesRead(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandlePing(const TSharedPtr<FJsonObject>& Params);

    TSharedPtr<FJsonObject> MakeResponse(const TOptional<int32>& Id, const TSharedPtr<FJsonObject>& Result);
    TSharedPtr<FJsonObject> MakeErrorResponse(const TOptional<int32>& Id, int32 Code, const FString& Message);

    void RegisterTools();
    void RegisterResources();

    bool bInitialized;
    FString NegotiatedProtocolVersion;

    TArray<TSharedPtr<FJsonObject>> ToolList;
    TArray<TSharedPtr<FJsonObject>> ResourceList;
};
