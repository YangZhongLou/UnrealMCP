#include "Core/McpGameThreadQueue.h"

FMcpGameThreadQueue::FMcpGameThreadQueue()
{
}

FMcpGameThreadQueue::~FMcpGameThreadQueue()
{
}

void FMcpGameThreadQueue::Enqueue(TFunction<void()> Task)
{
    FScopeLock Lock(&QueueLock);
    PendingTasks.Add(MoveTemp(Task));
}

void FMcpGameThreadQueue::Tick(float DeltaTime)
{
    TArray<TFunction<void()>> TasksToExecute;

    {
        FScopeLock Lock(&QueueLock);
        TasksToExecute = MoveTemp(PendingTasks);
        PendingTasks.Empty();
    }

    for (TFunction<void()>& Task : TasksToExecute)
    {
        if (Task)
        {
            Task();
        }
    }
}

TStatId FMcpGameThreadQueue::GetStatId() const
{
    RETURN_QUICK_DECLARE_CYCLE_STAT(FMcpGameThreadQueue, STATGROUP_Tickables);
}
