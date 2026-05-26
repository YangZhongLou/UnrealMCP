#include "CoreMinimal.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "Components/MeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstance.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Editor.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "UObject/SavePackage.h"

static AActor* FindActor(UWorld* World, const FString& Name)
{
    for (TActorIterator<AActor> It(World); It; ++It)
    {
        if (It->GetName() == Name)
        {
            return *It;
        }
    }
    return nullptr;
}

static UMeshComponent* FindMeshComponent(AActor* Actor, const FString& ComponentName, int32 SlotIndex)
{
    UMeshComponent* Result = nullptr;
    if (!ComponentName.IsEmpty())
    {
        TSet<UActorComponent*> Components = Actor->GetComponents();
        for (UActorComponent* Comp : Components)
        {
            if (Comp->GetName() == ComponentName)
            {
                Result = Cast<UMeshComponent>(Comp);
                break;
            }
        }
    }
    else
    {
        TArray<UMeshComponent*> MeshComps;
        Actor->GetComponents<UMeshComponent>(MeshComps);
        if (SlotIndex >= 0 && SlotIndex < MeshComps.Num())
        {
            Result = MeshComps[SlotIndex];
        }
        else if (MeshComps.Num() > 0)
        {
            Result = MeshComps[0];
        }
    }
    return Result;
}

FString HandleSetMaterial(const TSharedPtr<FJsonObject>& Params)
{
    FString ActorName = Params->GetStringField(TEXT("actorName"));
    FString MaterialPath = Params->GetStringField(TEXT("materialPath"));
    FString ComponentName = Params->HasField(TEXT("componentName"))
        ? Params->GetStringField(TEXT("componentName"))
        : TEXT("");
    int32 SlotIndex = Params->HasField(TEXT("slotIndex"))
        ? FMath::RoundToInt(Params->GetNumberField(TEXT("slotIndex")))
        : 0;

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!World) return TEXT("{\"success\":false,\"error\":\"No world available\"}");

    AActor* Actor = FindActor(World, ActorName);
    if (!Actor) return FString::Printf(TEXT("{\"success\":false,\"error\":\"Actor not found: %s\"}"), *ActorName);

    UMeshComponent* MeshComp = FindMeshComponent(Actor, ComponentName, SlotIndex);
    if (!MeshComp) return TEXT("{\"success\":false,\"error\":\"No mesh component found on actor\"}");

    UMaterialInterface* Material = LoadObject<UMaterialInterface>(nullptr, *MaterialPath);
    if (!Material) return FString::Printf(TEXT("{\"success\":false,\"error\":\"Material not found: %s\"}"), *MaterialPath);

    MeshComp->SetMaterial(SlotIndex, Material);
    Actor->MarkPackageDirty();

    return FString::Printf(TEXT("{\"success\":true,\"result\":{\"actor\":\"%s\",\"material\":\"%s\",\"slot\":%d}}"),
        *ActorName, *MaterialPath, SlotIndex);
}

FString HandleCreateMaterialInstance(const TSharedPtr<FJsonObject>& Params)
{
    FString Path = Params->GetStringField(TEXT("path"));
    FString ParentPath = Params->GetStringField(TEXT("parentPath"));
    FString InstanceType = Params->HasField(TEXT("instanceType"))
        ? Params->GetStringField(TEXT("instanceType"))
        : TEXT("constant");

    UMaterialInterface* ParentMat = LoadObject<UMaterialInterface>(nullptr, *ParentPath);
    if (!ParentMat)
    {
        return FString::Printf(TEXT("{\"success\":false,\"error\":\"Parent material not found: %s\"}"), *ParentPath);
    }

    FString PackageName;
    FString AssetName;
    Path.Split(TEXT("."), &PackageName, &AssetName);
    if (AssetName.IsEmpty())
    {
        AssetName = FPaths::GetBaseFilename(Path);
        PackageName = Path;
    }

    UPackage* Package = CreatePackage(*PackageName);
    if (!Package)
    {
        return TEXT("{\"success\":false,\"error\":\"Failed to create package\"}");
    }

    UMaterialInstance* NewInstance = nullptr;
    if (InstanceType.Equals(TEXT("dynamic"), ESearchCase::IgnoreCase))
    {
        NewInstance = UMaterialInstanceDynamic::Create(ParentMat, Package, FName(*AssetName));
    }
    else
    {
        UMaterialInstanceConstant* MIC = NewObject<UMaterialInstanceConstant>(Package, FName(*AssetName), RF_Public | RF_Standalone);
        if (MIC)
        {
            MIC->SetParentEditorOnly(ParentMat);
            NewInstance = MIC;
        }
    }

    if (!NewInstance)
    {
        return TEXT("{\"success\":false,\"error\":\"Failed to create material instance\"}");
    }

    NewInstance->PostEditChange();
    Package->MarkPackageDirty();
    FAssetRegistryModule& AssetRegModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
    AssetRegModule.AssetCreated(NewInstance);

    FString PackageFileName = FPackageName::LongPackageNameToFilename(
        PackageName, FPackageName::GetAssetPackageExtension());
    FSavePackageArgs SaveArgs;
    SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
    UPackage::SavePackage(Package, NewInstance, *PackageFileName, SaveArgs);

    TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
    Result->SetStringField(TEXT("path"), Path);
    Result->SetStringField(TEXT("type"), InstanceType.ToLower());
    Result->SetStringField(TEXT("parent"), ParentPath);

    FString ResultStr;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ResultStr);
    FJsonSerializer::Serialize(Result.ToSharedRef(), Writer);

    return FString::Printf(TEXT("{\"success\":true,\"result\":%s}"), *ResultStr);
}

FString HandleSetMaterialParameter(const TSharedPtr<FJsonObject>& Params)
{
    FString ActorName = Params->GetStringField(TEXT("actorName"));
    FString ParameterName = Params->GetStringField(TEXT("parameterName"));
    FString ComponentName = Params->HasField(TEXT("componentName"))
        ? Params->GetStringField(TEXT("componentName"))
        : TEXT("");
    int32 SlotIndex = Params->HasField(TEXT("slotIndex"))
        ? FMath::RoundToInt(Params->GetNumberField(TEXT("slotIndex")))
        : 0;

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!World) return TEXT("{\"success\":false,\"error\":\"No world available\"}");

    AActor* Actor = FindActor(World, ActorName);
    if (!Actor) return FString::Printf(TEXT("{\"success\":false,\"error\":\"Actor not found: %s\"}"), *ActorName);

    UMeshComponent* MeshComp = FindMeshComponent(Actor, ComponentName, SlotIndex);
    if (!MeshComp) return TEXT("{\"success\":false,\"error\":\"No mesh component found\"}");

    UMaterialInterface* MatInterface = MeshComp->GetMaterial(SlotIndex);
    if (!MatInterface) return TEXT("{\"success\":false,\"error\":\"No material in slot\"}");

    UMaterialInstance* MatInstance = Cast<UMaterialInstance>(MatInterface);
    if (!MatInstance)
    {
        MatInstance = MeshComp->CreateDynamicMaterialInstance(SlotIndex, MatInterface);
    }

    if (!MatInstance) return TEXT("{\"success\":false,\"error\":\"Cannot create/modify material instance\"}");

    UMaterialInstanceDynamic* MID = Cast<UMaterialInstanceDynamic>(MatInstance);
    if (!MID)
    {
        return TEXT("{\"success\":false,\"error\":\"Cannot modify non-dynamic material instance\"}");
    }

    if (Params->HasField(TEXT("scalarValue")))
    {
        float Value = Params->GetNumberField(TEXT("scalarValue"));
        MID->SetScalarParameterValue(FName(*ParameterName), Value);
    }
    else if (Params->HasField(TEXT("vectorValue")))
    {
        const TArray<TSharedPtr<FJsonValue>>& Arr = Params->GetArrayField(TEXT("vectorValue"));
        if (Arr.Num() >= 3)
        {
            FLinearColor Color(Arr[0]->AsNumber(), Arr[1]->AsNumber(), Arr[2]->AsNumber());
            MID->SetVectorParameterValue(FName(*ParameterName), Color);
        }
    }
    else
    {
        return TEXT("{\"success\":false,\"error\":\"Provide scalarValue or vectorValue\"}");
    }

    MeshComp->MarkRenderStateDirty();

    return FString::Printf(TEXT("{\"success\":true,\"result\":{\"actor\":\"%s\",\"parameter\":\"%s\"}}"),
        *ActorName, *ParameterName);
}
