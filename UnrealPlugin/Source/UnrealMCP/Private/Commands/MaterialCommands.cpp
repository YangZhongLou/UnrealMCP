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
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "Factories/MaterialInstanceConstantFactoryNew.h"
#include "Factories/MaterialFactoryNew.h"
#include "UObject/SavePackage.h"
#include "Async/Async.h"

static AActor* FindActor(UWorld* World, const FString& Name)
{
	AActor* Result = nullptr;
	FEvent* Done = FPlatformProcess::GetSynchEventFromPool();
	AsyncTask(ENamedThreads::GameThread, [&]()
	{
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			if (It->GetName() == Name) { Result = *It; break; }
		}
		Done->Trigger();
	});
	Done->Wait();
	FPlatformProcess::ReturnSynchEventToPool(Done);
	return Result;
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

FString HandleCreateMaterial(const TSharedPtr<FJsonObject>& Params)
{
	FString Path = Params->GetStringField(TEXT("path"));

	FString ErrorMsg;
	TSharedPtr<FJsonObject> ResponseJson;
	FEvent* DoneEvent = FPlatformProcess::GetSynchEventFromPool();

	AsyncTask(ENamedThreads::GameThread, [&]()
	{
		FString AssetName = FPaths::GetBaseFilename(Path);
		FString PackagePath = FPaths::GetPath(Path);

		IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
		UMaterialFactoryNew* Factory = NewObject<UMaterialFactoryNew>();
		Factory->bEditAfterNew = false;
		UObject* NewAsset = AssetTools.CreateAsset(
			AssetName, PackagePath, UMaterial::StaticClass(), Factory);

		if (!NewAsset)
		{
			ErrorMsg = TEXT("Failed to create material");
			DoneEvent->Trigger();
			return;
		}

		TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
		Result->SetStringField(TEXT("path"), Path);
		Result->SetStringField(TEXT("assetName"), AssetName);

		ResponseJson = MakeShareable(new FJsonObject);
		ResponseJson->SetBoolField(TEXT("success"), true);
		ResponseJson->SetObjectField(TEXT("result"), Result);

		DoneEvent->Trigger();
	});

	DoneEvent->Wait();
	FPlatformProcess::ReturnSynchEventToPool(DoneEvent);

	if (!ErrorMsg.IsEmpty())
		return FString::Printf(TEXT("{\"success\":false,\"error\":\"%s\"}"), *ErrorMsg);

	FString Out;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Out);
	FJsonSerializer::Serialize(ResponseJson.ToSharedRef(), Writer);
	return Out;
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

	FString ErrorMsg;
	FString ResultStr;
	FEvent* DoneEvent = FPlatformProcess::GetSynchEventFromPool();

	AsyncTask(ENamedThreads::GameThread, [&]()
	{
		UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
		if (!World) { ErrorMsg = TEXT("No world available"); DoneEvent->Trigger(); return; }

		AActor* Actor = nullptr;
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			if (It->GetName() == ActorName) { Actor = *It; break; }
		}
		if (!Actor) { ErrorMsg = FString::Printf(TEXT("Actor not found: %s"), *ActorName); DoneEvent->Trigger(); return; }

		UMeshComponent* MeshComp = FindMeshComponent(Actor, ComponentName, SlotIndex);
		if (!MeshComp) { ErrorMsg = TEXT("No mesh component found on actor"); DoneEvent->Trigger(); return; }

		UMaterialInterface* Material = LoadObject<UMaterialInterface>(nullptr, *MaterialPath);
		if (!Material) { ErrorMsg = FString::Printf(TEXT("Material not found: %s"), *MaterialPath); DoneEvent->Trigger(); return; }

		MeshComp->SetMaterial(SlotIndex, Material);
		Actor->MarkPackageDirty();

		ResultStr = FString::Printf(TEXT("{\"actor\":\"%s\",\"material\":\"%s\",\"slot\":%d}"),
			*ActorName, *MaterialPath, SlotIndex);

		DoneEvent->Trigger();
	});

	DoneEvent->Wait();
	FPlatformProcess::ReturnSynchEventToPool(DoneEvent);

	if (!ErrorMsg.IsEmpty())
		return FString::Printf(TEXT("{\"success\":false,\"error\":\"%s\"}"), *ErrorMsg);

	return FString::Printf(TEXT("{\"success\":true,\"result\":%s}"), *ResultStr);
}

FString HandleCreateMaterialInstance(const TSharedPtr<FJsonObject>& Params)
{
	FString Path = Params->GetStringField(TEXT("path"));
	FString ParentPath = Params->GetStringField(TEXT("parentPath"));
	FString InstanceType = Params->HasField(TEXT("instanceType"))
		? Params->GetStringField(TEXT("instanceType"))
		: TEXT("constant");

	FString ErrorMsg;
	TSharedPtr<FJsonObject> ResponseJson;
	FEvent* DoneEvent = FPlatformProcess::GetSynchEventFromPool();

	AsyncTask(ENamedThreads::GameThread, [&]()
	{
		UMaterialInterface* ParentMat = LoadObject<UMaterialInterface>(nullptr, *ParentPath);
		if (!ParentMat)
		{
			ErrorMsg = FString::Printf(TEXT("Parent material not found: %s"), *ParentPath);
			DoneEvent->Trigger();
			return;
		}

		FString AssetName = FPaths::GetBaseFilename(Path);
		FString PackagePath = FPaths::GetPath(Path);

		UMaterialInstance* NewInstance = nullptr;
		if (InstanceType.Equals(TEXT("dynamic"), ESearchCase::IgnoreCase))
		{
			UPackage* Package = CreatePackage(*(PackagePath / AssetName));
			if (Package)
			{
				NewInstance = UMaterialInstanceDynamic::Create(ParentMat, Package, FName(*AssetName));
			}
		}
		else
		{
			IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
			UMaterialInstanceConstantFactoryNew* Factory = NewObject<UMaterialInstanceConstantFactoryNew>();
			Factory->InitialParent = ParentMat;
			Factory->bEditAfterNew = false;
			NewInstance = Cast<UMaterialInstance>(AssetTools.CreateAsset(
				AssetName, PackagePath, UMaterialInstanceConstant::StaticClass(), Factory));
		}

		if (!NewInstance)
		{
			ErrorMsg = TEXT("Failed to create material instance");
			DoneEvent->Trigger();
			return;
		}

		TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
		Result->SetStringField(TEXT("path"), Path);
		Result->SetStringField(TEXT("type"), InstanceType.ToLower());
		Result->SetStringField(TEXT("parent"), ParentPath);

		ResponseJson = MakeShareable(new FJsonObject);
		ResponseJson->SetBoolField(TEXT("success"), true);
		ResponseJson->SetObjectField(TEXT("result"), Result);

		DoneEvent->Trigger();
	});

	DoneEvent->Wait();
	FPlatformProcess::ReturnSynchEventToPool(DoneEvent);

	if (!ErrorMsg.IsEmpty())
		return FString::Printf(TEXT("{\"success\":false,\"error\":\"%s\"}"), *ErrorMsg);

	FString Out;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Out);
	FJsonSerializer::Serialize(ResponseJson.ToSharedRef(), Writer);
	return Out;
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
	bool bHasScalar = Params->HasField(TEXT("scalarValue"));
	bool bHasVector = Params->HasField(TEXT("vectorValue"));
	float ScalarValue = bHasScalar ? (float)Params->GetNumberField(TEXT("scalarValue")) : 0.0f;
	FString VectorJson;
	if (bHasVector)
	{
		const TArray<TSharedPtr<FJsonValue>>& Arr = Params->GetArrayField(TEXT("vectorValue"));
		if (Arr.Num() >= 3)
			VectorJson = FString::Printf(TEXT("[%f,%f,%f]"), Arr[0]->AsNumber(), Arr[1]->AsNumber(), Arr[2]->AsNumber());
	}

	FString ErrorMsg;
	FString ResultStr;
	FEvent* DoneEvent = FPlatformProcess::GetSynchEventFromPool();

	AsyncTask(ENamedThreads::GameThread, [&]()
	{
		UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
		if (!World) { ErrorMsg = TEXT("No world available"); DoneEvent->Trigger(); return; }

		AActor* Actor = nullptr;
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			if (It->GetName() == ActorName) { Actor = *It; break; }
		}
		if (!Actor) { ErrorMsg = FString::Printf(TEXT("Actor not found: %s"), *ActorName); DoneEvent->Trigger(); return; }

		UMeshComponent* MeshComp = FindMeshComponent(Actor, ComponentName, SlotIndex);
		if (!MeshComp) { ErrorMsg = TEXT("No mesh component found"); DoneEvent->Trigger(); return; }

		UMaterialInterface* MatInterface = MeshComp->GetMaterial(SlotIndex);
		if (!MatInterface) { ErrorMsg = TEXT("No material in slot"); DoneEvent->Trigger(); return; }

		UMaterialInstance* MatInstance = Cast<UMaterialInstance>(MatInterface);
		if (!MatInstance)
			MatInstance = MeshComp->CreateDynamicMaterialInstance(SlotIndex, MatInterface);
		if (!MatInstance) { ErrorMsg = TEXT("Cannot create/modify material instance"); DoneEvent->Trigger(); return; }

			UMaterialInstanceDynamic* MID = Cast<UMaterialInstanceDynamic>(MatInstance);
			if (!MID)
			{
				MID = MeshComp->CreateDynamicMaterialInstance(SlotIndex, MatInterface);
				if (!MID) { ErrorMsg = TEXT("Cannot create dynamic material instance"); DoneEvent->Trigger(); return; }
			}

		if (bHasScalar)
			MID->SetScalarParameterValue(FName(*ParameterName), ScalarValue);
		else if (bHasVector)
		{
			const TArray<TSharedPtr<FJsonValue>>& Arr = Params->GetArrayField(TEXT("vectorValue"));
			FLinearColor Color(Arr[0]->AsNumber(), Arr[1]->AsNumber(), Arr[2]->AsNumber());
			MID->SetVectorParameterValue(FName(*ParameterName), Color);
		}
		else
		{
			ErrorMsg = TEXT("Provide scalarValue or vectorValue");
			DoneEvent->Trigger();
			return;
		}

		MeshComp->MarkRenderStateDirty();
		ResultStr = FString::Printf(TEXT("{\"actor\":\"%s\",\"parameter\":\"%s\"}"), *ActorName, *ParameterName);
		DoneEvent->Trigger();
	});

	DoneEvent->Wait();
	FPlatformProcess::ReturnSynchEventToPool(DoneEvent);

	if (!ErrorMsg.IsEmpty())
		return FString::Printf(TEXT("{\"success\":false,\"error\":\"%s\"}"), *ErrorMsg);

	return FString::Printf(TEXT("{\"success\":true,\"result\":%s}"), *ResultStr);
}
