#include "CoreMinimal.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "EditorAssetLibrary.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "Misc/Paths.h"

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

FString HandleImportAsset(const TSharedPtr<FJsonObject>& Params)
{
    FString FilePath = Params->GetStringField(TEXT("file_path"));
    FString DestinationPath = Params->HasField(TEXT("destination_path"))
        ? Params->GetStringField(TEXT("destination_path"))
        : TEXT("/Game");

    TArray<FString> Files = { FilePath };

    FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");
    TArray<UObject*> ImportedAssets = AssetToolsModule.Get().ImportAssets(Files, DestinationPath);

    if (ImportedAssets.Num() == 0)
    {
        return FString::Printf(TEXT("{\"success\":false,\"error\":\"Failed to import: %s\"}"), *FilePath);
    }

    TArray<TSharedPtr<FJsonValue>> Assets;
    for (UObject* Asset : ImportedAssets)
    {
        if (Asset)
        {
            TSharedPtr<FJsonObject> Obj = MakeShareable(new FJsonObject);
            Obj->SetStringField(TEXT("name"), Asset->GetName());
            Obj->SetStringField(TEXT("path"), Asset->GetPathName());
            Obj->SetStringField(TEXT("class"), Asset->GetClass()->GetName());
            Assets.Add(MakeShareable(new FJsonValueObject(Obj)));
        }
    }

    TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
    Result->SetStringField(TEXT("source_file"), FilePath);
    Result->SetStringField(TEXT("destination_path"), DestinationPath);
    Result->SetNumberField(TEXT("count"), Assets.Num());
    Result->SetArrayField(TEXT("imported"), Assets);

    TSharedPtr<FJsonObject> Response = MakeShareable(new FJsonObject);
    Response->SetBoolField(TEXT("success"), true);
    Response->SetObjectField(TEXT("result"), Result);

    FString Out;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Out);
    FJsonSerializer::Serialize(Response.ToSharedRef(), Writer);
    return Out;
}

FString HandleExportAsset(const TSharedPtr<FJsonObject>& Params)
{
    FString AssetPath = Params->GetStringField(TEXT("asset_path"));
    FString OutputDir = Params->HasField(TEXT("output_dir"))
        ? Params->GetStringField(TEXT("output_dir"))
        : FPaths::ProjectSavedDir() / TEXT("Exports");

    UObject* Asset = LoadObject<UObject>(nullptr, *AssetPath);
    if (!Asset)
    {
        return FString::Printf(TEXT("{\"success\":false,\"error\":\"Asset not found: %s\"}"), *AssetPath);
    }

    // Export using AssetTools
    FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");
    TArray<FString> AssetsToExport = { AssetPath };
    AssetToolsModule.Get().ExportAssets(AssetsToExport, OutputDir);

    TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
    Result->SetStringField(TEXT("asset_path"), AssetPath);
    Result->SetStringField(TEXT("output_dir"), OutputDir);
    Result->SetStringField(TEXT("asset_name"), Asset->GetName());

    TSharedPtr<FJsonObject> Response = MakeShareable(new FJsonObject);
    Response->SetBoolField(TEXT("success"), true);
    Response->SetObjectField(TEXT("result"), Result);

    FString Out;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Out);
    FJsonSerializer::Serialize(Response.ToSharedRef(), Writer);
    return Out;
}
