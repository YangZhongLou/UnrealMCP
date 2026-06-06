#pragma once

#include "CoreMinimal.h"
#include "HAL/Runnable.h"
#include "HAL/RunnableThread.h"
#include "Sockets.h"
#include "SocketSubsystem.h"

class FMcpProtocolAdapter;

/** JSON-RPC 2.0 Request / Notification 结构 */
struct UNREALMCP_API FMcpJsonRpcMessage
{
    FString JsonRpc;
    TOptional<int32> Id;
    FString Method;
    TSharedPtr<FJsonObject> Params;
    bool bIsNotification;
};

/** TCP 服务端，处理 Length-Prefixed JSON-RPC 帧 */
class UNREALMCP_API FMcpJsonRpcServer : public FRunnable
{
public:
    FMcpJsonRpcServer();
    virtual ~FMcpJsonRpcServer();

    bool StartServer(int32 Port = 13377);
    void StopServer();

    void SetProtocolAdapter(TSharedPtr<FMcpProtocolAdapter> InAdapter);

    virtual bool Init() override;
    virtual uint32 Run() override;
    virtual void Stop() override;
    virtual void Exit() override;

private:
    void HandleClientConnection(FSocket* ClientSocket);
    bool ReadFrame(FSocket* ClientSocket, TArray<uint8>& OutFrame);
    bool ParseMessage(const TArray<uint8>& Frame, FMcpJsonRpcMessage& OutMessage);
    void SendResponse(FSocket* ClientSocket, const TSharedPtr<FJsonObject>& Response);
    void SendError(FSocket* ClientSocket, const TOptional<int32>& Id, int32 Code, const FString& Message);

    FRunnableThread* Thread;
    FSocket* ListenSocket;
    TSharedPtr<FInternetAddr> ListenAddr;
    FThreadSafeBool bRunning;
    int32 ServerPort;

    TSharedPtr<FMcpProtocolAdapter> ProtocolAdapter;
    FCriticalSection AdapterLock;
};
