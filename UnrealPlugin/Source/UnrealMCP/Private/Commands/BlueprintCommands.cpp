#include "CoreMinimal.h"
#include "Engine/Blueprint.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"

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
