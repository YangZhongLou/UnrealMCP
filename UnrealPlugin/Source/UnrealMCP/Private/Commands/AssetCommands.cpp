#include "CoreMinimal.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "EditorAssetLibrary.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"

FString HandleGetAssetList(const TSharedPtr<FJsonObject>& Params)
{
    FString Path = Params->GetStringField(TEXT("path"));
    if (Path.IsEmpty())
    {
        Path = TEXT("/Game");
    }

    TArray<FAssetData> AssetDataList;
    FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
    AssetRegistryModule.Get().GetAssetsByPath(FName(*Path), AssetDataList, true);

    TArray<TSharedPtr<FJsonValue>> Assets;
    for (const FAssetData& AssetData : AssetDataList)
    {
        TSharedPtr<FJsonObject> Obj = MakeShareable(new FJsonObject);
        Obj->SetStringField(TEXT("name"), AssetData.AssetName.ToString());
        Obj->SetStringField(TEXT("path"), AssetData.GetObjectPathString());
        Obj->SetStringField(TEXT("class"), AssetData.AssetClassPath.ToString());
        Assets.Add(MakeShareable(new FJsonValueObject(Obj)));
    }

    TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
    Result->SetArrayField(TEXT("assets"), Assets);
    Result->SetNumberField(TEXT("count"), Assets.Num());

    TSharedPtr<FJsonObject> Response = MakeShareable(new FJsonObject);
    Response->SetBoolField(TEXT("success"), true);
    Response->SetObjectField(TEXT("result"), Result);

    FString Out;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Out);
    FJsonSerializer::Serialize(Response.ToSharedRef(), Writer);
    return Out;
}

FString HandleGetAssetInfo(const TSharedPtr<FJsonObject>& Params)
{
    FString Path = Params->GetStringField(TEXT("path"));

    FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
    FAssetData AssetData = AssetRegistryModule.Get().GetAssetByObjectPath(FSoftObjectPath(Path));

    if (!AssetData.IsValid())
    {
        return FString::Printf(TEXT("{\"success\":false,\"error\":\"Asset not found: %s\"}"), *Path);
    }

    TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
    Result->SetStringField(TEXT("name"), AssetData.AssetName.ToString());
    Result->SetStringField(TEXT("path"), AssetData.GetObjectPathString());
    Result->SetStringField(TEXT("class"), AssetData.AssetClassPath.ToString());
    Result->SetStringField(TEXT("package_path"), AssetData.PackagePath.ToString());

    TSharedPtr<FJsonObject> Response = MakeShareable(new FJsonObject);
    Response->SetBoolField(TEXT("success"), true);
    Response->SetObjectField(TEXT("result"), Result);

    FString Out;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Out);
    FJsonSerializer::Serialize(Response.ToSharedRef(), Writer);
    return Out;
}

FString HandleDeleteAsset(const TSharedPtr<FJsonObject>& Params)
{
    FString Path = Params->GetStringField(TEXT("path"));

    bool bDeleted = UEditorAssetLibrary::DeleteAsset(Path);

    if (bDeleted)
    {
        return TEXT("{\"success\":true,\"result\":{\"deleted\":true}}");
    }
    return FString::Printf(TEXT("{\"success\":false,\"error\":\"Failed to delete asset: %s\"}"), *Path);
}

FString HandleRenameAsset(const TSharedPtr<FJsonObject>& Params)
{
    FString Path = Params->GetStringField(TEXT("path"));
    FString NewName = Params->GetStringField(TEXT("newName"));

    bool bRenamed = UEditorAssetLibrary::RenameAsset(Path, NewName);

    if (bRenamed)
    {
        return TEXT("{\"success\":true,\"result\":{\"renamed\":true}}");
    }
    return TEXT("{\"success\":false,\"error\":\"Failed to rename asset\"}");
}
