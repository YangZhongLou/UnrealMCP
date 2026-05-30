#if WITH_EDITOR
#include "CoreMinimal.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "EditorAssetLibrary.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "Misc/Paths.h"
#include "Async/Async.h"

FString HandleGetAssetList(const TSharedPtr<FJsonObject>& Params)
{
    FString Path = Params->GetStringField(TEXT("path"));
    if (Path.IsEmpty()) { Path = TEXT("/Game"); }

    TArray<TSharedPtr<FJsonValue>> Assets;
    FEvent* DoneEvent = FPlatformProcess::GetSynchEventFromPool();

    AsyncTask(ENamedThreads::GameThread, [&]()
    {
        TArray<FAssetData> AssetDataList;
        FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
        AssetRegistryModule.Get().GetAssetsByPath(FName(*Path), AssetDataList, true);

        for (const FAssetData& AssetData : AssetDataList)
        {
            TSharedPtr<FJsonObject> Obj = MakeShareable(new FJsonObject);
            Obj->SetStringField(TEXT("name"), AssetData.AssetName.ToString());
            Obj->SetStringField(TEXT("path"), AssetData.GetObjectPathString());
            Obj->SetStringField(TEXT("class"), AssetData.AssetClassPath.ToString());
            Assets.Add(MakeShareable(new FJsonValueObject(Obj)));
        }
        DoneEvent->Trigger();
    });

    DoneEvent->Wait();
    FPlatformProcess::ReturnSynchEventToPool(DoneEvent);

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

    TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
    FString ErrorMsg;
    FEvent* DoneEvent = FPlatformProcess::GetSynchEventFromPool();

    AsyncTask(ENamedThreads::GameThread, [&]()
    {
        FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
        FAssetData AssetData = AssetRegistryModule.Get().GetAssetByObjectPath(FSoftObjectPath(Path));

        if (!AssetData.IsValid()) { ErrorMsg = FString::Printf(TEXT("Asset not found: %s"), *Path); DoneEvent->Trigger(); return; }

        Result->SetStringField(TEXT("name"), AssetData.AssetName.ToString());
        Result->SetStringField(TEXT("path"), AssetData.GetObjectPathString());
        Result->SetStringField(TEXT("class"), AssetData.AssetClassPath.ToString());
        Result->SetStringField(TEXT("package_path"), AssetData.PackagePath.ToString());
        DoneEvent->Trigger();
    });

    DoneEvent->Wait();
    FPlatformProcess::ReturnSynchEventToPool(DoneEvent);

    if (!ErrorMsg.IsEmpty())
        return FString::Printf(TEXT("{\"success\":false,\"error\":\"%s\"}"), *ErrorMsg);

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

    bool bDeleted = false;
    FEvent* DoneEvent = FPlatformProcess::GetSynchEventFromPool();

    AsyncTask(ENamedThreads::GameThread, [&]()
    {
        bDeleted = UEditorAssetLibrary::DeleteAsset(Path);
        DoneEvent->Trigger();
    });

    DoneEvent->Wait();
    FPlatformProcess::ReturnSynchEventToPool(DoneEvent);

    if (bDeleted)
        return TEXT("{\"success\":true,\"result\":{\"deleted\":true}}");
    return FString::Printf(TEXT("{\"success\":false,\"error\":\"Failed to delete asset: %s\"}"), *Path);
}

FString HandleRenameAsset(const TSharedPtr<FJsonObject>& Params)
{
    FString Path = Params->GetStringField(TEXT("path"));
    FString NewName = Params->GetStringField(TEXT("newName"));

    bool bRenamed = false;
    FEvent* DoneEvent = FPlatformProcess::GetSynchEventFromPool();

    AsyncTask(ENamedThreads::GameThread, [&]()
    {
        bRenamed = UEditorAssetLibrary::RenameAsset(Path, NewName);
        DoneEvent->Trigger();
    });

    DoneEvent->Wait();
    FPlatformProcess::ReturnSynchEventToPool(DoneEvent);

    if (bRenamed)
        return TEXT("{\"success\":true,\"result\":{\"renamed\":true}}");
    return TEXT("{\"success\":false,\"error\":\"Failed to rename asset\"}");
}

FString HandleImportAsset(const TSharedPtr<FJsonObject>& Params)
{
    FString FilePath = Params->GetStringField(TEXT("file_path"));
    FString DestinationPath = Params->HasField(TEXT("destination_path"))
        ? Params->GetStringField(TEXT("destination_path"))
        : TEXT("/Game");

    TArray<TSharedPtr<FJsonValue>> Assets;
    FEvent* DoneEvent = FPlatformProcess::GetSynchEventFromPool();

    AsyncTask(ENamedThreads::GameThread, [&]()
    {
        TArray<FString> Files = { FilePath };
        FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");
        TArray<UObject*> ImportedAssets = AssetToolsModule.Get().ImportAssets(Files, DestinationPath);

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
        DoneEvent->Trigger();
    });

    DoneEvent->Wait();
    FPlatformProcess::ReturnSynchEventToPool(DoneEvent);

    if (Assets.Num() == 0)
        return FString::Printf(TEXT("{\"success\":false,\"error\":\"Failed to import: %s\"}"), *FilePath);

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

    TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
    FString ErrorMsg;
    FEvent* DoneEvent = FPlatformProcess::GetSynchEventFromPool();

    AsyncTask(ENamedThreads::GameThread, [&]()
    {
        UObject* Asset = LoadObject<UObject>(nullptr, *AssetPath);
        if (!Asset) { ErrorMsg = FString::Printf(TEXT("Asset not found: %s"), *AssetPath); DoneEvent->Trigger(); return; }

        FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");
        TArray<FString> AssetsToExport = { AssetPath };
        AssetToolsModule.Get().ExportAssets(AssetsToExport, OutputDir);

        Result->SetStringField(TEXT("asset_path"), AssetPath);
        Result->SetStringField(TEXT("output_dir"), OutputDir);
        Result->SetStringField(TEXT("asset_name"), Asset->GetName());
        DoneEvent->Trigger();
    });

    DoneEvent->Wait();
    FPlatformProcess::ReturnSynchEventToPool(DoneEvent);

    if (!ErrorMsg.IsEmpty())
        return FString::Printf(TEXT("{\"success\":false,\"error\":\"%s\"}"), *ErrorMsg);

    TSharedPtr<FJsonObject> Response = MakeShareable(new FJsonObject);
    Response->SetBoolField(TEXT("success"), true);
    Response->SetObjectField(TEXT("result"), Result);

    FString Out;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Out);
    FJsonSerializer::Serialize(Response.ToSharedRef(), Writer);
    return Out;
}

#endif
