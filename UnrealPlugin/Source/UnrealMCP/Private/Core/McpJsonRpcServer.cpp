#include "Core/McpJsonRpcServer.h"
#include "Core/McpProtocolAdapter.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

DEFINE_LOG_CATEGORY_STATIC(LogMcpJsonRpcServer, Log, All);

FMcpJsonRpcServer::FMcpJsonRpcServer()
    : Thread(nullptr)
    , ListenSocket(nullptr)
    , bRunning(false)
    , ServerPort(13377)
{
}

FMcpJsonRpcServer::~FMcpJsonRpcServer()
{
    StopServer();
}

bool FMcpJsonRpcServer::StartServer(int32 Port)
{
    ServerPort = Port;
    bRunning = true;

    Thread = FRunnableThread::Create(this, TEXT("McpJsonRpcServer"), 0, TPri_Normal);
    return Thread != nullptr;
}

void FMcpJsonRpcServer::StopServer()
{
    bRunning = false;

    if (ListenSocket)
    {
        ListenSocket->Close();
    }

    if (Thread)
    {
        Thread->Kill(true);
        delete Thread;
        Thread = nullptr;
    }

    if (ListenSocket)
    {
        ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(ListenSocket);
        ListenSocket = nullptr;
    }
}

void FMcpJsonRpcServer::SetProtocolAdapter(TSharedPtr<FMcpProtocolAdapter> InAdapter)
{
    FScopeLock Lock(&AdapterLock);
    ProtocolAdapter = InAdapter;
}

bool FMcpJsonRpcServer::Init()
{
    ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
    ListenSocket = SocketSubsystem->CreateSocket(NAME_Stream, TEXT("McpJsonRpcListenSocket"), false);

    if (!ListenSocket)
    {
        UE_LOG(LogMcpJsonRpcServer, Error, TEXT("Failed to create listen socket"));
        return false;
    }

    ListenAddr = SocketSubsystem->CreateInternetAddr();
    ListenAddr->SetLoopbackAddress();
    ListenAddr->SetPort(ServerPort);

    ListenSocket->SetReuseAddr(true);
    ListenSocket->SetNonBlocking(false);
    int32 NewSize = 0;
    ListenSocket->SetReceiveBufferSize(65536, NewSize);
    ListenSocket->SetSendBufferSize(65536, NewSize);

    if (!ListenSocket->Bind(*ListenAddr))
    {
        UE_LOG(LogMcpJsonRpcServer, Error, TEXT("Failed to bind socket to port %d"), ServerPort);
        return false;
    }

    if (!ListenSocket->Listen(4))
    {
        UE_LOG(LogMcpJsonRpcServer, Error, TEXT("Failed to listen on socket"));
        return false;
    }

    UE_LOG(LogMcpJsonRpcServer, Log, TEXT("MCP JSON-RPC Server listening on 127.0.0.1:%d"), ServerPort);
    return true;
}

uint32 FMcpJsonRpcServer::Run()
{
    while (bRunning)
    {
        if (!ListenSocket)
        {
            FPlatformProcess::Sleep(0.1f);
            continue;
        }

        bool bHasPendingConnection = false;
        ListenSocket->HasPendingConnection(bHasPendingConnection);

        if (bHasPendingConnection)
        {
            FSocket* ClientSocket = ListenSocket->Accept(TEXT("McpJsonRpcClient"));
            if (ClientSocket)
            {
                UE_LOG(LogMcpJsonRpcServer, Log, TEXT("MCP client connected"));
                HandleClientConnection(ClientSocket);
                ClientSocket->Close();
                ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(ClientSocket);
                UE_LOG(LogMcpJsonRpcServer, Log, TEXT("MCP client disconnected"));
            }
        }

        FPlatformProcess::Sleep(0.01f);
    }

    return 0;
}

void FMcpJsonRpcServer::Stop()
{
    bRunning = false;
}

void FMcpJsonRpcServer::Exit()
{
}

void FMcpJsonRpcServer::HandleClientConnection(FSocket* ClientSocket)
{
    ClientSocket->SetNonBlocking(false);

    while (bRunning)
    {
        TArray<uint8> Frame;
        if (!ReadFrame(ClientSocket, Frame))
        {
            break;
        }

        FMcpJsonRpcMessage Message;
        if (!ParseMessage(Frame, Message))
        {
            SendError(ClientSocket, TOptional<int32>(), -32700, TEXT("Parse error"));
            continue;
        }

        if (Message.bIsNotification)
        {
            FScopeLock Lock(&AdapterLock);
            if (ProtocolAdapter.IsValid())
            {
                ProtocolAdapter->ProcessRequest(Message);
            }
            continue;
        }

        TSharedPtr<FJsonObject> Response;
        {
            FScopeLock Lock(&AdapterLock);
            if (ProtocolAdapter.IsValid())
            {
                Response = ProtocolAdapter->ProcessRequest(Message);
            }
        }

        if (Response.IsValid())
        {
            SendResponse(ClientSocket, Response);
        }
        else
        {
            SendError(ClientSocket, Message.Id, -32603, TEXT("Internal error"));
        }
    }
}

bool FMcpJsonRpcServer::ReadFrame(FSocket* ClientSocket, TArray<uint8>& OutFrame)
{
    // Read 4-byte length prefix (big-endian)
    uint8 LengthPrefix[4];
    int32 BytesRead = 0;
    int32 TotalRead = 0;

    while (TotalRead < 4)
    {
        if (!ClientSocket->Recv(LengthPrefix + TotalRead, 4 - TotalRead, BytesRead))
        {
            return false;
        }
        if (BytesRead == 0)
        {
            FPlatformProcess::Sleep(0.001f);
            continue;
        }
        TotalRead += BytesRead;
    }

    const uint32 PayloadLength =
        (static_cast<uint32>(LengthPrefix[0]) << 24) |
        (static_cast<uint32>(LengthPrefix[1]) << 16) |
        (static_cast<uint32>(LengthPrefix[2]) << 8) |
        static_cast<uint32>(LengthPrefix[3]);

    if (PayloadLength == 0)
    {
        OutFrame.Empty();
        return true;
    }

    if (PayloadLength > 16 * 1024 * 1024) // 16MB max frame
    {
        UE_LOG(LogMcpJsonRpcServer, Error, TEXT("Frame too large: %u bytes"), PayloadLength);
        return false;
    }

    OutFrame.SetNumUninitialized(PayloadLength);
    TotalRead = 0;

    while (TotalRead < static_cast<int32>(PayloadLength))
    {
        if (!ClientSocket->Recv(OutFrame.GetData() + TotalRead, PayloadLength - TotalRead, BytesRead))
        {
            return false;
        }
        if (BytesRead == 0)
        {
            FPlatformProcess::Sleep(0.001f);
            continue;
        }
        TotalRead += BytesRead;
    }

    return true;
}

bool FMcpJsonRpcServer::ParseMessage(const TArray<uint8>& Frame, FMcpJsonRpcMessage& OutMessage)
{
    const FString JsonStr = FString(UTF8_TO_TCHAR(reinterpret_cast<const char*>(Frame.GetData())));

    TSharedPtr<FJsonObject> JsonObj;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonStr);

    if (!FJsonSerializer::Deserialize(Reader, JsonObj) || !JsonObj.IsValid())
    {
        return false;
    }

    OutMessage.JsonRpc = JsonObj->GetStringField(TEXT("jsonrpc"));
    if (OutMessage.JsonRpc != TEXT("2.0"))
    {
        return false;
    }

    // id is optional (notifications have no id)
    if (JsonObj->HasField(TEXT("id")))
    {
        const TSharedPtr<FJsonValue>* IdField = JsonObj->Values.Find(TEXT("id"));
        if (IdField && IdField->IsValid())
        {
            if ((*IdField)->Type == EJson::Number)
            {
                OutMessage.Id = static_cast<int32>((*IdField)->AsNumber());
            }
            else if ((*IdField)->Type == EJson::String)
            {
                // String IDs are valid in JSON-RPC but we map them to hash for simplicity
                OutMessage.Id = FCString::Atoi(*(*IdField)->AsString());
            }
        }
        OutMessage.bIsNotification = false;
    }
    else
    {
        OutMessage.bIsNotification = true;
    }

    OutMessage.Method = JsonObj->GetStringField(TEXT("method"));
    OutMessage.Params = JsonObj->GetObjectField(TEXT("params"));

    return true;
}

void FMcpJsonRpcServer::SendResponse(FSocket* ClientSocket, const TSharedPtr<FJsonObject>& Response)
{
    FString ResponseStr;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ResponseStr);
    FJsonSerializer::Serialize(Response.ToSharedRef(), Writer);

    FTCHARToUTF8 UTF8Response(*ResponseStr);
    const uint32 PayloadLength = UTF8Response.Length();

    uint8 LengthPrefix[4];
    LengthPrefix[0] = static_cast<uint8>((PayloadLength >> 24) & 0xFF);
    LengthPrefix[1] = static_cast<uint8>((PayloadLength >> 16) & 0xFF);
    LengthPrefix[2] = static_cast<uint8>((PayloadLength >> 8) & 0xFF);
    LengthPrefix[3] = static_cast<uint8>(PayloadLength & 0xFF);

    int32 BytesSent = 0;
    ClientSocket->Send(LengthPrefix, 4, BytesSent);

    if (PayloadLength > 0)
    {
        ClientSocket->Send((const uint8*)UTF8Response.Get(), PayloadLength, BytesSent);
    }

    UE_LOG(LogMcpJsonRpcServer, Verbose, TEXT("Sent response: %s"), *ResponseStr.Left(256));
}

void FMcpJsonRpcServer::SendError(FSocket* ClientSocket, const TOptional<int32>& Id, int32 Code, const FString& Message)
{
    TSharedPtr<FJsonObject> Response = MakeShareable(new FJsonObject);
    Response->SetStringField(TEXT("jsonrpc"), TEXT("2.0"));

    if (Id.IsSet())
    {
        Response->SetNumberField(TEXT("id"), Id.GetValue());
    }
    else
    {
        Response->SetField(TEXT("id"), MakeShareable(new FJsonValueNull()));
    }

    TSharedPtr<FJsonObject> ErrorObj = MakeShareable(new FJsonObject);
    ErrorObj->SetNumberField(TEXT("code"), Code);
    ErrorObj->SetStringField(TEXT("message"), Message);
    Response->SetObjectField(TEXT("error"), ErrorObj);

    SendResponse(ClientSocket, Response);
}
