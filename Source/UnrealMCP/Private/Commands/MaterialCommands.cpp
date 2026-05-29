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
#include "Materials/MaterialExpressionConstant3Vector.h"
#include "Materials/MaterialExpressionConstant.h"
#include "Materials/MaterialExpressionFresnel.h"
#include "Materials/MaterialExpressionMultiply.h"
#include "Materials/MaterialExpressionNoise.h"
#include "Materials/MaterialExpressionTextureCoordinate.h"
#include "Materials/MaterialExpressionTextureSampleParameter2D.h"
#include "Materials/MaterialExpressionLinearInterpolate.h"
#include "Materials/MaterialExpressionOneMinus.h"
#include "MaterialEditingLibrary.h"
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
#include "Engine/SubsurfaceProfile.h"
#include "Misc/ScopedSlowTask.h"
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

	// Parse optional params
	FString ShadingModelStr = Params->HasField(TEXT("shadingModel"))
		? Params->GetStringField(TEXT("shadingModel")) : TEXT("");
	FString BlendModeStr = Params->HasField(TEXT("blendMode"))
		? Params->GetStringField(TEXT("blendMode")) : TEXT("");
	bool bReuse = Params->HasField(TEXT("reuse")) ? Params->GetBoolField(TEXT("reuse")) : false;

	bool bHasBaseColor = Params->HasField(TEXT("baseColor"));
	FLinearColor BaseColor(0, 0, 0);
	if (bHasBaseColor)
	{
		const TArray<TSharedPtr<FJsonValue>>& Arr = Params->GetArrayField(TEXT("baseColor"));
		if (Arr.Num() >= 3)
			BaseColor = FLinearColor(Arr[0]->AsNumber(), Arr[1]->AsNumber(), Arr[2]->AsNumber());
		else
			bHasBaseColor = false;
	}
	bool bHasMetallic = Params->HasField(TEXT("metallic"));
	float MetallicVal = bHasMetallic ? (float)Params->GetNumberField(TEXT("metallic")) : 0.0f;
	bool bHasRoughness = Params->HasField(TEXT("roughness"));
	float RoughnessVal = bHasRoughness ? (float)Params->GetNumberField(TEXT("roughness")) : 0.5f;
	bool bHasSpecular = Params->HasField(TEXT("specular"));
	float SpecularVal = bHasSpecular ? (float)Params->GetNumberField(TEXT("specular")) : 0.5f;
	bool bHasFresnel = Params->HasField(TEXT("fresnelExponent"));
	float FresnelExponent = bHasFresnel ? (float)Params->GetNumberField(TEXT("fresnelExponent")) : 3.0f;
	bool bHasClouds = Params->HasField(TEXT("proceduralClouds")) && Params->GetBoolField(TEXT("proceduralClouds"));
	bool bHasTexture = Params->HasField(TEXT("texture"));
	FString TexturePath = bHasTexture ? Params->GetStringField(TEXT("texture")) : TEXT("");
	bool bHasLerpB = Params->HasField(TEXT("lerpColorB"));
	FLinearColor LerpColorB(0,0,0);
	if (bHasLerpB) {
		const TArray<TSharedPtr<FJsonValue>>& LArr = Params->GetArrayField(TEXT("lerpColorB"));
		if (LArr.Num() >= 3) LerpColorB = FLinearColor(LArr[0]->AsNumber(), LArr[1]->AsNumber(), LArr[2]->AsNumber());
		else bHasLerpB = false;
	}
	float LerpAlpha = Params->HasField(TEXT("lerpAlpha")) ? (float)Params->GetNumberField(TEXT("lerpAlpha")) : 0.5f;
	bool bHasOneMinus = Params->HasField(TEXT("oneMinusValue"));
	float OneMinusVal = bHasOneMinus ? (float)Params->GetNumberField(TEXT("oneMinusValue")) : 0.5f;
	bool bHasProperties = bHasBaseColor || bHasMetallic || bHasRoughness || bHasSpecular || bHasFresnel || bHasClouds || bHasTexture || bHasLerpB || bHasOneMinus;

	FString ErrorMsg;
	TSharedPtr<FJsonObject> ResponseJson;
	FEvent* DoneEvent = FPlatformProcess::GetSynchEventFromPool();

	AsyncTask(ENamedThreads::GameThread, [&]()
	{
		FScopedSlowTask SlowTask(0, FText(), false);

		FString AssetName = FPaths::GetBaseFilename(Path);
		FString PackagePath = FPaths::GetPath(Path);
		FString FullName = PackagePath / AssetName;

		// Check if asset already exists
		if (LoadObject<UMaterial>(nullptr, *(FullName + TEXT(".") + AssetName)))
		{
			if (bReuse)
			{
				TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
				Result->SetStringField(TEXT("path"), Path);
				Result->SetStringField(TEXT("assetName"), AssetName);
				Result->SetBoolField(TEXT("reused"), true);
				ResponseJson = MakeShareable(new FJsonObject);
				ResponseJson->SetBoolField(TEXT("success"), true);
				ResponseJson->SetObjectField(TEXT("result"), Result);
				DoneEvent->Trigger();
				return;
			}
			ErrorMsg = FString::Printf(TEXT("Material already exists: %s"), *FullName);
			DoneEvent->Trigger();
			return;
		}

		UPackage* Package = CreatePackage(*FullName);
		if (!Package)
		{
			ErrorMsg = TEXT("Failed to create package");
			DoneEvent->Trigger();
			return;
		}

		UMaterial* NewMaterial = NewObject<UMaterial>(Package, FName(*AssetName), RF_Public | RF_Standalone);
		if (!NewMaterial)
		{
			ErrorMsg = TEXT("Failed to create material object");
			DoneEvent->Trigger();
			return;
		}

		// Set blend mode
		if (!BlendModeStr.IsEmpty())
		{
			if (BlendModeStr.Equals(TEXT("opaque"), ESearchCase::IgnoreCase))
				NewMaterial->BlendMode = BLEND_Opaque;
			else if (BlendModeStr.Equals(TEXT("masked"), ESearchCase::IgnoreCase))
				NewMaterial->BlendMode = BLEND_Masked;
			else if (BlendModeStr.Equals(TEXT("translucent"), ESearchCase::IgnoreCase))
				NewMaterial->BlendMode = BLEND_Translucent;
			else if (BlendModeStr.Equals(TEXT("additive"), ESearchCase::IgnoreCase))
				NewMaterial->BlendMode = BLEND_Additive;
			else if (BlendModeStr.Equals(TEXT("modulate"), ESearchCase::IgnoreCase))
				NewMaterial->BlendMode = BLEND_Modulate;
		}

		// Set shading model
		if (!ShadingModelStr.IsEmpty())
		{
			EMaterialShadingModel SM = MSM_DefaultLit;
			if (ShadingModelStr.Equals(TEXT("default_lit"), ESearchCase::IgnoreCase))
				SM = MSM_DefaultLit;
			else if (ShadingModelStr.Equals(TEXT("unlit"), ESearchCase::IgnoreCase))
				SM = MSM_Unlit;
			else if (ShadingModelStr.Equals(TEXT("subsurface"), ESearchCase::IgnoreCase))
				SM = MSM_Subsurface;
			else if (ShadingModelStr.Equals(TEXT("subsurface_profile"), ESearchCase::IgnoreCase))
				SM = MSM_SubsurfaceProfile;
			else if (ShadingModelStr.Equals(TEXT("clear_coat"), ESearchCase::IgnoreCase))
				SM = MSM_ClearCoat;
			else if (ShadingModelStr.Equals(TEXT("thin_translucent"), ESearchCase::IgnoreCase))
				SM = MSM_ThinTranslucent;
			NewMaterial->SetShadingModel(SM);
		}

		// Set subsurface profile if provided (auto-create if not found)
		if (Params->HasField(TEXT("subsurfaceProfile")))
		{
			FString ProfilePath = Params->GetStringField(TEXT("subsurfaceProfile"));
			USubsurfaceProfile* Profile = LoadObject<USubsurfaceProfile>(nullptr, *ProfilePath);
			if (!Profile)
			{
				FString ProfileName = FPaths::GetBaseFilename(ProfilePath);
				FString ProfilePkgPath = FPaths::GetPath(ProfilePath);
				UPackage* ProfilePkg = CreatePackage(*(ProfilePkgPath / ProfileName));
				if (ProfilePkg)
				{
					Profile = NewObject<USubsurfaceProfile>(ProfilePkg, FName(*ProfileName), RF_Public | RF_Standalone);
					if (Profile)
					{
						FAssetRegistryModule::AssetCreated(Profile);
						Profile->MarkPackageDirty();
					}
				}
			}
			if (Profile)
			{
				NewMaterial->SubsurfaceProfile = Profile;
			}
		}

		// Mark for modification and snapshot pre-edit state
		if (bHasProperties)
		{
			NewMaterial->Modify();
			NewMaterial->PreEditChange(nullptr);
		}

		// Create expression nodes for material properties
		if (bHasBaseColor)
		{
			UMaterialExpression* Expr = UMaterialEditingLibrary::CreateMaterialExpression(
				NewMaterial, UMaterialExpressionConstant3Vector::StaticClass(), -400, 0);
			UMaterialExpressionConstant3Vector* BaseColorExpr = Cast<UMaterialExpressionConstant3Vector>(Expr);
			if (BaseColorExpr)
			{
				BaseColorExpr->Constant = BaseColor;
				UMaterialEditingLibrary::ConnectMaterialProperty(BaseColorExpr, TEXT(""), MP_BaseColor);
			}
		}
		if (bHasMetallic)
		{
			UMaterialExpression* Expr = UMaterialEditingLibrary::CreateMaterialExpression(
				NewMaterial, UMaterialExpressionConstant::StaticClass(), -400, -200);
			UMaterialExpressionConstant* MetallicExpr = Cast<UMaterialExpressionConstant>(Expr);
			if (MetallicExpr)
			{
				MetallicExpr->R = MetallicVal;
				UMaterialEditingLibrary::ConnectMaterialProperty(MetallicExpr, TEXT(""), MP_Metallic);
			}
		}
		if (bHasRoughness)
		{
			UMaterialExpression* Expr = UMaterialEditingLibrary::CreateMaterialExpression(
				NewMaterial, UMaterialExpressionConstant::StaticClass(), -400, -400);
			UMaterialExpressionConstant* RoughnessExpr = Cast<UMaterialExpressionConstant>(Expr);
			if (RoughnessExpr)
			{
				RoughnessExpr->R = RoughnessVal;
				UMaterialEditingLibrary::ConnectMaterialProperty(RoughnessExpr, TEXT(""), MP_Roughness);
			}
		}
		if (bHasSpecular)
		{
			UMaterialExpression* Expr = UMaterialEditingLibrary::CreateMaterialExpression(
				NewMaterial, UMaterialExpressionConstant::StaticClass(), -400, -600);
			UMaterialExpressionConstant* SpecularExpr = Cast<UMaterialExpressionConstant>(Expr);
			if (SpecularExpr)
			{
				SpecularExpr->R = SpecularVal;
				UMaterialEditingLibrary::ConnectMaterialProperty(SpecularExpr, TEXT(""), MP_Specular);
			}
		}

		// Fresnel edge glow: Fresnel -> Multiply(RimColor) -> Emissive
		if (bHasFresnel)
		{
			UMaterialExpression* FresnelExpr = UMaterialEditingLibrary::CreateMaterialExpression(
				NewMaterial, UMaterialExpressionFresnel::StaticClass(), 400, 200);
			UMaterialExpressionFresnel* Fresnel = Cast<UMaterialExpressionFresnel>(FresnelExpr);
			if (Fresnel)
			{
				// ExponentIn is FExpressionInput — must connect a Constant expression
				UMaterialExpression* ExpConstExpr = UMaterialEditingLibrary::CreateMaterialExpression(
					NewMaterial, UMaterialExpressionConstant::StaticClass(), 200, 300);
				UMaterialExpressionConstant* ExpConst = Cast<UMaterialExpressionConstant>(ExpConstExpr);
				if (ExpConst)
				{
					ExpConst->R = FresnelExponent;
					Fresnel->ExponentIn.Expression = ExpConst;
					Fresnel->ExponentIn.OutputIndex = 0;
				}
				// Rim color: light jade green glow
				UMaterialExpression* RimColorExpr = UMaterialEditingLibrary::CreateMaterialExpression(
					NewMaterial, UMaterialExpressionConstant3Vector::StaticClass(), 400, 0);
				UMaterialExpressionConstant3Vector* RimColor = Cast<UMaterialExpressionConstant3Vector>(RimColorExpr);
				if (RimColor)
				{
					RimColor->Constant = FLinearColor(0.15f, 0.55f, 0.30f, 1.0f);
					// Multiply: Fresnel(scalar) * RimColor(vector) = colored rim glow
					UMaterialExpression* MulExpr = UMaterialEditingLibrary::CreateMaterialExpression(
						NewMaterial, UMaterialExpressionMultiply::StaticClass(), 400, -100);
					UMaterialExpressionMultiply* Mul = Cast<UMaterialExpressionMultiply>(MulExpr);
					if (Mul)
					{
						Mul->A.Expression = Fresnel;
						Mul->A.OutputIndex = 0;
						Mul->B.Expression = RimColor;
						Mul->B.OutputIndex = 0;
						UMaterialEditingLibrary::ConnectMaterialProperty(Mul, TEXT(""), MP_EmissiveColor);
					}
				}
			}
		}

		// Procedural clouds: TexCoord -> Noise(Scale=3.0) blended into BaseColor
		if (bHasClouds)
		{
			UMaterialExpression* TCExpr = UMaterialEditingLibrary::CreateMaterialExpression(
				NewMaterial, UMaterialExpressionTextureCoordinate::StaticClass(), -600, 200);
			UMaterialExpressionTextureCoordinate* TexCoord = Cast<UMaterialExpressionTextureCoordinate>(TCExpr);
			if (TexCoord) TexCoord->CoordinateIndex = 0;
			UMaterialExpression* NoiseExpr = UMaterialEditingLibrary::CreateMaterialExpression(
				NewMaterial, UMaterialExpressionNoise::StaticClass(), -400, 200);
			UMaterialExpressionNoise* Noise = Cast<UMaterialExpressionNoise>(NoiseExpr);
			if (Noise)
			{
				Noise->Scale = 3.0f;
				Noise->Quality = 2;
				if (TexCoord)
				{
					Noise->Position.Expression = TexCoord;
					Noise->Position.OutputIndex = 0;
				}
			}
		}

		// Texture sample: load texture and tint with baseColor
		if (bHasTexture)
		{
			UTexture* Tex = LoadObject<UTexture>(nullptr, *TexturePath);
			if (Tex)
			{
				UMaterialExpression* TSParamExpr = UMaterialEditingLibrary::CreateMaterialExpression(
					NewMaterial, UMaterialExpressionTextureSampleParameter2D::StaticClass(), -600, -400);
				UMaterialExpressionTextureSampleParameter2D* TSParam = Cast<UMaterialExpressionTextureSampleParameter2D>(TSParamExpr);
				if (TSParam)
				{
					TSParam->Texture = Tex;
					TSParam->ParameterName = FName(TEXT("TexParam"));
					if (bHasBaseColor)
					{
						UMaterialExpression* MulExpr = UMaterialEditingLibrary::CreateMaterialExpression(
							NewMaterial, UMaterialExpressionMultiply::StaticClass(), -200, -300);
						UMaterialExpressionMultiply* Mul = Cast<UMaterialExpressionMultiply>(MulExpr);
						if (Mul)
						{
							Mul->A.Expression = TSParam;
							Mul->A.OutputIndex = 0;
							UMaterialEditingLibrary::ConnectMaterialProperty(Mul, TEXT(""), MP_BaseColor);
						}
					}
					else
					{
						UMaterialEditingLibrary::ConnectMaterialProperty(TSParam, TEXT(""), MP_BaseColor);
					}
				}
			}
		}

		// Lerp: blend two colors by alpha
		if (bHasLerpB && bHasBaseColor)
		{
			UMaterialExpression* CAExpr = UMaterialEditingLibrary::CreateMaterialExpression(
				NewMaterial, UMaterialExpressionConstant3Vector::StaticClass(), -400, -600);
			UMaterialExpressionConstant3Vector* ColorA = Cast<UMaterialExpressionConstant3Vector>(CAExpr);
			if (ColorA) ColorA->Constant = BaseColor;
			UMaterialExpression* CBExpr = UMaterialEditingLibrary::CreateMaterialExpression(
				NewMaterial, UMaterialExpressionConstant3Vector::StaticClass(), -400, -800);
			UMaterialExpressionConstant3Vector* ColorB = Cast<UMaterialExpressionConstant3Vector>(CBExpr);
			if (ColorB) ColorB->Constant = LerpColorB;
			UMaterialExpression* AlphaExpr = UMaterialEditingLibrary::CreateMaterialExpression(
				NewMaterial, UMaterialExpressionConstant::StaticClass(), -400, -1000);
			UMaterialExpressionConstant* AlphaNode = Cast<UMaterialExpressionConstant>(AlphaExpr);
			if (AlphaNode) AlphaNode->R = LerpAlpha;
			UMaterialExpression* LerpExpr = UMaterialEditingLibrary::CreateMaterialExpression(
				NewMaterial, UMaterialExpressionLinearInterpolate::StaticClass(), -200, -700);
			UMaterialExpressionLinearInterpolate* LerpNode = Cast<UMaterialExpressionLinearInterpolate>(LerpExpr);
			if (LerpNode && ColorA && ColorB && AlphaNode)
			{
				LerpNode->A.Expression = ColorA;
				LerpNode->A.OutputIndex = 0;
				LerpNode->B.Expression = ColorB;
				LerpNode->B.OutputIndex = 0;
				LerpNode->Alpha.Expression = AlphaNode;
				LerpNode->Alpha.OutputIndex = 0;
				UMaterialEditingLibrary::ConnectMaterialProperty(LerpNode, TEXT(""), MP_BaseColor);
			}
		}

		// OneMinus: 1-x, useful for roughness/smoothness conversion
		if (bHasOneMinus)
		{
			UMaterialExpression* OMInExpr = UMaterialEditingLibrary::CreateMaterialExpression(
				NewMaterial, UMaterialExpressionConstant::StaticClass(), -400, -800);
			UMaterialExpressionConstant* OMInConst = Cast<UMaterialExpressionConstant>(OMInExpr);
			if (OMInConst) OMInConst->R = OneMinusVal;
			UMaterialExpression* OMExpr = UMaterialEditingLibrary::CreateMaterialExpression(
				NewMaterial, UMaterialExpressionOneMinus::StaticClass(), -200, -800);
			UMaterialExpressionOneMinus* OMNode = Cast<UMaterialExpressionOneMinus>(OMExpr);
			if (OMNode && OMInConst)
			{
				OMNode->Input.Expression = OMInConst;
				OMNode->Input.OutputIndex = 0;
				UMaterialEditingLibrary::ConnectMaterialProperty(OMNode, TEXT(""), MP_Roughness);
			}
		}

		// Trigger shader compilation
		if (bHasProperties)
		{
			NewMaterial->PostEditChange();
		}

		FAssetRegistryModule::AssetCreated(NewMaterial);

		// Save package to disk so asset persists across editor restarts
		Package->MarkPackageDirty();
		FString PackageFile = FPackageName::LongPackageNameToFilename(FullName, FPackageName::GetAssetPackageExtension());
		FSavePackageArgs SaveArgs;
		SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
		UPackage::SavePackage(Package, NewMaterial, *PackageFile, SaveArgs);

		TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
		Result->SetStringField(TEXT("path"), Path);
		Result->SetStringField(TEXT("assetName"), AssetName);
		if (!ShadingModelStr.IsEmpty())
			Result->SetStringField(TEXT("shadingModel"), ShadingModelStr);

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

		FScopedSlowTask SlowTask(0, FText(), false);

		FString AssetName = FPaths::GetBaseFilename(Path);
		FString PackagePath = FPaths::GetPath(Path);
		FString FullName = PackagePath / AssetName;

		// Reuse existing MI, but reparent if parent changed
		if (UMaterialInstance* Existing = LoadObject<UMaterialInstance>(nullptr, *(FullName + TEXT(".") + AssetName)))
		{
			UMaterialInstanceConstant* ExistingMIC = Cast<UMaterialInstanceConstant>(Existing);
			if (ExistingMIC && ExistingMIC->Parent != ParentMat)
			{
				ExistingMIC->SetParentEditorOnly(ParentMat);
				ExistingMIC->PostEditChange();
			}
			TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
			Result->SetStringField(TEXT("path"), Path);
			Result->SetStringField(TEXT("type"), InstanceType.ToLower());
			Result->SetStringField(TEXT("parent"), ParentPath);
			Result->SetBoolField(TEXT("reused"), true);
			ResponseJson = MakeShareable(new FJsonObject);
			ResponseJson->SetBoolField(TEXT("success"), true);
			ResponseJson->SetObjectField(TEXT("result"), Result);
			DoneEvent->Trigger();
			return;
		}

		UPackage* Package = CreatePackage(*FullName);
		if (!Package) { ErrorMsg = TEXT("Failed to create package"); DoneEvent->Trigger(); return; }

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

		if (!NewInstance) { ErrorMsg = TEXT("Failed to create material instance"); DoneEvent->Trigger(); return; }

		FAssetRegistryModule::AssetCreated(NewInstance);

		// Save package to disk
		Package->MarkPackageDirty();
		FString PackageFile = FPackageName::LongPackageNameToFilename(FullName, FPackageName::GetAssetPackageExtension());
		FSavePackageArgs SaveArgs;
		SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
		UPackage::SavePackage(Package, NewInstance, *PackageFile, SaveArgs);

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
