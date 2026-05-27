#include "CoreMinimal.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Engine/Blueprint.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "Particles/ParticleSystemComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Editor.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

static TMap<FString, AActor*> SpawnedActors;

static FString ActorToJson(AActor* Actor)
{
    if (!Actor)
    {
        return TEXT("null");
    }

    TSharedPtr<FJsonObject> Obj = MakeShareable(new FJsonObject);
    Obj->SetStringField(TEXT("name"), Actor->GetName());
    Obj->SetStringField(TEXT("class"), Actor->GetClass()->GetName());

    FVector Loc = Actor->GetActorLocation();
    TArray<TSharedPtr<FJsonValue>> LocArr;
    LocArr.Add(MakeShareable(new FJsonValueNumber(Loc.X)));
    LocArr.Add(MakeShareable(new FJsonValueNumber(Loc.Y)));
    LocArr.Add(MakeShareable(new FJsonValueNumber(Loc.Z)));
    Obj->SetArrayField(TEXT("location"), LocArr);

    FString Out;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Out);
    FJsonSerializer::Serialize(Obj.ToSharedRef(), Writer);
    return Out;
}

FString HandleSpawnActor(const TSharedPtr<FJsonObject>& Params)
{
    FString ClassName = Params->GetStringField(TEXT("className"));
    FString ActorName = Params->GetStringField(TEXT("name"));

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!World)
    {
        return TEXT("{\"success\":false,\"error\":\"No world available\"}");
    }

    UClass* ActorClass = nullptr;
    if (ClassName == TEXT("Actor"))
    {
        ActorClass = AActor::StaticClass();
    }
    else
    {
        ActorClass = FindFirstObject<UClass>(*ClassName);
    }

    if (!ActorClass)
    {
        return FString::Printf(TEXT("{\"success\":false,\"error\":\"Class not found: %s\"}"), *ClassName);
    }

    FVector Location(0, 0, 0);
    if (Params->HasField(TEXT("location")))
    {
        const TArray<TSharedPtr<FJsonValue>>& Arr = Params->GetArrayField(TEXT("location"));
        if (Arr.Num() >= 3)
        {
            Location.X = Arr[0]->AsNumber();
            Location.Y = Arr[1]->AsNumber();
            Location.Z = Arr[2]->AsNumber();
        }
    }

    FActorSpawnParameters SpawnParams;
    if (!ActorName.IsEmpty())
    {
        SpawnParams.Name = FName(*ActorName);
    }

    AActor* SpawnedActor = World->SpawnActor<AActor>(ActorClass, Location, FRotator::ZeroRotator, SpawnParams);
    if (!SpawnedActor)
    {
        return TEXT("{\"success\":false,\"error\":\"Failed to spawn actor\"}");
    }

    FString Key = SpawnedActor->GetName();
    SpawnedActors.Add(Key, SpawnedActor);

    TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
    Result->SetStringField(TEXT("actor_name"), Key);
    Result->SetStringField(TEXT("class"), ActorClass->GetName());

    TSharedPtr<FJsonObject> Response = MakeShareable(new FJsonObject);
    Response->SetBoolField(TEXT("success"), true);
    Response->SetObjectField(TEXT("result"), Result);

    FString Out;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Out);
    FJsonSerializer::Serialize(Response.ToSharedRef(), Writer);
    return Out;
}

FString HandleSetActorProperty(const TSharedPtr<FJsonObject>& Params)
{
    FString ActorName = Params->GetStringField(TEXT("actorName"));
    FString PropertyName = Params->GetStringField(TEXT("propertyName"));

    AActor* Actor = nullptr;
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (World)
    {
        for (TActorIterator<AActor> It(World); It; ++It)
        {
            if (It->GetName() == ActorName)
            {
                Actor = *It;
                break;
            }
        }
    }

    if (!Actor)
    {
        return FString::Printf(TEXT("{\"success\":false,\"error\":\"Actor not found: %s\"}"), *ActorName);
    }

    FProperty* Property = Actor->GetClass()->FindPropertyByName(FName(*PropertyName));
    if (!Property)
    {
        return FString::Printf(TEXT("{\"success\":false,\"error\":\"Property not found: %s\"}"), *PropertyName);
    }

    TSharedPtr<FJsonObject> ValueObj = Params->GetObjectField(TEXT("value"));
    if (ValueObj->HasField(TEXT("FloatValue")))
    {
        float FloatValue = ValueObj->GetNumberField(TEXT("FloatValue"));
        if (FFloatProperty* FloatProp = CastField<FFloatProperty>(Property))
        {
            FloatProp->SetPropertyValue_InContainer(Actor, FloatValue);
        }
    }
    else if (ValueObj->HasField(TEXT("IntValue")))
    {
        int32 IntValue = ValueObj->GetIntegerField(TEXT("IntValue"));
        if (FIntProperty* IntProp = CastField<FIntProperty>(Property))
        {
            IntProp->SetPropertyValue_InContainer(Actor, IntValue);
        }
    }
    else if (ValueObj->HasField(TEXT("StringValue")))
    {
        FString StringValue = ValueObj->GetStringField(TEXT("StringValue"));
        if (FStrProperty* StrProp = CastField<FStrProperty>(Property))
        {
            StrProp->SetPropertyValue_InContainer(Actor, StringValue);
        }
    }
    else if (ValueObj->HasField(TEXT("BoolValue")))
    {
        bool BoolValue = ValueObj->GetBoolField(TEXT("BoolValue"));
        if (FBoolProperty* BoolProp = CastField<FBoolProperty>(Property))
        {
            BoolProp->SetPropertyValue_InContainer(Actor, BoolValue);
        }
    }

    return TEXT("{\"success\":true,\"result\":{\"set\":true}}");
}

FString HandleGetActorProperty(const TSharedPtr<FJsonObject>& Params)
{
    FString ActorName = Params->GetStringField(TEXT("actorName"));
    FString PropertyName = Params->GetStringField(TEXT("propertyName"));

    AActor* Actor = nullptr;
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (World)
    {
        for (TActorIterator<AActor> It(World); It; ++It)
        {
            if (It->GetName() == ActorName)
            {
                Actor = *It;
                break;
            }
        }
    }

    if (!Actor)
    {
        return FString::Printf(TEXT("{\"success\":false,\"error\":\"Actor not found: %s\"}"), *ActorName);
    }

    FProperty* Property = Actor->GetClass()->FindPropertyByName(FName(*PropertyName));
    if (!Property)
    {
        return FString::Printf(TEXT("{\"success\":false,\"error\":\"Property not found: %s\"}"), *PropertyName);
    }

    TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);

    if (FFloatProperty* FloatProp = CastField<FFloatProperty>(Property))
    {
        float Value = FloatProp->GetPropertyValue_InContainer(Actor);
        Result->SetNumberField(TEXT("value"), Value);
    }
    else if (FIntProperty* IntProp = CastField<FIntProperty>(Property))
    {
        int32 Value = IntProp->GetPropertyValue_InContainer(Actor);
        Result->SetNumberField(TEXT("value"), Value);
    }
    else if (FStrProperty* StrProp = CastField<FStrProperty>(Property))
    {
        FString Value = StrProp->GetPropertyValue_InContainer(Actor);
        Result->SetStringField(TEXT("value"), Value);
    }
    else if (FBoolProperty* BoolProp = CastField<FBoolProperty>(Property))
    {
        bool Value = BoolProp->GetPropertyValue_InContainer(Actor);
        Result->SetBoolField(TEXT("value"), Value);
    }
    else
    {
        Result->SetStringField(TEXT("value"), TEXT("Unsupported property type"));
    }

    TSharedPtr<FJsonObject> Response = MakeShareable(new FJsonObject);
    Response->SetBoolField(TEXT("success"), true);
    Response->SetObjectField(TEXT("result"), Result);

    FString Out;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Out);
    FJsonSerializer::Serialize(Response.ToSharedRef(), Writer);
    return Out;
}

FString HandleDuplicateActor(const TSharedPtr<FJsonObject>& Params)
{
    FString Name = Params->GetStringField(TEXT("name"));
    FString NewName = Params->GetStringField(TEXT("newName"));

    AActor* SourceActor = nullptr;
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (World)
    {
        for (TActorIterator<AActor> It(World); It; ++It)
        {
            if (It->GetName() == Name)
            {
                SourceActor = *It;
                break;
            }
        }
    }

    if (!SourceActor)
    {
        return FString::Printf(TEXT("{\"success\":false,\"error\":\"Actor not found: %s\"}"), *Name);
    }

    FVector Location = SourceActor->GetActorLocation() + FVector(100, 0, 0);
    FActorSpawnParameters SpawnParams;
    if (!NewName.IsEmpty())
    {
        SpawnParams.Name = FName(*NewName);
    }

    AActor* DuplicatedActor = World->SpawnActor<AActor>(SourceActor->GetClass(), Location, SourceActor->GetActorRotation(), SpawnParams);
    if (!DuplicatedActor)
    {
        return TEXT("{\"success\":false,\"error\":\"Failed to duplicate actor\"}");
    }

    TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
    Result->SetStringField(TEXT("actor_name"), DuplicatedActor->GetName());
    Result->SetStringField(TEXT("source"), Name);

    TSharedPtr<FJsonObject> Response = MakeShareable(new FJsonObject);
    Response->SetBoolField(TEXT("success"), true);
    Response->SetObjectField(TEXT("result"), Result);

    FString Out;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Out);
    FJsonSerializer::Serialize(Response.ToSharedRef(), Writer);
    return Out;
}

FString HandleOpenLevel(const TSharedPtr<FJsonObject>& Params)
{
    FString Path = Params->GetStringField(TEXT("path"));

    if (GEditor)
    {
        GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAsset(Path);
        return TEXT("{\"success\":true,\"result\":{\"opened\":true}}");
    }

    return TEXT("{\"success\":false,\"error\":\"Editor not available\"}");
}

FString HandleDestroyActor(const TSharedPtr<FJsonObject>& Params)
{
    FString Name = Params->GetStringField(TEXT("name"));

    AActor** Found = SpawnedActors.Find(Name);
    if (!Found)
    {
        UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
        if (World)
        {
            for (TActorIterator<AActor> It(World); It; ++It)
            {
                AActor* Actor = *It;
                if (Actor->GetName() == Name)
                {
                    SpawnedActors.Add(Name, Actor);
                    Found = SpawnedActors.Find(Name);
                    break;
                }
            }
        }
    }

    if (Found && *Found)
    {
        (*Found)->Destroy();
        SpawnedActors.Remove(Name);
        return TEXT("{\"success\":true,\"result\":{\"destroyed\":true}}");
    }

    return FString::Printf(TEXT("{\"success\":false,\"error\":\"Actor not found: %s\"}"), *Name);
}

FString HandleSetActorTransform(const TSharedPtr<FJsonObject>& Params)
{
    FString Name = Params->GetStringField(TEXT("name"));

    AActor* Actor = nullptr;
    AActor** Found = SpawnedActors.Find(Name);
    if (Found)
    {
        Actor = *Found;
    }
    else
    {
        UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
        if (World)
        {
            for (TActorIterator<AActor> It(World); It; ++It)
            {
                if (It->GetName() == Name)
                {
                    Actor = *It;
                    break;
                }
            }
        }
    }

    if (!Actor)
    {
        return FString::Printf(TEXT("{\"success\":false,\"error\":\"Actor not found: %s\"}"), *Name);
    }

    if (Params->HasField(TEXT("location")))
    {
        const TArray<TSharedPtr<FJsonValue>>& Arr = Params->GetArrayField(TEXT("location"));
        if (Arr.Num() >= 3)
        {
            FVector Loc(Arr[0]->AsNumber(), Arr[1]->AsNumber(), Arr[2]->AsNumber());
            Actor->SetActorLocation(Loc);
        }
    }

    if (Params->HasField(TEXT("rotation")))
    {
        const TArray<TSharedPtr<FJsonValue>>& Arr = Params->GetArrayField(TEXT("rotation"));
        if (Arr.Num() >= 3)
        {
            FRotator Rot(Arr[0]->AsNumber(), Arr[1]->AsNumber(), Arr[2]->AsNumber());
            Actor->SetActorRotation(Rot);
        }
    }

    if (Params->HasField(TEXT("scale")))
    {
        const TArray<TSharedPtr<FJsonValue>>& Arr = Params->GetArrayField(TEXT("scale"));
        if (Arr.Num() >= 3)
        {
            FVector Scale(Arr[0]->AsNumber(), Arr[1]->AsNumber(), Arr[2]->AsNumber());
            Actor->SetActorScale3D(Scale);
        }
    }

    return TEXT("{\"success\":true,\"result\":{\"updated\":true}}");
}

FString HandleGetActorList(const TSharedPtr<FJsonObject>& Params)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!World)
    {
        return TEXT("{\"success\":false,\"error\":\"No world available\"}");
    }

    TArray<TSharedPtr<FJsonValue>> Actors;
    for (TActorIterator<AActor> It(World); It; ++It)
    {
        TSharedPtr<FJsonObject> Obj = MakeShareable(new FJsonObject);
        Obj->SetStringField(TEXT("name"), It->GetName());
        Obj->SetStringField(TEXT("class"), It->GetClass()->GetName());
        Actors.Add(MakeShareable(new FJsonValueObject(Obj)));
    }

    TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
    Result->SetArrayField(TEXT("actors"), Actors);

    TSharedPtr<FJsonObject> Response = MakeShareable(new FJsonObject);
    Response->SetBoolField(TEXT("success"), true);
    Response->SetObjectField(TEXT("result"), Result);

    FString Out;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Out);
    FJsonSerializer::Serialize(Response.ToSharedRef(), Writer);
    return Out;
}

FString HandleSetStaticMesh(const TSharedPtr<FJsonObject>& Params)
{
    FString ActorName = Params->GetStringField(TEXT("actorName"));
    FString MeshPath = Params->GetStringField(TEXT("meshPath"));

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!World)
    {
        return TEXT("{\"success\":false,\"error\":\"No world available\"}");
    }

    AActor* TargetActor = nullptr;
    for (TActorIterator<AActor> It(World); It; ++It)
    {
        if (It->GetName() == ActorName)
        {
            TargetActor = *It;
            break;
        }
    }

    if (!TargetActor)
    {
        return FString::Printf(TEXT("{\"success\":false,\"error\":\"Actor not found: %s\"}"), *ActorName);
    }

    UStaticMesh* StaticMesh = LoadObject<UStaticMesh>(nullptr, *MeshPath);
    if (!StaticMesh)
    {
        return FString::Printf(TEXT("{\"success\":false,\"error\":\"Mesh not found: %s\"}"), *MeshPath);
    }

    UStaticMeshComponent* MeshComp = nullptr;
    if (Params->HasField(TEXT("componentName")))
    {
        FString CompName = Params->GetStringField(TEXT("componentName"));
        TSet<UActorComponent*> Components = TargetActor->GetComponents();
        for (UActorComponent* Comp : Components)
        {
            if (Comp->GetName() == CompName)
            {
                MeshComp = Cast<UStaticMeshComponent>(Comp);
                break;
            }
        }
    }
    else
    {
        MeshComp = TargetActor->FindComponentByClass<UStaticMeshComponent>();
    }

    if (!MeshComp)
    {
        return TEXT("{\"success\":false,\"error\":\"No StaticMeshComponent found on actor\"}");
    }

    MeshComp->SetStaticMesh(StaticMesh);
    TargetActor->MarkPackageDirty();

    return FString::Printf(TEXT("{\"success\":true,\"result\":{\"actor\":\"%s\",\"mesh\":\"%s\"}}"), *ActorName, *MeshPath);
}

FString HandleFindActorsByClass(const TSharedPtr<FJsonObject>& Params)
{
    FString ClassName = Params->GetStringField(TEXT("className"));
    bool bExactMatch = Params->HasField(TEXT("exactMatch"))
        ? Params->GetBoolField(TEXT("exactMatch"))
        : false;

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!World)
    {
        return TEXT("{\"success\":false,\"error\":\"No world available\"}");
    }

    TArray<TSharedPtr<FJsonValue>> Actors;
    for (TActorIterator<AActor> It(World); It; ++It)
    {
        FString ActorClassName = It->GetClass()->GetName();
        bool bMatches = bExactMatch
            ? ActorClassName.Equals(ClassName, ESearchCase::IgnoreCase)
            : ActorClassName.Contains(ClassName, ESearchCase::IgnoreCase);

        if (bMatches)
        {
            TSharedPtr<FJsonObject> Obj = MakeShareable(new FJsonObject);
            Obj->SetStringField(TEXT("name"), It->GetName());
            Obj->SetStringField(TEXT("class"), ActorClassName);
            FVector Loc = It->GetActorLocation();
            TArray<TSharedPtr<FJsonValue>> LocArr;
            LocArr.Add(MakeShareable(new FJsonValueNumber(Loc.X)));
            LocArr.Add(MakeShareable(new FJsonValueNumber(Loc.Y)));
            LocArr.Add(MakeShareable(new FJsonValueNumber(Loc.Z)));
            Obj->SetArrayField(TEXT("location"), LocArr);
            Actors.Add(MakeShareable(new FJsonValueObject(Obj)));
        }
    }

    TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
    Result->SetArrayField(TEXT("actors"), Actors);
    Result->SetNumberField(TEXT("count"), Actors.Num());

    FString ResultStr;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ResultStr);
    FJsonSerializer::Serialize(Result.ToSharedRef(), Writer);

    return FString::Printf(TEXT("{\"success\":true,\"result\":%s}"), *ResultStr);
}

FString HandleSpawnBlueprintActor(const TSharedPtr<FJsonObject>& Params)
{
    FString BlueprintPath = Params->GetStringField(TEXT("blueprintPath"));
    FString ActorName = Params->HasField(TEXT("name"))
        ? Params->GetStringField(TEXT("name"))
        : TEXT("");

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!World)
    {
        return TEXT("{\"success\":false,\"error\":\"No world available\"}");
    }

    UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *BlueprintPath);
    if (!Blueprint)
    {
        return FString::Printf(TEXT("{\"success\":false,\"error\":\"Blueprint not found: %s\"}"), *BlueprintPath);
    }

    UClass* GeneratedClass = Blueprint->GeneratedClass;
    if (!GeneratedClass)
    {
        return TEXT("{\"success\":false,\"error\":\"Blueprint has no generated class\"}");
    }

    FVector Location(0, 0, 0);
    if (Params->HasField(TEXT("location")))
    {
        const TArray<TSharedPtr<FJsonValue>>& Arr = Params->GetArrayField(TEXT("location"));
        if (Arr.Num() >= 3)
        {
            Location.X = Arr[0]->AsNumber();
            Location.Y = Arr[1]->AsNumber();
            Location.Z = Arr[2]->AsNumber();
        }
    }

    FRotator Rotation(0, 0, 0);
    if (Params->HasField(TEXT("rotation")))
    {
        const TArray<TSharedPtr<FJsonValue>>& Arr = Params->GetArrayField(TEXT("rotation"));
        if (Arr.Num() >= 3)
        {
            Rotation.Pitch = Arr[0]->AsNumber();
            Rotation.Yaw = Arr[1]->AsNumber();
            Rotation.Roll = Arr[2]->AsNumber();
        }
    }

    FActorSpawnParameters SpawnParams;
    if (!ActorName.IsEmpty())
    {
        SpawnParams.Name = FName(*ActorName);
    }

    AActor* SpawnedActor = World->SpawnActor<AActor>(GeneratedClass, Location, Rotation, SpawnParams);
    if (!SpawnedActor)
    {
        return TEXT("{\"success\":false,\"error\":\"Failed to spawn actor\"}");
    }

    TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
    Result->SetStringField(TEXT("name"), SpawnedActor->GetName());
    Result->SetStringField(TEXT("class"), SpawnedActor->GetClass()->GetName());
    Result->SetStringField(TEXT("blueprint"), BlueprintPath);

    FString ResultStr;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ResultStr);
    FJsonSerializer::Serialize(Result.ToSharedRef(), Writer);

    return FString::Printf(TEXT("{\"success\":true,\"result\":%s}"), *ResultStr);
}

FString HandleSpawnEffect(const TSharedPtr<FJsonObject>& Params)
{
    FString AssetPath = Params->GetStringField(TEXT("assetPath"));

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!World) return TEXT("{\"success\":false,\"error\":\"No world available\"}");

    UObject* EffectAsset = LoadObject<UObject>(nullptr, *AssetPath);
    if (!EffectAsset)
    {
        return FString::Printf(TEXT("{\"success\":false,\"error\":\"Effect asset not found: %s\"}"), *AssetPath);
    }

    FVector Location(0, 0, 0);
    if (Params->HasField(TEXT("location")))
    {
        const TArray<TSharedPtr<FJsonValue>>& Arr = Params->GetArrayField(TEXT("location"));
        if (Arr.Num() >= 3)
        {
            Location.X = Arr[0]->AsNumber();
            Location.Y = Arr[1]->AsNumber();
            Location.Z = Arr[2]->AsNumber();
        }
    }

    FRotator Rotation(0, 0, 0);
    if (Params->HasField(TEXT("rotation")))
    {
        const TArray<TSharedPtr<FJsonValue>>& Arr = Params->GetArrayField(TEXT("rotation"));
        if (Arr.Num() >= 3)
        {
            Rotation.Pitch = Arr[0]->AsNumber();
            Rotation.Yaw = Arr[1]->AsNumber();
            Rotation.Roll = Arr[2]->AsNumber();
        }
    }

    bool bAutoDestroy = !Params->HasField(TEXT("autoDestroy")) || Params->GetBoolField(TEXT("autoDestroy"));

    FActorSpawnParameters SpawnParams;
    SpawnParams.bNoFail = true;

    AActor* EffectActor = World->SpawnActor<AActor>(AActor::StaticClass(), Location, Rotation, SpawnParams);
    if (!EffectActor)
    {
        return TEXT("{\"success\":false,\"error\":\"Failed to spawn effect container\"}");
    }

    UParticleSystemComponent* PSC = nullptr;
    if (UParticleSystem* PSTemplate = Cast<UParticleSystem>(EffectAsset))
    {
        PSC = UGameplayStatics::SpawnEmitterAtLocation(
            World, PSTemplate, Location, Rotation, FVector(1.0f), bAutoDestroy);
    }
    else
    {
        // Try Niagara
        PSC = NewObject<UParticleSystemComponent>(EffectActor);
        if (PSC)
        {
            PSC->RegisterComponent();
            PSC->SetWorldLocation(Location);
            PSC->SetWorldRotation(Rotation);
        }
    }

    if (!PSC)
    {
        EffectActor->Destroy();
        return TEXT("{\"success\":false,\"error\":\"Could not create particle system\"}");
    }

    PSC->AttachToComponent(EffectActor->GetRootComponent(),
        FAttachmentTransformRules::KeepWorldTransform);

    // Ensure the container doesn't interfere
    EffectActor->SetActorHiddenInGame(true);

    TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
    Result->SetStringField(TEXT("name"), PSC->GetName());
    Result->SetStringField(TEXT("asset"), AssetPath);

    FString ResultStr;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ResultStr);
    FJsonSerializer::Serialize(Result.ToSharedRef(), Writer);

    return FString::Printf(TEXT("{\"success\":true,\"result\":%s}"), *ResultStr);
}

FString HandleAddActorTag(const TSharedPtr<FJsonObject>& Params)
{
    FString ActorName = Params->GetStringField(TEXT("actorName"));
    FString Tag = Params->GetStringField(TEXT("tag"));

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!World) return TEXT("{\"success\":false,\"error\":\"No world available\"}");

    AActor* Actor = nullptr;
    for (TActorIterator<AActor> It(World); It; ++It)
    {
        if (It->GetName() == ActorName)
        {
            Actor = *It;
            break;
        }
    }

    if (!Actor) return FString::Printf(TEXT("{\"success\":false,\"error\":\"Actor not found: %s\"}"), *ActorName);

    Actor->Tags.AddUnique(FName(*Tag));

    return FString::Printf(TEXT("{\"success\":true,\"result\":{\"actor\":\"%s\",\"tag\":\"%s\"}}"), *ActorName, *Tag);
}
