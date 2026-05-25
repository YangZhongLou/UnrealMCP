#pragma once

#include "CoreMinimal.h"
#include "HAL/Runnable.h"
#include "HAL/RunnableThread.h"
#include "Sockets.h"
#include "SocketSubsystem.h"

class FMCPCommandServer : public FRunnable
{
public:
    FMCPCommandServer();
    virtual ~FMCPCommandServer();

    bool StartServer(int32 Port = 13377);
    void StopServer();

    virtual bool Init() override;
    virtual uint32 Run() override;
    virtual void Stop() override;
    virtual void Exit() override;

private:
    void HandleClientConnection(FSocket* ClientSocket);
    FString ProcessCommand(const FString& JsonRequest);

    FRunnableThread* Thread;
    FSocket* ListenSocket;
    TSharedPtr<FInternetAddr> ListenAddr;
    FThreadSafeBool bRunning;
    int32 ServerPort;
};
