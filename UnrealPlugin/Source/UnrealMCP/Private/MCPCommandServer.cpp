#include "MCPCommandServer.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

DEFINE_LOG_CATEGORY_STATIC(LogMCPCommandServer, Log, All);

// Forward declarations from command files
FString HandleSpawnActor(const TSharedPtr<FJsonObject>& Params);
FString HandleDestroyActor(const TSharedPtr<FJsonObject>& Params);
FString HandleSetActorTransform(const TSharedPtr<FJsonObject>& Params);
FString HandleGetActorList(const TSharedPtr<FJsonObject>& Params);
FString HandleRunConsoleCommand(const TSharedPtr<FJsonObject>& Params);
FString HandleSaveCurrentLevel(const TSharedPtr<FJsonObject>& Params);
FString HandlePlayInEditor(const TSharedPtr<FJsonObject>& Params);
FString HandleStopPlayInEditor(const TSharedPtr<FJsonObject>& Params);
FString HandleCreateBlueprint(const TSharedPtr<FJsonObject>& Params);
FString HandleCompileBlueprint(const TSharedPtr<FJsonObject>& Params);
FString HandleGetBlueprintInfo(const TSharedPtr<FJsonObject>& Params);
FString HandleGetAssetList(const TSharedPtr<FJsonObject>& Params);
FString HandleGetAssetInfo(const TSharedPtr<FJsonObject>& Params);
FString HandleDeleteAsset(const TSharedPtr<FJsonObject>& Params);
FString HandleRenameAsset(const TSharedPtr<FJsonObject>& Params);

FMCPCommandServer::FMCPCommandServer()
    : Thread(nullptr)
    , ListenSocket(nullptr)
    , bRunning(false)
    , ServerPort(13377)
{
}

FMCPCommandServer::~FMCPCommandServer()
{
    StopServer();
}

bool FMCPCommandServer::StartServer(int32 Port)
{
    ServerPort = Port;
    bRunning = true;

    Thread = FRunnableThread::Create(this, TEXT("MCPCommandServerThread"), 0, TPri_Normal);
    return Thread != nullptr;
}

void FMCPCommandServer::StopServer()
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

bool FMCPCommandServer::Init()
{
    ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
    ListenSocket = SocketSubsystem->CreateSocket(NAME_Stream, TEXT("MCPListenSocket"), false);

    if (!ListenSocket)
    {
        UE_LOG(LogMCPCommandServer, Error, TEXT("Failed to create listen socket"));
        return false;
    }

    ListenAddr = SocketSubsystem->CreateInternetAddr();
    ListenAddr->SetLoopbackAddress(true);
    ListenAddr->SetPort(ServerPort);

    ListenSocket->SetReuseAddr(true);
    ListenSocket->SetNonBlocking(false);

    if (!ListenSocket->Bind(*ListenAddr))
    {
        UE_LOG(LogMCPCommandServer, Error, TEXT("Failed to bind socket to port %d"), ServerPort);
        return false;
    }

    if (!ListenSocket->Listen(4))
    {
        UE_LOG(LogMCPCommandServer, Error, TEXT("Failed to listen on socket"));
        return false;
    }

    UE_LOG(LogMCPCommandServer, Log, TEXT("Server listening on port %d"), ServerPort);
    return true;
}

uint32 FMCPCommandServer::Run()
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
            FSocket* ClientSocket = ListenSocket->Accept(TEXT("MCPClientSocket"));
            if (ClientSocket)
            {
                UE_LOG(LogMCPCommandServer, Log, TEXT("Client connected"));
                HandleClientConnection(ClientSocket);
                ClientSocket->Close();
                ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(ClientSocket);
                UE_LOG(LogMCPCommandServer, Log, TEXT("Client disconnected"));
            }
        }

        FPlatformProcess::Sleep(0.01f);
    }

    return 0;
}

void FMCPCommandServer::Stop()
{
    bRunning = false;
}

void FMCPCommandServer::Exit()
{
}

void FMCPCommandServer::HandleClientConnection(FSocket* ClientSocket)
{
    TArray<uint8> Buffer;
    Buffer.SetNumUninitialized(65536);

    while (bRunning)
    {
        int32 BytesRead = 0;
        if (!ClientSocket->Recv(Buffer.GetData(), Buffer.Num(), BytesRead))
        {
            break;
        }

        if (BytesRead > 0)
        {
            FString RequestStr = FString(UTF8_TO_TCHAR(reinterpret_cast<const char*>(Buffer.GetData())));
            RequestStr = RequestStr.Left(BytesRead);

            UE_LOG(LogMCPCommandServer, Log, TEXT("Received: %s"), *RequestStr);

            FString ResponseStr = ProcessCommand(RequestStr);

            FTCHARToUTF8 UTF8Response(*ResponseStr);
            int32 BytesSent = 0;
            ClientSocket->Send((const uint8*)UTF8Response.Get(), UTF8Response.Length(), BytesSent);
        }

        FPlatformProcess::Sleep(0.001f);
    }
}

FString FMCPCommandServer::ProcessCommand(const FString& JsonRequest)
{
    TSharedPtr<FJsonObject> RequestObject;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonRequest);

    if (!FJsonSerializer::Deserialize(Reader, RequestObject) || !RequestObject.IsValid())
    {
        return TEXT("{\"success\":false,\"error\":\"Invalid JSON\"}");
    }

    FString Method = RequestObject->GetStringField(TEXT("method"));
    TSharedPtr<FJsonObject> Params = RequestObject->GetObjectField(TEXT("params"));
    FString RequestId = RequestObject->GetStringField(TEXT("id"));

    FString ResultStr;

    if (Method == TEXT("get_editor_info"))
    {
        TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
        Result->SetStringField(TEXT("engine_version"), FEngineVersion::Current().ToString());
        Result->SetStringField(TEXT("project_name"), FApp::GetProjectName());

        TSharedPtr<FJsonObject> Response = MakeShareable(new FJsonObject);
        Response->SetStringField(TEXT("id"), RequestId);
        Response->SetBoolField(TEXT("success"), true);
        Response->SetObjectField(TEXT("result"), Result);

        TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ResultStr);
        FJsonSerializer::Serialize(Response.ToSharedRef(), Writer);
    }
    else if (Method == TEXT("spawn_actor"))
    {
        ResultStr = HandleSpawnActor(Params);
    }
    else if (Method == TEXT("destroy_actor"))
    {
        ResultStr = HandleDestroyActor(Params);
    }
    else if (Method == TEXT("set_actor_transform"))
    {
        ResultStr = HandleSetActorTransform(Params);
    }
    else if (Method == TEXT("get_actor_list"))
    {
        ResultStr = HandleGetActorList(Params);
    }
    else if (Method == TEXT("run_console_command"))
    {
        ResultStr = HandleRunConsoleCommand(Params);
    }
    else if (Method == TEXT("save_current_level"))
    {
        ResultStr = HandleSaveCurrentLevel(Params);
    }
    else if (Method == TEXT("play_in_editor"))
    {
        ResultStr = HandlePlayInEditor(Params);
    }
    else if (Method == TEXT("stop_play_in_editor"))
    {
        ResultStr = HandleStopPlayInEditor(Params);
    }
    else if (Method == TEXT("create_blueprint"))
    {
        ResultStr = HandleCreateBlueprint(Params);
    }
    else if (Method == TEXT("compile_blueprint"))
    {
        ResultStr = HandleCompileBlueprint(Params);
    }
    else if (Method == TEXT("get_blueprint_info"))
    {
        ResultStr = HandleGetBlueprintInfo(Params);
    }
    else if (Method == TEXT("get_asset_list"))
    {
        ResultStr = HandleGetAssetList(Params);
    }
    else if (Method == TEXT("get_asset_info"))
    {
        ResultStr = HandleGetAssetInfo(Params);
    }
    else if (Method == TEXT("delete_asset"))
    {
        ResultStr = HandleDeleteAsset(Params);
    }
    else if (Method == TEXT("rename_asset"))
    {
        ResultStr = HandleRenameAsset(Params);
    }
    else
    {
        TSharedPtr<FJsonObject> Response = MakeShareable(new FJsonObject);
        Response->SetStringField(TEXT("id"), RequestId);
        Response->SetBoolField(TEXT("success"), false);
        Response->SetStringField(TEXT("error"), FString::Printf(TEXT("Unknown method: %s"), *Method));

        TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ResultStr);
        FJsonSerializer::Serialize(Response.ToSharedRef(), Writer);
    }

    return ResultStr;
}
