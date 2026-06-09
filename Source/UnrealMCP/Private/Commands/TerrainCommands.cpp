#if WITH_EDITOR
#include "CoreMinimal.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "Materials/MaterialInterface.h"
#include "Editor.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Async/Async.h"
#include "Misc/ScopedSlowTask.h"

#include "AHexTerrain.h"
#include "HexTerrainChunk.h"
#include "HexTerrainGenerator.h"

// ============================================================================
// Helpers
// ============================================================================

static EHexTerrainType ParseTerrainType(const FString& TypeStr)
{
	FString Lower = TypeStr.ToLower();
	if (Lower == TEXT("water")) return EHexTerrainType::Water;
	if (Lower == TEXT("sand"))  return EHexTerrainType::Sand;
	if (Lower == TEXT("grass")) return EHexTerrainType::Grass;
	if (Lower == TEXT("rock"))  return EHexTerrainType::Rock;
	if (Lower == TEXT("snow"))  return EHexTerrainType::Snow;
	return EHexTerrainType::Count;  // Invalid sentinel
}

static FString TerrainTypeToString(EHexTerrainType Type)
{
	switch (Type)
	{
	case EHexTerrainType::Water: return TEXT("Water");
	case EHexTerrainType::Sand:  return TEXT("Sand");
	case EHexTerrainType::Grass: return TEXT("Grass");
	case EHexTerrainType::Rock:  return TEXT("Rock");
	case EHexTerrainType::Snow:  return TEXT("Snow");
	default: return TEXT("Unknown");
	}
}

static AHexTerrain* FindTerrainByName(UWorld* World, const FString& Name)
{
	for (TActorIterator<AHexTerrain> It(World); It; ++It)
	{
		if (It->GetName() == Name || It->GetActorLabel() == Name)
		{
			return *It;
		}
	}
	return nullptr;
}

// ============================================================================
// HandleSetTerrainLayerTexture
// ============================================================================

FString HandleSetTerrainLayerTexture(const TSharedPtr<FJsonObject>& Params)
{
	FString TerrainName = Params->GetStringField(TEXT("terrainName"));
	FString TypeStr = Params->GetStringField(TEXT("terrainType"));
	FString ParamName = Params->GetStringField(TEXT("parameterName"));
	FString TexturePath = Params->GetStringField(TEXT("texturePath"));

	const EHexTerrainType Type = ParseTerrainType(TypeStr);
	if (Type == EHexTerrainType::Count)
	{
		return TEXT("{\"success\":false,\"error\":\"Invalid terrain type. Valid: Water, Sand, Grass, Rock, Snow\"}");
	}

	FString ErrorMsg;
	FEvent* DoneEvent = FPlatformProcess::GetSynchEventFromPool();

	AsyncTask(ENamedThreads::GameThread, [&]()
	{
		UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
		if (!World) { ErrorMsg = TEXT("No world available"); DoneEvent->Trigger(); return; }

		AHexTerrain* Terrain = FindTerrainByName(World, TerrainName);
		if (!Terrain) { ErrorMsg = FString::Printf(TEXT("Terrain not found: %s"), *TerrainName); DoneEvent->Trigger(); return; }

		UTexture* Texture = LoadObject<UTexture>(nullptr, *TexturePath);
		if (!Texture)
		{
			// Texture path may be a soft reference — try again without extension
			FString CleanPath = TexturePath;
			int32 DotIdx;
			if (CleanPath.FindLastChar('.', DotIdx))
			{
				CleanPath = CleanPath.Left(DotIdx);
			}
			Texture = LoadObject<UTexture>(nullptr, *CleanPath);
		}
		if (!Texture) { ErrorMsg = FString::Printf(TEXT("Texture not found: %s"), *TexturePath); DoneEvent->Trigger(); return; }

		int32 UpdatedChunks = 0;
		const auto& ChunkMap = Terrain->GetChunkMapConst();
		for (const auto& Pair : ChunkMap)
		{
			if (Pair.Value)
			{
				Pair.Value->SetLayerTexture(Type, FName(*ParamName), Texture);
				++UpdatedChunks;
			}
		}

		UE_LOG(LogTemp, Log, TEXT("[TerrainCommands] SetLayerTexture: terrain=%s type=%s param=%s texture=%s chunks=%d"),
			*TerrainName, *TypeStr, *ParamName, *TexturePath, UpdatedChunks);

		DoneEvent->Trigger();
	});

	DoneEvent->Wait();
	FPlatformProcess::ReturnSynchEventToPool(DoneEvent);

	if (!ErrorMsg.IsEmpty())
		return FString::Printf(TEXT("{\"success\":false,\"error\":\"%s\"}"), *ErrorMsg);

	return FString::Printf(TEXT("{\"success\":true,\"result\":{\"terrain\":\"%s\",\"type\":\"%s\",\"parameter\":\"%s\",\"texture\":\"%s\"}}"),
		*TerrainName, *TypeStr, *ParamName, *TexturePath);
}

// ============================================================================
// HandleSetTerrainLayerMaterial
// ============================================================================

FString HandleSetTerrainLayerMaterial(const TSharedPtr<FJsonObject>& Params)
{
	FString TerrainName = Params->GetStringField(TEXT("terrainName"));
	FString TypeStr = Params->GetStringField(TEXT("terrainType"));
	FString MaterialPath = Params->GetStringField(TEXT("materialPath"));

	const EHexTerrainType Type = ParseTerrainType(TypeStr);
	if (Type == EHexTerrainType::Count)
	{
		return TEXT("{\"success\":false,\"error\":\"Invalid terrain type. Valid: Water, Sand, Grass, Rock, Snow\"}");
	}

	FString ErrorMsg;
	FEvent* DoneEvent = FPlatformProcess::GetSynchEventFromPool();

	AsyncTask(ENamedThreads::GameThread, [&]()
	{
		UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
		if (!World) { ErrorMsg = TEXT("No world available"); DoneEvent->Trigger(); return; }

		AHexTerrain* Terrain = FindTerrainByName(World, TerrainName);
		if (!Terrain) { ErrorMsg = FString::Printf(TEXT("Terrain not found: %s"), *TerrainName); DoneEvent->Trigger(); return; }

		UMaterialInterface* Material = LoadObject<UMaterialInterface>(nullptr, *MaterialPath);
		if (!Material)
		{
			// Try without extension
			FString CleanPath = MaterialPath;
			int32 DotIdx;
			if (CleanPath.FindLastChar('.', DotIdx))
			{
				CleanPath = CleanPath.Left(DotIdx);
			}
			Material = LoadObject<UMaterialInterface>(nullptr, *CleanPath);
		}
		if (!Material) { ErrorMsg = FString::Printf(TEXT("Material not found: %s"), *MaterialPath); DoneEvent->Trigger(); return; }

		Terrain->SetLayerMaterial(Type, Material);

		UE_LOG(LogTemp, Log, TEXT("[TerrainCommands] SetLayerMaterial: terrain=%s type=%s material=%s"),
			*TerrainName, *TypeStr, *MaterialPath);

		DoneEvent->Trigger();
	});

	DoneEvent->Wait();
	FPlatformProcess::ReturnSynchEventToPool(DoneEvent);

	if (!ErrorMsg.IsEmpty())
		return FString::Printf(TEXT("{\"success\":false,\"error\":\"%s\"}"), *ErrorMsg);

	return FString::Printf(TEXT("{\"success\":true,\"result\":{\"terrain\":\"%s\",\"type\":\"%s\",\"material\":\"%s\"}}"),
		*TerrainName, *TypeStr, *MaterialPath);
}

// ============================================================================
// HandleGetTerrainLayerInfo
// ============================================================================

FString HandleGetTerrainLayerInfo(const TSharedPtr<FJsonObject>& Params)
{
	FString TerrainName = Params->GetStringField(TEXT("terrainName"));

	FString ErrorMsg;
	FString ResultStr;
	FEvent* DoneEvent = FPlatformProcess::GetSynchEventFromPool();

	AsyncTask(ENamedThreads::GameThread, [&]()
	{
		UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
		if (!World) { ErrorMsg = TEXT("No world available"); DoneEvent->Trigger(); return; }

		AHexTerrain* Terrain = FindTerrainByName(World, TerrainName);
		if (!Terrain) { ErrorMsg = FString::Printf(TEXT("Terrain not found: %s"), *TerrainName); DoneEvent->Trigger(); return; }

		TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
		Result->SetStringField(TEXT("terrainName"), Terrain->GetName());
		Result->SetNumberField(TEXT("gridRadius"), Terrain->GridRadius);
		Result->SetNumberField(TEXT("cellRadius"), Terrain->CellRadius);
		Result->SetNumberField(TEXT("cellCount"), Terrain->GetTerrainCells().Num());
		Result->SetNumberField(TEXT("chunkCount"), Terrain->GetChunkCount());
		Result->SetNumberField(TEXT("textureTileSize"), Terrain->TextureTileSize);
		Result->SetBoolField(TEXT("bManualMode"), Terrain->bManualMode);

		// Per-terrain-type layer materials
		TArray<TSharedPtr<FJsonValue>> LayersArr;
		for (uint8 i = 0; i < static_cast<uint8>(EHexTerrainType::Count); ++i)
		{
			const EHexTerrainType Type = static_cast<EHexTerrainType>(i);
			TSharedPtr<FJsonObject> LayerObj = MakeShareable(new FJsonObject);
			LayerObj->SetStringField(TEXT("type"), TerrainTypeToString(Type));

			// Check chunk sections to see if this type has a MID
			int32 ChunksWithType = 0;
			const auto& ChunkMap = Terrain->GetChunkMapConst();
			for (const auto& Pair : ChunkMap)
			{
				if (Pair.Value && Pair.Value->FindSectionByType(Type) != INDEX_NONE)
				{
					++ChunksWithType;
				}
			}
			LayerObj->SetNumberField(TEXT("activeChunks"), ChunksWithType);
			LayersArr.Add(MakeShareable(new FJsonValueObject(LayerObj)));
		}
		Result->SetArrayField(TEXT("layers"), LayersArr);

		TSharedPtr<FJsonObject> ResponseJson = MakeShareable(new FJsonObject);
		ResponseJson->SetBoolField(TEXT("success"), true);
		ResponseJson->SetObjectField(TEXT("result"), Result);

		TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ResultStr);
		FJsonSerializer::Serialize(ResponseJson.ToSharedRef(), Writer);

		DoneEvent->Trigger();
	});

	DoneEvent->Wait();
	FPlatformProcess::ReturnSynchEventToPool(DoneEvent);

	if (!ErrorMsg.IsEmpty())
		return FString::Printf(TEXT("{\"success\":false,\"error\":\"%s\"}"), *ErrorMsg);

	return ResultStr;
}

#endif
