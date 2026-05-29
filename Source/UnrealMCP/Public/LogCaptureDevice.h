#pragma once

#include "CoreMinimal.h"
#include "Misc/OutputDevice.h"

struct FLogEntry
{
    FString Message;
    FString Category;
    ELogVerbosity::Type Verbosity;
    FDateTime Timestamp;
};

class FLogCaptureDevice : public FOutputDevice
{
public:
    static FLogCaptureDevice& Get();

    void Start();
    void Stop();
    bool IsRunning() const { return bRunning; }

    virtual void Serialize(const TCHAR* V, ELogVerbosity::Type Verbosity, const FName& Category) override;

    void GetLogs(int32 Count, const FString& MinVerbosity, TArray<FLogEntry>& OutLogs, bool bClearAfter = false);
    int32 GetBufferSize() const { return LogBuffer.Num(); }

    static ELogVerbosity::Type ParseVerbosity(const FString& VerbosityStr);

private:
    FLogCaptureDevice();
    ~FLogCaptureDevice();

    static const int32 MaxBufferSize = 1000;
    TArray<FLogEntry> LogBuffer;
    int32 HeadIndex = 0;
    int32 EntryCount = 0;
    bool bRunning = false;
    FCriticalSection CriticalSection;
};
