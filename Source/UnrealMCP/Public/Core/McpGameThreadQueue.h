#pragma once

#include "CoreMinimal.h"
#include "Tickable.h"

/** Game Thread 委托队列 — 将 TCP 线程的请求安全地调度到 Game Thread */
class UNREALMCP_API FMcpGameThreadQueue : public FTickableGameObject
{
public:
    FMcpGameThreadQueue();
    virtual ~FMcpGameThreadQueue();

    /** 从任意线程入队一个任务 */
    void Enqueue(TFunction<void()> Task);

    // FTickableGameObject interface
    virtual void Tick(float DeltaTime) override;
    virtual bool IsTickable() const override { return true; }
    virtual TStatId GetStatId() const override;
    virtual bool IsTickableWhenPaused() const override { return true; }

private:
    FCriticalSection QueueLock;
    TArray<TFunction<void()>> PendingTasks;
};
