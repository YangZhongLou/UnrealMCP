#include "Core/McpProtocolAdapter.h"
#include "Core/McpJsonRpcServer.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

FMcpProtocolAdapter::FMcpProtocolAdapter()
    : bInitialized(false)
{
    RegisterTools();
    RegisterResources();
}

TSharedPtr<FJsonObject> FMcpProtocolAdapter::ProcessRequest(const FMcpJsonRpcMessage& Request)
{
    // Notifications have no response
    if (Request.bIsNotification)
    {
        if (Request.Method == TEXT("notifications/initialized"))
        {
            // Client confirmed initialization
            return nullptr;
        }
        if (Request.Method == TEXT("notifications/cancelled"))
        {
            // Request cancellation — not implemented
            return nullptr;
        }
        return nullptr;
    }

    // Methods that don't require initialization
    if (Request.Method == TEXT("initialize"))
    {
        return HandleInitialize(Request.Params);
    }

    // Methods that require initialization
    if (!bInitialized)
    {
        return MakeErrorResponse(Request.Id, -32002, TEXT("Server not initialized"));
    }

    if (Request.Method == TEXT("tools/list"))
    {
        return HandleToolsList(Request.Params);
    }
    if (Request.Method == TEXT("tools/call"))
    {
        return HandleToolsCall(Request.Params);
    }
    if (Request.Method == TEXT("resources/list"))
    {
        return HandleResourcesList(Request.Params);
    }
    if (Request.Method == TEXT("resources/read"))
    {
        return HandleResourcesRead(Request.Params);
    }
    if (Request.Method == TEXT("ping"))
    {
        return HandlePing(Request.Params);
    }

    return MakeErrorResponse(Request.Id, -32601,
        FString::Printf(TEXT("Method not found: %s"), *Request.Method));
}

TSharedPtr<FJsonObject> FMcpProtocolAdapter::HandleInitialize(
    const TSharedPtr<FJsonObject>& Params)
{
    FString ClientProtocolVersion = TEXT("2024-11-05");
    if (Params.IsValid() && Params->HasField(TEXT("protocolVersion")))
    {
        ClientProtocolVersion = Params->GetStringField(TEXT("protocolVersion"));
    }

    NegotiatedProtocolVersion = ClientProtocolVersion;
    bInitialized = true;

    TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
    Result->SetStringField(TEXT("protocolVersion"), NegotiatedProtocolVersion);

    TSharedPtr<FJsonObject> Capabilities = MakeShareable(new FJsonObject);
    TSharedPtr<FJsonObject> ToolsCapability = MakeShareable(new FJsonObject);
    ToolsCapability->SetBoolField(TEXT("listChanged"), true);
    Capabilities->SetObjectField(TEXT("tools"), ToolsCapability);

    TSharedPtr<FJsonObject> ResourcesCapability = MakeShareable(new FJsonObject);
    ResourcesCapability->SetBoolField(TEXT("listChanged"), false);
    ResourcesCapability->SetBoolField(TEXT("subscribe"), false);
    Capabilities->SetObjectField(TEXT("resources"), ResourcesCapability);

    Result->SetObjectField(TEXT("capabilities"), Capabilities);

    TSharedPtr<FJsonObject> ServerInfo = MakeShareable(new FJsonObject);
    ServerInfo->SetStringField(TEXT("name"), TEXT("unreal-mcp-game"));
    ServerInfo->SetStringField(TEXT("version"), TEXT("3.4.0"));
    Result->SetObjectField(TEXT("serverInfo"), ServerInfo);

    return MakeResponse(TOptional<int32>(), Result);
}

TSharedPtr<FJsonObject> FMcpProtocolAdapter::HandleToolsList(
    const TSharedPtr<FJsonObject>& Params)
{
    TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);

    TArray<TSharedPtr<FJsonValue>> ToolsArray;
    for (const TSharedPtr<FJsonObject>& Tool : ToolList)
    {
        ToolsArray.Add(MakeShareable(new FJsonValueObject(Tool)));
    }
    Result->SetArrayField(TEXT("tools"), ToolsArray);

    return MakeResponse(TOptional<int32>(), Result);
}

TSharedPtr<FJsonObject> FMcpProtocolAdapter::HandleToolsCall(
    const TSharedPtr<FJsonObject>& Params)
{
    if (!Params.IsValid())
    {
        return MakeErrorResponse(TOptional<int32>(), -32602, TEXT("Missing params"));
    }

    FString ToolName = Params->GetStringField(TEXT("name"));
    TSharedPtr<FJsonObject> ToolParams = Params->GetObjectField(TEXT("arguments"));

    // For Phase 1, only stub implementations
    TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);

    if (ToolName == TEXT("generate_umg_snippet"))
    {
        TSharedPtr<FJsonObject> ContentItem = MakeShareable(new FJsonObject);
        ContentItem->SetStringField(TEXT("type"), TEXT("text"));
        ContentItem->SetStringField(TEXT("text"),
            TEXT("Widget tree generation not yet implemented in Phase 1"));

        TArray<TSharedPtr<FJsonValue>> ContentArray;
        ContentArray.Add(MakeShareable(new FJsonValueObject(ContentItem)));
        Result->SetArrayField(TEXT("content"), ContentArray);
        Result->SetBoolField(TEXT("isError"), false);
    }
    else if (ToolName == TEXT("preview_umg_snippet"))
    {
        TSharedPtr<FJsonObject> ContentItem = MakeShareable(new FJsonObject);
        ContentItem->SetStringField(TEXT("type"), TEXT("text"));
        ContentItem->SetStringField(TEXT("text"),
            TEXT("UMG preview not yet implemented in Phase 1"));

        TArray<TSharedPtr<FJsonValue>> ContentArray;
        ContentArray.Add(MakeShareable(new FJsonValueObject(ContentItem)));
        Result->SetArrayField(TEXT("content"), ContentArray);
        Result->SetBoolField(TEXT("isError"), false);
    }
    else if (ToolName == TEXT("ping"))
    {
        Result->SetStringField(TEXT("result"), TEXT("pong"));
    }
    else
    {
        return MakeErrorResponse(TOptional<int32>(), -32602,
            FString::Printf(TEXT("Unknown tool: %s"), *ToolName));
    }

    return MakeResponse(TOptional<int32>(), Result);
}

TSharedPtr<FJsonObject> FMcpProtocolAdapter::HandleResourcesList(
    const TSharedPtr<FJsonObject>& Params)
{
    TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);

    TArray<TSharedPtr<FJsonValue>> ResourcesArray;
    for (const TSharedPtr<FJsonObject>& Resource : ResourceList)
    {
        ResourcesArray.Add(MakeShareable(new FJsonValueObject(Resource)));
    }
    Result->SetArrayField(TEXT("resources"), ResourcesArray);

    return MakeResponse(TOptional<int32>(), Result);
}

TSharedPtr<FJsonObject> FMcpProtocolAdapter::HandleResourcesRead(
    const TSharedPtr<FJsonObject>& Params)
{
    if (!Params.IsValid() || !Params->HasField(TEXT("uri")))
    {
        return MakeErrorResponse(TOptional<int32>(), -32602, TEXT("Missing uri param"));
    }

    FString Uri = Params->GetStringField(TEXT("uri"));

    TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
    Result->SetStringField(TEXT("uri"), Uri);
    Result->SetStringField(TEXT("mimeType"), TEXT("application/json"));
    Result->SetStringField(TEXT("text"), TEXT("{}"));

    return MakeResponse(TOptional<int32>(), Result);
}

TSharedPtr<FJsonObject> FMcpProtocolAdapter::HandlePing(
    const TSharedPtr<FJsonObject>& Params)
{
    TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
    Result->SetStringField(TEXT("result"), TEXT("pong"));
    return MakeResponse(TOptional<int32>(), Result);
}

TSharedPtr<FJsonObject> FMcpProtocolAdapter::MakeResponse(
    const TOptional<int32>& Id, const TSharedPtr<FJsonObject>& Result)
{
    TSharedPtr<FJsonObject> Response = MakeShareable(new FJsonObject);
    Response->SetStringField(TEXT("jsonrpc"), TEXT("2.0"));

    if (Id.IsSet())
    {
        Response->SetNumberField(TEXT("id"), Id.GetValue());
    }

    Response->SetObjectField(TEXT("result"), Result);
    return Response;
}

TSharedPtr<FJsonObject> FMcpProtocolAdapter::MakeErrorResponse(
    const TOptional<int32>& Id, int32 Code, const FString& Message)
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

    return Response;
}

void FMcpProtocolAdapter::RegisterTools()
{
    // generate_umg_snippet
    {
        TSharedPtr<FJsonObject> Tool = MakeShareable(new FJsonObject);
        Tool->SetStringField(TEXT("name"), TEXT("generate_umg_snippet"));
        Tool->SetStringField(TEXT("description"),
            TEXT("Generate a UMG Widget tree JSON from a natural language description"));

        TSharedPtr<FJsonObject> Schema = MakeShareable(new FJsonObject);
        Schema->SetStringField(TEXT("type"), TEXT("object"));

        TSharedPtr<FJsonObject> Props = MakeShareable(new FJsonObject);

        TSharedPtr<FJsonObject> DescProp = MakeShareable(new FJsonObject);
        DescProp->SetStringField(TEXT("type"), TEXT("string"));
        DescProp->SetStringField(TEXT("description"), TEXT("Natural language UI layout description"));
        Props->SetObjectField(TEXT("description"), DescProp);

        TSharedPtr<FJsonObject> WidthProp = MakeShareable(new FJsonObject);
        WidthProp->SetStringField(TEXT("type"), TEXT("number"));
        WidthProp->SetStringField(TEXT("description"), TEXT("Preview width in pixels"));
        Props->SetObjectField(TEXT("width"), WidthProp);

        TSharedPtr<FJsonObject> HeightProp = MakeShareable(new FJsonObject);
        HeightProp->SetStringField(TEXT("type"), TEXT("number"));
        HeightProp->SetStringField(TEXT("description"), TEXT("Preview height in pixels"));
        Props->SetObjectField(TEXT("height"), HeightProp);

        Schema->SetObjectField(TEXT("properties"), Props);

        TArray<TSharedPtr<FJsonValue>> RequiredArray;
        RequiredArray.Add(MakeShareable(new FJsonValueString(TEXT("description"))));
        Schema->SetArrayField(TEXT("required"), RequiredArray);

        Tool->SetObjectField(TEXT("inputSchema"), Schema);
        ToolList.Add(Tool);
    }

    // preview_umg_snippet
    {
        TSharedPtr<FJsonObject> Tool = MakeShareable(new FJsonObject);
        Tool->SetStringField(TEXT("name"), TEXT("preview_umg_snippet"));
        Tool->SetStringField(TEXT("description"),
            TEXT("Render a Widget tree in an independent preview window"));

        TSharedPtr<FJsonObject> Schema = MakeShareable(new FJsonObject);
        Schema->SetStringField(TEXT("type"), TEXT("object"));

        TSharedPtr<FJsonObject> Props = MakeShareable(new FJsonObject);

        TSharedPtr<FJsonObject> JsonProp = MakeShareable(new FJsonObject);
        JsonProp->SetStringField(TEXT("type"), TEXT("object"));
        JsonProp->SetStringField(TEXT("description"), TEXT("Widget tree JSON conforming to UMG Schema"));
        Props->SetObjectField(TEXT("widget_json"), JsonProp);

        TSharedPtr<FJsonObject> IdProp = MakeShareable(new FJsonObject);
        IdProp->SetStringField(TEXT("type"), TEXT("string"));
        IdProp->SetStringField(TEXT("description"), TEXT("Preview instance identifier"));
        Props->SetObjectField(TEXT("preview_id"), IdProp);

        Schema->SetObjectField(TEXT("properties"), Props);

        TArray<TSharedPtr<FJsonValue>> RequiredArray;
        RequiredArray.Add(MakeShareable(new FJsonValueString(TEXT("widget_json"))));
        RequiredArray.Add(MakeShareable(new FJsonValueString(TEXT("preview_id"))));
        Schema->SetArrayField(TEXT("required"), RequiredArray);

        Tool->SetObjectField(TEXT("inputSchema"), Schema);
        ToolList.Add(Tool);
    }

    // ping (internal testing tool)
    {
        TSharedPtr<FJsonObject> Tool = MakeShareable(new FJsonObject);
        Tool->SetStringField(TEXT("name"), TEXT("ping"));
        Tool->SetStringField(TEXT("description"), TEXT("Ping the server for connectivity testing"));

        TSharedPtr<FJsonObject> Schema = MakeShareable(new FJsonObject);
        Schema->SetStringField(TEXT("type"), TEXT("object"));
        Schema->SetObjectField(TEXT("properties"), MakeShareable(new FJsonObject));
        Tool->SetObjectField(TEXT("inputSchema"), Schema);
        ToolList.Add(Tool);
    }
}

void FMcpProtocolAdapter::RegisterResources()
{
    // Widget template library resource
    {
        TSharedPtr<FJsonObject> Resource = MakeShareable(new FJsonObject);
        Resource->SetStringField(TEXT("uri"), TEXT("umg://templates"));
        Resource->SetStringField(TEXT("name"), TEXT("UMG Widget Templates"));
        Resource->SetStringField(TEXT("mimeType"), TEXT("application/json"));
        Resource->SetStringField(TEXT("description"),
            TEXT("Library of reusable UMG Widget tree templates"));
        ResourceList.Add(Resource);
    }

    // Server log resource
    {
        TSharedPtr<FJsonObject> Resource = MakeShareable(new FJsonObject);
        Resource->SetStringField(TEXT("uri"), TEXT("logs://unreal"));
        Resource->SetStringField(TEXT("name"), TEXT("Unreal Engine Logs"));
        Resource->SetStringField(TEXT("mimeType"), TEXT("text/plain"));
        Resource->SetStringField(TEXT("description"), TEXT("Recent Unreal Engine log output"));
        ResourceList.Add(Resource);
    }
}
