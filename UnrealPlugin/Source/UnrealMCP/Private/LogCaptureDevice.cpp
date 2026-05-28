#include "LogCaptureDevice.h"

FLogCaptureDevice& FLogCaptureDevice::Get()
{
    static FLogCaptureDevice Instance;
    return Instance;
}

FLogCaptureDevice::FLogCaptureDevice()
{
    LogBuffer.SetNum(MaxBufferSize);
}

FLogCaptureDevice::~FLogCaptureDevice()
{
    Stop();
}

void FLogCaptureDevice::Start()
{
    if (bRunning) return;

    FScopeLock Lock(&CriticalSection);
    bRunning = true;
    GLog->AddOutputDevice(this);
    UE_LOG(LogTemp, Log, TEXT("LogCaptureDevice started"));
}

void FLogCaptureDevice::Stop()
{
    if (!bRunning) return;

    if (GLog)
    {
        GLog->RemoveOutputDevice(this);
    }
    bRunning = false;
    UE_LOG(LogTemp, Log, TEXT("LogCaptureDevice stopped"));
}

void FLogCaptureDevice::Serialize(const TCHAR* V, ELogVerbosity::Type Verbosity, const FName& Category)
{
    if (!bRunning) return;

    FScopeLock Lock(&CriticalSection);

    FLogEntry& Entry = LogBuffer[HeadIndex];
    Entry.Message = FString(V).TrimEnd();
    Entry.Category = Category.ToString();
    Entry.Verbosity = Verbosity;
    Entry.Timestamp = FDateTime::Now();

    HeadIndex = (HeadIndex + 1) % MaxBufferSize;
    if (EntryCount < MaxBufferSize)
    {
        EntryCount++;
    }
}

void FLogCaptureDevice::GetLogs(int32 Count, const FString& MinVerbosity, TArray<FLogEntry>& OutLogs, bool bClearAfter)
{
    FScopeLock Lock(&CriticalSection);

    ELogVerbosity::Type MinLevel = ParseVerbosity(MinVerbosity);

    OutLogs.Empty();
    if (EntryCount == 0) return;

    // Read from oldest to newest; oldest is at (HeadIndex - EntryCount) wrapped.
    int32 StartIndex = (HeadIndex - EntryCount + MaxBufferSize) % MaxBufferSize;

    for (int32 i = 0; i < EntryCount && OutLogs.Num() < Count; i++)
    {
        int32 Idx = (StartIndex + i) % MaxBufferSize;
        const FLogEntry& Entry = LogBuffer[Idx];

        if (Entry.Verbosity <= MinLevel)
        {
            OutLogs.Add(Entry);
        }
    }

    if (bClearAfter)
    {
        EntryCount = 0;
        HeadIndex = 0;
    }
}

ELogVerbosity::Type FLogCaptureDevice::ParseVerbosity(const FString& VerbosityStr)
{
    if (VerbosityStr.Equals(TEXT("Error"), ESearchCase::IgnoreCase))
        return ELogVerbosity::Error;
    if (VerbosityStr.Equals(TEXT("Warning"), ESearchCase::IgnoreCase))
        return ELogVerbosity::Warning;
    if (VerbosityStr.Equals(TEXT("Log"), ESearchCase::IgnoreCase))
        return ELogVerbosity::Log;
    if (VerbosityStr.Equals(TEXT("Verbose"), ESearchCase::IgnoreCase))
        return ELogVerbosity::Verbose;
    if (VerbosityStr.Equals(TEXT("VeryVerbose"), ESearchCase::IgnoreCase))
        return ELogVerbosity::VeryVerbose;
    return ELogVerbosity::Log; // default
}
