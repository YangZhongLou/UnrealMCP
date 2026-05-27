#include "CoreMinimal.h"
#include "Engine/Blueprint.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "K2Node_CallFunction.h"
#include "K2Node_Event.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_FunctionResult.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Kismet/KismetMathLibrary.h"

FString HandleCreateBlueprint(const TSharedPtr<FJsonObject>& Params)
{
    FString Name = Params->GetStringField(TEXT("name"));
    FString ParentClassName = Params->GetStringField(TEXT("parentClass"));
    FString Path = Params->GetStringField(TEXT("path"));

    if (Path.IsEmpty())
    {
        Path = TEXT("/Game/Blueprints");
    }

    UClass* ParentClass = FindObject<UClass>(ANY_PACKAGE, *ParentClassName);
    if (!ParentClass)
    {
        ParentClass = AActor::StaticClass();
    }

    FString PackagePath = Path / Name;
    UPackage* Package = CreatePackage(*PackagePath);

    UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprint(
        ParentClass,
        Package,
        FName(*Name),
        EBlueprintType::BPTYPE_Normal,
        UBlueprint::StaticClass(),
        UBlueprintGeneratedClass::StaticClass(),
        FName("CreateBlueprintCommand")
    );

    if (!Blueprint)
    {
        return TEXT("{\"success\":false,\"error\":\"Failed to create blueprint\"}");
    }

    FAssetRegistryModule& AssetRegModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
    AssetRegModule.AssetCreated(Blueprint);
    Package->MarkPackageDirty();

    TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
    Result->SetStringField(TEXT("blueprint_name"), Name);
    Result->SetStringField(TEXT("path"), PackagePath);
    Result->SetStringField(TEXT("parent_class"), ParentClass->GetName());

    TSharedPtr<FJsonObject> Response = MakeShareable(new FJsonObject);
    Response->SetBoolField(TEXT("success"), true);
    Response->SetObjectField(TEXT("result"), Result);

    FString Out;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Out);
    FJsonSerializer::Serialize(Response.ToSharedRef(), Writer);
    return Out;
}

FString HandleCompileBlueprint(const TSharedPtr<FJsonObject>& Params)
{
    FString Path = Params->GetStringField(TEXT("path"));

    UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *Path);
    if (!Blueprint)
    {
        return FString::Printf(TEXT("{\"success\":false,\"error\":\"Blueprint not found: %s\"}"), *Path);
    }

    FBlueprintEditorUtils::RefreshAllNodes(Blueprint);
    FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

    EBlueprintCompileOptions Options = EBlueprintCompileOptions::SkipGarbageCollection;
    FKismetEditorUtilities::CompileBlueprint(Blueprint, Options);

    return TEXT("{\"success\":true,\"result\":{\"compiled\":true}}");
}

FString HandleGetBlueprintInfo(const TSharedPtr<FJsonObject>& Params)
{
    FString Path = Params->GetStringField(TEXT("path"));

    UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *Path);
    if (!Blueprint)
    {
        return FString::Printf(TEXT("{\"success\":false,\"error\":\"Blueprint not found: %s\"}"), *Path);
    }

    TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
    Result->SetStringField(TEXT("name"), Blueprint->GetName());
    Result->SetStringField(TEXT("parent_class"), Blueprint->ParentClass ? Blueprint->ParentClass->GetName() : TEXT("None"));
    Result->SetBoolField(TEXT("is_compiled"), !Blueprint->bBeingCompiled && Blueprint->GeneratedClass != nullptr);

    TArray<TSharedPtr<FJsonValue>> Variables;
    for (FBPVariableDescription& Var : Blueprint->NewVariables)
    {
        TSharedPtr<FJsonObject> VarObj = MakeShareable(new FJsonObject);
        VarObj->SetStringField(TEXT("name"), Var.VarName.ToString());
        VarObj->SetStringField(TEXT("type"), Var.VarType.PinCategory.ToString());
        Variables.Add(MakeShareable(new FJsonValueObject(VarObj)));
    }
    Result->SetArrayField(TEXT("variables"), Variables);

    TSharedPtr<FJsonObject> Response = MakeShareable(new FJsonObject);
    Response->SetBoolField(TEXT("success"), true);
    Response->SetObjectField(TEXT("result"), Result);

    FString Out;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Out);
    FJsonSerializer::Serialize(Response.ToSharedRef(), Writer);
    return Out;
}

// ---- Blueprint Graph Editing ----

static UFunction* FindFunctionByName(const FString& FunctionName)
{
    TArray<UClass*> SearchClasses = {
        UKismetSystemLibrary::StaticClass(),
        UKismetMathLibrary::StaticClass(),
        AActor::StaticClass(),
        UActorComponent::StaticClass(),
        UGameplayStatics::StaticClass(),
    };

    for (UClass* Class : SearchClasses)
    {
        UFunction* Func = Class->FindFunctionByName(*FunctionName);
        if (Func) return Func;
    }

    return FindObject<UFunction>(ANY_PACKAGE, *FunctionName);
}

FString HandleAddBlueprintNode(const TSharedPtr<FJsonObject>& Params)
{
    FString Path = Params->GetStringField(TEXT("path"));
    FString NodeType = Params->GetStringField(TEXT("node_type"));

    UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *Path);
    if (!Blueprint)
    {
        return FString::Printf(TEXT("{\"success\":false,\"error\":\"Blueprint not found: %s\"}"), *Path);
    }

    UEdGraph* TargetGraph = nullptr;
    FString GraphType = Params->HasField(TEXT("graph_type"))
        ? Params->GetStringField(TEXT("graph_type"))
        : TEXT("EventGraph");

    if (GraphType == TEXT("EventGraph"))
    {
        if (Blueprint->UbergraphPages.Num() > 0)
        {
            TargetGraph = Blueprint->UbergraphPages[0];
        }
    }
    else
    {
        for (UEdGraph* Graph : Blueprint->FunctionGraphs)
        {
            if (Graph->GetName() == GraphType) { TargetGraph = Graph; break; }
        }
        if (!TargetGraph)
        {
            for (UEdGraph* Graph : Blueprint->UbergraphPages)
            {
                if (Graph->GetName() == GraphType) { TargetGraph = Graph; break; }
            }
        }
    }

    if (!TargetGraph)
    {
        return FString::Printf(TEXT("{\"success\":false,\"error\":\"Graph not found: %s\"}"), *GraphType);
    }

    UK2Node* NewNode = nullptr;
    int32 PosX = Params->HasField(TEXT("pos_x")) ? (int32)Params->GetNumberField(TEXT("pos_x")) : 0;
    int32 PosY = Params->HasField(TEXT("pos_y")) ? (int32)Params->GetNumberField(TEXT("pos_y")) : 0;

    if (NodeType == TEXT("CallFunction"))
    {
        FString FunctionName = Params->GetStringField(TEXT("function_name"));
        UFunction* Function = nullptr;

        if (Params->HasField(TEXT("class_name")))
        {
            UClass* Class = FindObject<UClass>(ANY_PACKAGE, *Params->GetStringField(TEXT("class_name")));
            if (Class) { Function = Class->FindFunctionByName(*FunctionName); }
        }
        else
        {
            Function = FindFunctionByName(FunctionName);
        }

        if (!Function)
        {
            return FString::Printf(TEXT("{\"success\":false,\"error\":\"Function not found: %s\"}"), *FunctionName);
        }

        UK2Node_CallFunction* CallFuncNode = NewObject<UK2Node_CallFunction>(TargetGraph);
        CallFuncNode->SetFromFunction(Function);
        CallFuncNode->AllocateDefaultPins();
        TargetGraph->AddNode(CallFuncNode);
        NewNode = CallFuncNode;
    }
    else if (NodeType == TEXT("Event"))
    {
        FString EventName = Params->GetStringField(TEXT("event_name"));
        UFunction* EventFunc = AActor::StaticClass()->FindFunctionByName(*EventName);
        if (!EventFunc)
        {
            return FString::Printf(TEXT("{\"success\":false,\"error\":\"Event not found: %s\"}"), *EventName);
        }

        UK2Node_Event* EventNode = NewObject<UK2Node_Event>(TargetGraph);
        EventNode->EventReference.SetExternalMember(EventFunc->GetFName(), AActor::StaticClass());
        EventNode->AllocateDefaultPins();
        TargetGraph->AddNode(EventNode);
        NewNode = EventNode;
    }
    else if (NodeType == TEXT("CustomEvent"))
    {
        FString EventName = Params->GetStringField(TEXT("event_name"));

        UK2Node_CustomEvent* CustomEventNode = NewObject<UK2Node_CustomEvent>(TargetGraph);
        CustomEventNode->CustomFunctionName = *EventName;
        CustomEventNode->AllocateDefaultPins();
        TargetGraph->AddNode(CustomEventNode);
        NewNode = CustomEventNode;
    }
    else if (NodeType == TEXT("VariableGet") || NodeType == TEXT("VariableSet"))
    {
        FString VarName = Params->GetStringField(TEXT("variable_name"));

        FBPVariableDescription* VarDesc = Blueprint->NewVariables.FindByPredicate(
            [&VarName](const FBPVariableDescription& Var) { return Var.VarName == *VarName; });

        if (!VarDesc)
        {
            return FString::Printf(TEXT("{\"success\":false,\"error\":\"Variable not found: %s\"}"), *VarName);
        }

        if (NodeType == TEXT("VariableGet"))
        {
            UK2Node_VariableGet* GetNode = NewObject<UK2Node_VariableGet>(TargetGraph);
            GetNode->VariableReference.SetExternalMember(VarDesc->VarName, Blueprint->GeneratedClass);
            GetNode->AllocateDefaultPins();
            TargetGraph->AddNode(GetNode);
            NewNode = GetNode;
        }
        else
        {
            UK2Node_VariableSet* SetNode = NewObject<UK2Node_VariableSet>(TargetGraph);
            SetNode->VariableReference.SetExternalMember(VarDesc->VarName, Blueprint->GeneratedClass);
            SetNode->AllocateDefaultPins();
            TargetGraph->AddNode(SetNode);
            NewNode = SetNode;
        }
    }
    else if (NodeType == TEXT("PrintString"))
    {
        UFunction* PrintFunc = UKismetSystemLibrary::StaticClass()->FindFunctionByName(TEXT("PrintString"));
        if (!PrintFunc)
        {
            return TEXT("{\"success\":false,\"error\":\"PrintString function not found\"}");
        }

        UK2Node_CallFunction* CallFuncNode = NewObject<UK2Node_CallFunction>(TargetGraph);
        CallFuncNode->SetFromFunction(PrintFunc);
        CallFuncNode->AllocateDefaultPins();
        TargetGraph->AddNode(CallFuncNode);
        NewNode = CallFuncNode;
    }
    else
    {
        return FString::Printf(TEXT("{\"success\":false,\"error\":\"Unknown node type: %s\"}"), *NodeType);
    }

    NewNode->NodePosX = PosX;
    NewNode->NodePosY = PosY;

    TArray<TSharedPtr<FJsonValue>> Pins;
    for (UEdGraphPin* Pin : NewNode->GetAllPins())
    {
        if (Pin)
        {
            TSharedPtr<FJsonObject> PinObj = MakeShareable(new FJsonObject);
            PinObj->SetStringField(TEXT("name"), Pin->PinName.ToString());
            PinObj->SetStringField(TEXT("direction"), Pin->Direction == EGPD_Output ? TEXT("output") : TEXT("input"));
            PinObj->SetStringField(TEXT("category"), Pin->PinType.PinCategory.ToString());
            Pins.Add(MakeShareable(new FJsonValueObject(PinObj)));
        }
    }

    FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

    TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
    Result->SetStringField(TEXT("node_id"), NewNode->NodeGuid.ToString());
    Result->SetStringField(TEXT("node_type"), NodeType);
    Result->SetStringField(TEXT("graph"), GraphType);
    Result->SetNumberField(TEXT("pos_x"), PosX);
    Result->SetNumberField(TEXT("pos_y"), PosY);
    Result->SetArrayField(TEXT("pins"), Pins);

    TSharedPtr<FJsonObject> Response = MakeShareable(new FJsonObject);
    Response->SetBoolField(TEXT("success"), true);
    Response->SetObjectField(TEXT("result"), Result);

    FString Out;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Out);
    FJsonSerializer::Serialize(Response.ToSharedRef(), Writer);
    return Out;
}

FString HandleConnectBlueprintPins(const TSharedPtr<FJsonObject>& Params)
{
    FString Path = Params->GetStringField(TEXT("path"));
    FString SourceNodeId = Params->GetStringField(TEXT("source_node_id"));
    FString SourcePinName = Params->GetStringField(TEXT("source_pin"));
    FString TargetNodeId = Params->GetStringField(TEXT("target_node_id"));
    FString TargetPinName = Params->GetStringField(TEXT("target_pin"));

    UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *Path);
    if (!Blueprint)
    {
        return FString::Printf(TEXT("{\"success\":false,\"error\":\"Blueprint not found: %s\"}"), *Path);
    }

    auto FindNodeById = [](UEdGraph* Graph, const FString& NodeId) -> UEdGraphNode*
    {
        for (UEdGraphNode* Node : Graph->Nodes)
        {
            if (Node && Node->NodeGuid.ToString() == NodeId) return Node;
        }
        return nullptr;
    };

    UEdGraphNode* SourceNode = nullptr;
    UEdGraphNode* TargetNode = nullptr;

    for (UEdGraph* Graph : Blueprint->UbergraphPages)
    {
        if (!SourceNode) SourceNode = FindNodeById(Graph, SourceNodeId);
        if (!TargetNode) TargetNode = FindNodeById(Graph, TargetNodeId);
    }
    for (UEdGraph* Graph : Blueprint->FunctionGraphs)
    {
        if (!SourceNode) SourceNode = FindNodeById(Graph, SourceNodeId);
        if (!TargetNode) TargetNode = FindNodeById(Graph, TargetNodeId);
    }

    if (!SourceNode)
    {
        return FString::Printf(TEXT("{\"success\":false,\"error\":\"Source node not found: %s\"}"), *SourceNodeId);
    }
    if (!TargetNode)
    {
        return FString::Printf(TEXT("{\"success\":false,\"error\":\"Target node not found: %s\"}"), *TargetNodeId);
    }

    UEdGraphPin* SourcePin = SourceNode->FindPin(*SourcePinName, EGPD_Output);
    UEdGraphPin* TargetPin = TargetNode->FindPin(*TargetPinName, EGPD_Input);

    if (!SourcePin)
    {
        return FString::Printf(TEXT("{\"success\":false,\"error\":\"Source pin not found: %s on node %s\"}"), *SourcePinName, *SourceNodeId);
    }
    if (!TargetPin)
    {
        return FString::Printf(TEXT("{\"success\":false,\"error\":\"Target pin not found: %s on node %s\"}"), *TargetPinName, *TargetNodeId);
    }

    SourcePin->MakeLinkTo(TargetPin);

    FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

    TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
    Result->SetStringField(TEXT("source_node"), SourceNodeId);
    Result->SetStringField(TEXT("source_pin"), SourcePinName);
    Result->SetStringField(TEXT("target_node"), TargetNodeId);
    Result->SetStringField(TEXT("target_pin"), TargetPinName);

    TSharedPtr<FJsonObject> Response = MakeShareable(new FJsonObject);
    Response->SetBoolField(TEXT("success"), true);
    Response->SetObjectField(TEXT("result"), Result);

    FString Out;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Out);
    FJsonSerializer::Serialize(Response.ToSharedRef(), Writer);
    return Out;
}

FString HandleGetBlueprintGraph(const TSharedPtr<FJsonObject>& Params)
{
    FString Path = Params->GetStringField(TEXT("path"));
    FString GraphType = Params->HasField(TEXT("graph_type"))
        ? Params->GetStringField(TEXT("graph_type"))
        : TEXT("EventGraph");

    UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *Path);
    if (!Blueprint)
    {
        return FString::Printf(TEXT("{\"success\":false,\"error\":\"Blueprint not found: %s\"}"), *Path);
    }

    UEdGraph* TargetGraph = nullptr;
    if (GraphType == TEXT("EventGraph"))
    {
        if (Blueprint->UbergraphPages.Num() > 0)
        {
            TargetGraph = Blueprint->UbergraphPages[0];
        }
    }
    else
    {
        for (UEdGraph* Graph : Blueprint->UbergraphPages)
        {
            if (Graph->GetName() == GraphType) { TargetGraph = Graph; break; }
        }
        if (!TargetGraph)
        {
            for (UEdGraph* Graph : Blueprint->FunctionGraphs)
            {
                if (Graph->GetName() == GraphType) { TargetGraph = Graph; break; }
            }
        }
    }

    if (!TargetGraph)
    {
        return FString::Printf(TEXT("{\"success\":false,\"error\":\"Graph not found: %s\"}"), *GraphType);
    }

    TArray<TSharedPtr<FJsonValue>> Nodes;
    for (UEdGraphNode* Node : TargetGraph->Nodes)
    {
        if (!Node) continue;

        TSharedPtr<FJsonObject> NodeObj = MakeShareable(new FJsonObject);
        NodeObj->SetStringField(TEXT("node_id"), Node->NodeGuid.ToString());
        NodeObj->SetStringField(TEXT("class"), Node->GetClass()->GetName());
        NodeObj->SetStringField(TEXT("title"), Node->GetNodeTitle(ENodeTitleType::ListView).ToString());
        NodeObj->SetNumberField(TEXT("pos_x"), Node->NodePosX);
        NodeObj->SetNumberField(TEXT("pos_y"), Node->NodePosY);

        TArray<TSharedPtr<FJsonValue>> Pins;
        for (UEdGraphPin* Pin : Node->GetAllPins())
        {
            if (!Pin || Pin->bHidden) continue;

            TSharedPtr<FJsonObject> PinObj = MakeShareable(new FJsonObject);
            PinObj->SetStringField(TEXT("name"), Pin->PinName.ToString());
            PinObj->SetStringField(TEXT("direction"), Pin->Direction == EGPD_Output ? TEXT("output") : TEXT("input"));
            PinObj->SetStringField(TEXT("category"), Pin->PinType.PinCategory.ToString());

            TArray<TSharedPtr<FJsonValue>> Connections;
            for (UEdGraphPin* LinkedPin : Pin->LinkedTo)
            {
                if (LinkedPin && LinkedPin->GetOwningNode())
                {
                    TSharedPtr<FJsonObject> LinkObj = MakeShareable(new FJsonObject);
                    LinkObj->SetStringField(TEXT("node_id"), LinkedPin->GetOwningNode()->NodeGuid.ToString());
                    LinkObj->SetStringField(TEXT("pin"), LinkedPin->PinName.ToString());
                    Connections.Add(MakeShareable(new FJsonValueObject(LinkObj)));
                }
            }
            if (Connections.Num() > 0)
            {
                PinObj->SetArrayField(TEXT("connected_to"), Connections);
            }

            Pins.Add(MakeShareable(new FJsonValueObject(PinObj)));
        }
        NodeObj->SetArrayField(TEXT("pins"), Pins);
        Nodes.Add(MakeShareable(new FJsonValueObject(NodeObj)));
    }

    TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
    Result->SetStringField(TEXT("graph_name"), TargetGraph->GetName());
    Result->SetStringField(TEXT("graph_type"), GraphType);
    Result->SetNumberField(TEXT("node_count"), Nodes.Num());
    Result->SetArrayField(TEXT("nodes"), Nodes);

    TSharedPtr<FJsonObject> Response = MakeShareable(new FJsonObject);
    Response->SetBoolField(TEXT("success"), true);
    Response->SetObjectField(TEXT("result"), Result);

    FString Out;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Out);
    FJsonSerializer::Serialize(Response.ToSharedRef(), Writer);
    return Out;
}

// ---- Blueprint Variable Management ----

static bool ParsePinType(const FString& TypeName, FEdGraphPinType& OutPinType)
{
    if (TypeName == TEXT("int") || TypeName == TEXT("int32"))
    {
        OutPinType.PinCategory = TEXT("int");
        return true;
    }
    if (TypeName == TEXT("int64"))
    {
        OutPinType.PinCategory = TEXT("int64");
        return true;
    }
    if (TypeName == TEXT("float"))
    {
        OutPinType.PinCategory = TEXT("real");
        OutPinType.PinSubCategory = TEXT("float");
        return true;
    }
    if (TypeName == TEXT("double"))
    {
        OutPinType.PinCategory = TEXT("real");
        OutPinType.PinSubCategory = TEXT("double");
        return true;
    }
    if (TypeName == TEXT("bool"))
    {
        OutPinType.PinCategory = TEXT("bool");
        return true;
    }
    if (TypeName == TEXT("string") || TypeName == TEXT("FString"))
    {
        OutPinType.PinCategory = TEXT("string");
        return true;
    }
    if (TypeName == TEXT("name") || TypeName == TEXT("FName"))
    {
        OutPinType.PinCategory = TEXT("name");
        return true;
    }
    if (TypeName == TEXT("text") || TypeName == TEXT("FText"))
    {
        OutPinType.PinCategory = TEXT("text");
        return true;
    }
    if (TypeName == TEXT("Vector") || TypeName == TEXT("FVector"))
    {
        OutPinType.PinCategory = TEXT("struct");
        OutPinType.PinSubCategoryObject = TBaseStructure<FVector>::Get();
        return true;
    }
    if (TypeName == TEXT("Rotator") || TypeName == TEXT("FRotator"))
    {
        OutPinType.PinCategory = TEXT("struct");
        OutPinType.PinSubCategoryObject = TBaseStructure<FRotator>::Get();
        return true;
    }
    if (TypeName == TEXT("Transform") || TypeName == TEXT("FTransform"))
    {
        OutPinType.PinCategory = TEXT("struct");
        OutPinType.PinSubCategoryObject = TBaseStructure<FTransform>::Get();
        return true;
    }
    if (TypeName == TEXT("Color") || TypeName == TEXT("FColor") || TypeName == TEXT("FLinearColor"))
    {
        OutPinType.PinCategory = TEXT("struct");
        OutPinType.PinSubCategoryObject = TBaseStructure<FLinearColor>::Get();
        return true;
    }
    // Object types
    {
        UClass* FoundClass = FindObject<UClass>(ANY_PACKAGE, *TypeName);
        if (FoundClass && FoundClass->IsChildOf<UObject>())
        {
            OutPinType.PinCategory = TEXT("object");
            OutPinType.PinSubCategoryObject = FoundClass;
            return true;
        }
    }
    return false;
}

FString HandleAddBlueprintVariable(const TSharedPtr<FJsonObject>& Params)
{
    FString Path = Params->GetStringField(TEXT("path"));
    FString VarName = Params->GetStringField(TEXT("variable_name"));
    FString VarType = Params->GetStringField(TEXT("variable_type"));
    bool bIsArray = Params->HasField(TEXT("is_array")) && Params->GetBoolField(TEXT("is_array"));

    UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *Path);
    if (!Blueprint)
    {
        return FString::Printf(TEXT("{\"success\":false,\"error\":\"Blueprint not found: %s\"}"), *Path);
    }

    for (const FBPVariableDescription& Var : Blueprint->NewVariables)
    {
        if (Var.VarName == *VarName)
        {
            return FString::Printf(TEXT("{\"success\":false,\"error\":\"Variable already exists: %s\"}"), *VarName);
        }
    }

    FBPVariableDescription NewVar;
    NewVar.VarName = *VarName;

    if (!ParsePinType(VarType, NewVar.VarType))
    {
        return FString::Printf(TEXT("{\"success\":false,\"error\":\"Unsupported variable type: %s\"}"), *VarType);
    }

    NewVar.VarType.bIsArray = bIsArray;
    NewVar.DefaultValue = TEXT("");

    Blueprint->NewVariables.Add(NewVar);
    Blueprint->bIsNewlyCreated = false;

    FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

    TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
    Result->SetStringField(TEXT("variable_name"), VarName);
    Result->SetStringField(TEXT("variable_type"), VarType);
    Result->SetBoolField(TEXT("is_array"), bIsArray);

    TSharedPtr<FJsonObject> Response = MakeShareable(new FJsonObject);
    Response->SetBoolField(TEXT("success"), true);
    Response->SetObjectField(TEXT("result"), Result);

    FString Out;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Out);
    FJsonSerializer::Serialize(Response.ToSharedRef(), Writer);
    return Out;
}

FString HandleRemoveBlueprintVariable(const TSharedPtr<FJsonObject>& Params)
{
    FString Path = Params->GetStringField(TEXT("path"));
    FString VarName = Params->GetStringField(TEXT("variable_name"));

    UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *Path);
    if (!Blueprint)
    {
        return FString::Printf(TEXT("{\"success\":false,\"error\":\"Blueprint not found: %s\"}"), *Path);
    }

    int32 RemoveCount = Blueprint->NewVariables.RemoveAll(
        [&VarName](const FBPVariableDescription& Var) { return Var.VarName == *VarName; });

    if (RemoveCount == 0)
    {
        return FString::Printf(TEXT("{\"success\":false,\"error\":\"Variable not found: %s\"}"), *VarName);
    }

    FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

    TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
    Result->SetStringField(TEXT("variable_name"), VarName);

    TSharedPtr<FJsonObject> Response = MakeShareable(new FJsonObject);
    Response->SetBoolField(TEXT("success"), true);
    Response->SetObjectField(TEXT("result"), Result);

    FString Out;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Out);
    FJsonSerializer::Serialize(Response.ToSharedRef(), Writer);
    return Out;
}

// Find a graph by name in a Blueprint (searches FunctionGraphs, UbergraphPages, DelegateSignatureGraphs)
static UEdGraph* FindGraph(UBlueprint* Blueprint, const FString& Name)
{
    for (UEdGraph* Graph : Blueprint->FunctionGraphs)
    {
        if (Graph->GetName() == Name) return Graph;
    }
    for (UEdGraph* Graph : Blueprint->UbergraphPages)
    {
        if (Graph->GetName() == Name) return Graph;
    }
    for (UEdGraph* Graph : Blueprint->DelegateSignatureGraphs)
    {
        if (Graph->GetName() == Name) return Graph;
    }
    return nullptr;
}

// Find FunctionEntry node in a graph, return its node GUID as string
static FString GetEntryNodeId(UEdGraph* Graph)
{
    for (UEdGraphNode* Node : Graph->Nodes)
    {
        if (UK2Node_FunctionEntry* Entry = Cast<UK2Node_FunctionEntry>(Node))
        {
            return Entry->NodeGuid.ToString();
        }
    }
    return TEXT("");
}

FString HandleCreateBlueprintFunctionGraph(const TSharedPtr<FJsonObject>& Params)
{
    FString Path = Params->GetStringField(TEXT("path"));
    FString FunctionName = Params->GetStringField(TEXT("function_name"));
    FString Category = Params->HasField(TEXT("category"))
        ? Params->GetStringField(TEXT("category"))
        : TEXT("");

    UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *Path);
    if (!Blueprint)
    {
        return FString::Printf(TEXT("{\"success\":false,\"error\":\"Blueprint not found: %s\"}"), *Path);
    }

    // Check for existing graph with same name
    if (FindGraph(Blueprint, FunctionName))
    {
        return FString::Printf(TEXT("{\"success\":false,\"error\":\"Function already exists: %s\"}"), *FunctionName);
    }

    UEdGraph* NewGraph = FBlueprintEditorUtils::CreateNewGraph(
        Blueprint, FName(*FunctionName), UEdGraph::StaticClass(), UEdGraphSchema_K2::StaticClass());

    if (!NewGraph)
    {
        return TEXT("{\"success\":false,\"error\":\"Failed to create function graph\"}");
    }

    FBlueprintEditorUtils::AddFunctionGraph(Blueprint, NewGraph, true, nullptr);

    // Set function category if provided
    if (!Category.IsEmpty())
    {
        NewGraph->GetSchema()->CreateDefaultNodesForGraph(*NewGraph);
        for (UEdGraphNode* Node : NewGraph->Nodes)
        {
            if (UK2Node_FunctionEntry* Entry = Cast<UK2Node_FunctionEntry>(Node))
            {
                Entry->MetaData.Category = FText::FromString(Category);
                break;
            }
        }
    }

    FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

    FString EntryId = GetEntryNodeId(NewGraph);

    TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
    Result->SetStringField(TEXT("graph_name"), FunctionName);
    Result->SetStringField(TEXT("function_name"), FunctionName);
    Result->SetStringField(TEXT("entry_node_id"), EntryId);
    Result->SetNumberField(TEXT("node_count"), NewGraph->Nodes.Num());

    TSharedPtr<FJsonObject> Response = MakeShareable(new FJsonObject);
    Response->SetBoolField(TEXT("success"), true);
    Response->SetObjectField(TEXT("result"), Result);

    FString Out;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Out);
    FJsonSerializer::Serialize(Response.ToSharedRef(), Writer);
    return Out;
}

FString HandleListBlueprintGraphs(const TSharedPtr<FJsonObject>& Params)
{
    FString Path = Params->GetStringField(TEXT("path"));

    UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *Path);
    if (!Blueprint)
    {
        return FString::Printf(TEXT("{\"success\":false,\"error\":\"Blueprint not found: %s\"}"), *Path);
    }

    TArray<TSharedPtr<FJsonValue>> Graphs;

    auto AddGraph = [&Graphs](UEdGraph* Graph, const FString& Type)
    {
        TSharedPtr<FJsonObject> Obj = MakeShareable(new FJsonObject);
        Obj->SetStringField(TEXT("name"), Graph->GetName());
        Obj->SetStringField(TEXT("type"), Type);
        Obj->SetNumberField(TEXT("node_count"), Graph->Nodes.Num());

        FString EntryId;
        for (UEdGraphNode* Node : Graph->Nodes)
        {
            if (Cast<UK2Node_FunctionEntry>(Node))
            {
                EntryId = Node->NodeGuid.ToString();
                break;
            }
        }
        if (!EntryId.IsEmpty())
        {
            Obj->SetStringField(TEXT("entry_node_id"), EntryId);
        }

        Graphs.Add(MakeShareable(new FJsonValueObject(Obj)));
    };

    for (UEdGraph* Graph : Blueprint->UbergraphPages)
    {
        AddGraph(Graph, TEXT("EventGraph"));
    }
    for (UEdGraph* Graph : Blueprint->FunctionGraphs)
    {
        AddGraph(Graph, TEXT("FunctionGraph"));
    }
    for (UEdGraph* Graph : Blueprint->DelegateSignatureGraphs)
    {
        AddGraph(Graph, TEXT("DelegateSignature"));
    }

    TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
    Result->SetNumberField(TEXT("count"), Graphs.Num());
    Result->SetArrayField(TEXT("graphs"), Graphs);

    TSharedPtr<FJsonObject> Response = MakeShareable(new FJsonObject);
    Response->SetBoolField(TEXT("success"), true);
    Response->SetObjectField(TEXT("result"), Result);

    FString Out;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Out);
    FJsonSerializer::Serialize(Response.ToSharedRef(), Writer);
    return Out;
}

FString HandleDeleteBlueprintGraph(const TSharedPtr<FJsonObject>& Params)
{
    FString Path = Params->GetStringField(TEXT("path"));
    FString GraphName = Params->GetStringField(TEXT("graph_name"));

    UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *Path);
    if (!Blueprint)
    {
        return FString::Printf(TEXT("{\"success\":false,\"error\":\"Blueprint not found: %s\"}"), *Path);
    }

    UEdGraph* Graph = FindGraph(Blueprint, GraphName);
    if (!Graph)
    {
        return FString::Printf(TEXT("{\"success\":false,\"error\":\"Graph not found: %s\"}"), *GraphName);
    }

    // Refuse to delete EventGraph
    for (UEdGraph* EventGraph : Blueprint->UbergraphPages)
    {
        if (EventGraph == Graph)
        {
            return TEXT("{\"success\":false,\"error\":\"Cannot delete the EventGraph\"}");
        }
    }

    FBlueprintEditorUtils::RemoveGraph(Blueprint, Graph, EGraphRemoveFlags::Recompile);
    FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

    TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
    Result->SetStringField(TEXT("graph_name"), GraphName);
    Result->SetBoolField(TEXT("deleted"), true);

    TSharedPtr<FJsonObject> Response = MakeShareable(new FJsonObject);
    Response->SetBoolField(TEXT("success"), true);
    Response->SetObjectField(TEXT("result"), Result);

    FString Out;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Out);
    FJsonSerializer::Serialize(Response.ToSharedRef(), Writer);
    return Out;
}
