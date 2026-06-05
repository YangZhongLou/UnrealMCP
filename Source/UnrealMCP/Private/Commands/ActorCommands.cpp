#if WITH_EDITOR
#include "CoreMinimal.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Engine/Blueprint.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "Particles/ParticleSystemComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Editor.h"
#include "FileHelpers.h"
#include "LevelEditorSubsystem.h"
#include "Dom/JsonObject.h"
#include "Async/Async.h"
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
        SpawnParams.NameMode = FActorSpawnParameters::ESpawnActorNameMode::Requested;
    }
    FString MobilityStr = Params->HasField(TEXT("mobility"))
        ? Params->GetStringField(TEXT("mobility")) : TEXT("");

    AActor* SpawnedActor = nullptr;
    FEvent* DoneEvent = FPlatformProcess::GetSynchEventFromPool();

    AsyncTask(ENamedThreads::GameThread, [&]()
    {
        // Destroy existing actor with the same name to avoid spawn conflict / crash
        if (!ActorName.IsEmpty())
        {
            AActor** Existing = SpawnedActors.Find(ActorName);
            if (!Existing)
            {
                for (TActorIterator<AActor> It(World); It; ++It)
                {
                    if (It->GetName() == ActorName)
                    {
                        SpawnedActors.Add(ActorName, *It);
                        Existing = SpawnedActors.Find(ActorName);
                        break;
                    }
                }
            }
            if (Existing && *Existing)
            {
                (*Existing)->Destroy();
                SpawnedActors.Remove(ActorName);
            }
        }

        SpawnedActor = World->SpawnActor<AActor>(ActorClass, Location, FRotator::ZeroRotator, SpawnParams);
        if (SpawnedActor && !MobilityStr.IsEmpty())
        {
            EComponentMobility::Type Mobility = EComponentMobility::Movable;
            if (MobilityStr.Equals(TEXT("static"), ESearchCase::IgnoreCase))
                Mobility = EComponentMobility::Static;
            else if (MobilityStr.Equals(TEXT("stationary"), ESearchCase::IgnoreCase))
                Mobility = EComponentMobility::Stationary;
            USceneComponent* Root = SpawnedActor->GetRootComponent();
            if (Root) Root->SetMobility(Mobility);
        }
        DoneEvent->Trigger();
    });

    DoneEvent->Wait();
    FPlatformProcess::ReturnSynchEventToPool(DoneEvent);

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
    TSharedPtr<FJsonObject> ValueObj = Params->GetObjectField(TEXT("value"));

    FString ErrorMsg;
    FEvent* DoneEvent = FPlatformProcess::GetSynchEventFromPool();

    AsyncTask(ENamedThreads::GameThread, [&]()
    {
        AActor* Actor = nullptr;
        UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
        if (World)
        {
            for (TActorIterator<AActor> It(World); It; ++It)
            {
                if (It->GetName() == ActorName) { Actor = *It; break; }
            }
        }

        if (!Actor) { ErrorMsg = FString::Printf(TEXT("Actor not found: %s"), *ActorName); DoneEvent->Trigger(); return; }

        FProperty* Property = Actor->GetClass()->FindPropertyByName(FName(*PropertyName));
        if (!Property) { ErrorMsg = FString::Printf(TEXT("Property not found: %s"), *PropertyName); DoneEvent->Trigger(); return; }

        if (ValueObj->HasField(TEXT("FloatValue")))
        {
            if (FFloatProperty* FloatProp = CastField<FFloatProperty>(Property))
                FloatProp->SetPropertyValue_InContainer(Actor, ValueObj->GetNumberField(TEXT("FloatValue")));
        }
        else if (ValueObj->HasField(TEXT("IntValue")))
        {
            if (FIntProperty* IntProp = CastField<FIntProperty>(Property))
                IntProp->SetPropertyValue_InContainer(Actor, ValueObj->GetIntegerField(TEXT("IntValue")));
        }
        else if (ValueObj->HasField(TEXT("StringValue")))
        {
            if (FStrProperty* StrProp = CastField<FStrProperty>(Property))
                StrProp->SetPropertyValue_InContainer(Actor, ValueObj->GetStringField(TEXT("StringValue")));
        }
        else if (ValueObj->HasField(TEXT("BoolValue")))
        {
            if (FBoolProperty* BoolProp = CastField<FBoolProperty>(Property))
                BoolProp->SetPropertyValue_InContainer(Actor, ValueObj->GetBoolField(TEXT("BoolValue")));
        }

        DoneEvent->Trigger();
    });

    DoneEvent->Wait();
    FPlatformProcess::ReturnSynchEventToPool(DoneEvent);

    if (!ErrorMsg.IsEmpty())
        return FString::Printf(TEXT("{\"success\":false,\"error\":\"%s\"}"), *ErrorMsg);

    return TEXT("{\"success\":true,\"result\":{\"set\":true}}");
}

FString HandleGetActorProperty(const TSharedPtr<FJsonObject>& Params)
{
    FString ActorName = Params->GetStringField(TEXT("actorName"));
    FString PropertyName = Params->GetStringField(TEXT("propertyName"));

    FString ErrorMsg;
    TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
    FEvent* DoneEvent = FPlatformProcess::GetSynchEventFromPool();

    AsyncTask(ENamedThreads::GameThread, [&]()
    {
        AActor* Actor = nullptr;
        UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
        if (World)
        {
            for (TActorIterator<AActor> It(World); It; ++It)
            {
                if (It->GetName() == ActorName) { Actor = *It; break; }
            }
        }

        if (!Actor) { ErrorMsg = FString::Printf(TEXT("Actor not found: %s"), *ActorName); DoneEvent->Trigger(); return; }

        FProperty* Property = Actor->GetClass()->FindPropertyByName(FName(*PropertyName));
        if (!Property) { ErrorMsg = FString::Printf(TEXT("Property not found: %s"), *PropertyName); DoneEvent->Trigger(); return; }

        if (FFloatProperty* FloatProp = CastField<FFloatProperty>(Property))
        {
            Result->SetNumberField(TEXT("value"), FloatProp->GetPropertyValue_InContainer(Actor));
        }
        else if (FIntProperty* IntProp = CastField<FIntProperty>(Property))
        {
            Result->SetNumberField(TEXT("value"), (double)IntProp->GetPropertyValue_InContainer(Actor));
        }
        else if (FStrProperty* StrProp = CastField<FStrProperty>(Property))
        {
            Result->SetStringField(TEXT("value"), StrProp->GetPropertyValue_InContainer(Actor));
        }
        else if (FBoolProperty* BoolProp = CastField<FBoolProperty>(Property))
        {
            Result->SetBoolField(TEXT("value"), BoolProp->GetPropertyValue_InContainer(Actor));
        }
        else
        {
            Result->SetStringField(TEXT("value"), TEXT("Unsupported property type"));
        }

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

FString HandleDuplicateActor(const TSharedPtr<FJsonObject>& Params)
{
    FString Name = Params->GetStringField(TEXT("name"));
    FString NewName = Params->GetStringField(TEXT("newName"));

    TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
    FString ErrorMsg;
    FEvent* DoneEvent = FPlatformProcess::GetSynchEventFromPool();

    AsyncTask(ENamedThreads::GameThread, [&]()
    {
        UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
        if (!World) { ErrorMsg = TEXT("No world available"); DoneEvent->Trigger(); return; }

        AActor* SourceActor = nullptr;
        for (TActorIterator<AActor> It(World); It; ++It)
        {
            if (It->GetName() == Name) { SourceActor = *It; break; }
        }
        if (!SourceActor) { ErrorMsg = FString::Printf(TEXT("Actor not found: %s"), *Name); DoneEvent->Trigger(); return; }

        FVector Location = SourceActor->GetActorLocation() + FVector(100, 0, 0);
        FActorSpawnParameters SpawnParams;
        if (!NewName.IsEmpty()) SpawnParams.Name = FName(*NewName);

        AActor* DuplicatedActor = World->SpawnActor<AActor>(SourceActor->GetClass(), Location, SourceActor->GetActorRotation(), SpawnParams);
        if (!DuplicatedActor) { ErrorMsg = TEXT("Failed to duplicate actor"); DoneEvent->Trigger(); return; }

        Result->SetStringField(TEXT("actor_name"), DuplicatedActor->GetName());
        Result->SetStringField(TEXT("source"), Name);
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

FString HandleOpenLevel(const TSharedPtr<FJsonObject>& Params)
{
    // Support both "path" and "name" parameters for compatibility
    FString Path;
    if (Params->HasField(TEXT("path")))
    {
        Path = Params->GetStringField(TEXT("path"));
    }
    else if (Params->HasField(TEXT("name")))
    {
        Path = Params->GetStringField(TEXT("name"));
    }

    if (Path.IsEmpty())
    {
        return TEXT("{\"success\":false,\"error\":\"Missing 'path' or 'name' parameter\"}");
    }

    bool bSuccess = false;
    FEvent* DoneEvent = FPlatformProcess::GetSynchEventFromPool();

    AsyncTask(ENamedThreads::GameThread, [&]()
    {
        if (GEditor)
        {
            // Suppress UI dialogs during level load
            bool bPrevUnattended = GIsRunningUnattendedScript;
            GIsRunningUnattendedScript = true;

            ULevelEditorSubsystem* LevelEditor = GEditor->GetEditorSubsystem<ULevelEditorSubsystem>();
            if (LevelEditor)
            {
                bSuccess = LevelEditor->LoadLevel(Path);
            }

            GIsRunningUnattendedScript = bPrevUnattended;
        }
        DoneEvent->Trigger();
    });

    DoneEvent->Wait();
    FPlatformProcess::ReturnSynchEventToPool(DoneEvent);

    if (bSuccess)
    {
        TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
        Result->SetStringField(TEXT("path"), Path);
        Result->SetBoolField(TEXT("opened"), true);

        FString ResultStr;
        TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ResultStr);
        FJsonSerializer::Serialize(Result.ToSharedRef(), Writer);
        Writer->Close();

        return FString::Printf(TEXT("{\"success\":true,\"result\":%s}"), *ResultStr);
    }
    return TEXT("{\"success\":false,\"error\":\"Failed to open level\"}");
}

FString HandleDestroyActor(const TSharedPtr<FJsonObject>& Params)
{
    FString Name = Params->GetStringField(TEXT("name"));

    bool bDestroyed = false;
    FEvent* DoneEvent = FPlatformProcess::GetSynchEventFromPool();

    AsyncTask(ENamedThreads::GameThread, [&]()
    {
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
            bDestroyed = true;
        }

        DoneEvent->Trigger();
    });

    DoneEvent->Wait();
    FPlatformProcess::ReturnSynchEventToPool(DoneEvent);

    if (bDestroyed)
    {
        SpawnedActors.Remove(Name);
        return TEXT("{\"success\":true,\"result\":{\"destroyed\":true}}");
    }

    return FString::Printf(TEXT("{\"success\":false,\"error\":\"Actor not found: %s\"}"), *Name);
}

FString HandleSetActorTransform(const TSharedPtr<FJsonObject>& Params)
{
    FString Name = Params->GetStringField(TEXT("name"));

    // Copy out params before dispatching to GameThread
    FVector Loc(0, 0, 0);
    FRotator Rot(0, 0, 0);
    FVector Scale(1, 1, 1);
    bool bHasLoc = false, bHasRot = false, bHasScale = false;

    if (Params->HasField(TEXT("location")))
    {
        const TArray<TSharedPtr<FJsonValue>>& Arr = Params->GetArrayField(TEXT("location"));
        if (Arr.Num() >= 3) { Loc = FVector(Arr[0]->AsNumber(), Arr[1]->AsNumber(), Arr[2]->AsNumber()); bHasLoc = true; }
    }
    if (Params->HasField(TEXT("rotation")))
    {
        const TArray<TSharedPtr<FJsonValue>>& Arr = Params->GetArrayField(TEXT("rotation"));
        if (Arr.Num() >= 3) { Rot = FRotator(Arr[0]->AsNumber(), Arr[1]->AsNumber(), Arr[2]->AsNumber()); bHasRot = true; }
    }
    if (Params->HasField(TEXT("scale")))
    {
        const TArray<TSharedPtr<FJsonValue>>& Arr = Params->GetArrayField(TEXT("scale"));
        if (Arr.Num() >= 3) { Scale = FVector(Arr[0]->AsNumber(), Arr[1]->AsNumber(), Arr[2]->AsNumber()); bHasScale = true; }
    }

    bool bFound = false;
    FEvent* DoneEvent = FPlatformProcess::GetSynchEventFromPool();

    AsyncTask(ENamedThreads::GameThread, [&]()
    {
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
                    if (It->GetName() == Name) { Actor = *It; break; }
                }
            }
        }

        if (Actor)
        {
            if (bHasLoc) { Actor->SetActorLocation(Loc); }
            if (bHasRot) { Actor->SetActorRotation(Rot); }
            if (bHasScale) { Actor->SetActorScale3D(Scale); }
            bFound = true;
        }

        DoneEvent->Trigger();
    });

    DoneEvent->Wait();
    FPlatformProcess::ReturnSynchEventToPool(DoneEvent);

    if (!bFound)
    {
        return FString::Printf(TEXT("{\"success\":false,\"error\":\"Actor not found: %s\"}"), *Name);
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
    FEvent* DoneEvent = FPlatformProcess::GetSynchEventFromPool();

    AsyncTask(ENamedThreads::GameThread, [&]()
    {
        for (TActorIterator<AActor> It(World); It; ++It)
        {
            TSharedPtr<FJsonObject> Obj = MakeShareable(new FJsonObject);
            Obj->SetStringField(TEXT("name"), It->GetName());
            Obj->SetStringField(TEXT("class"), It->GetClass()->GetName());
            Actors.Add(MakeShareable(new FJsonValueObject(Obj)));
        }
        DoneEvent->Trigger();
    });

    DoneEvent->Wait();
    FPlatformProcess::ReturnSynchEventToPool(DoneEvent);

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

    UStaticMesh* StaticMesh = LoadObject<UStaticMesh>(nullptr, *MeshPath);
    if (!StaticMesh)
        return FString::Printf(TEXT("{\"success\":false,\"error\":\"Mesh not found: %s\"}"), *MeshPath);

    FString CompName = Params->HasField(TEXT("componentName")) ? Params->GetStringField(TEXT("componentName")) : TEXT("");
    FString ErrorMsg;
    FEvent* DoneEvent = FPlatformProcess::GetSynchEventFromPool();

    AsyncTask(ENamedThreads::GameThread, [&]()
    {
        UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
        if (!World) { ErrorMsg = TEXT("No world available"); DoneEvent->Trigger(); return; }

        AActor* TargetActor = nullptr;
        for (TActorIterator<AActor> It(World); It; ++It)
        {
            if (It->GetName() == ActorName) { TargetActor = *It; break; }
        }
        if (!TargetActor) { ErrorMsg = FString::Printf(TEXT("Actor not found: %s"), *ActorName); DoneEvent->Trigger(); return; }

        UStaticMeshComponent* MeshComp = nullptr;
        if (!CompName.IsEmpty())
        {
            TSet<UActorComponent*> Components = TargetActor->GetComponents();
            for (UActorComponent* Comp : Components)
            {
                if (Comp->GetName() == CompName) { MeshComp = Cast<UStaticMeshComponent>(Comp); break; }
            }
        }
        else
        {
            MeshComp = TargetActor->FindComponentByClass<UStaticMeshComponent>();
        }
        if (!MeshComp) { ErrorMsg = TEXT("No StaticMeshComponent found on actor"); DoneEvent->Trigger(); return; }

        MeshComp->SetStaticMesh(StaticMesh);
        TargetActor->MarkPackageDirty();
        DoneEvent->Trigger();
    });

    DoneEvent->Wait();
    FPlatformProcess::ReturnSynchEventToPool(DoneEvent);

    if (!ErrorMsg.IsEmpty())
        return FString::Printf(TEXT("{\"success\":false,\"error\":\"%s\"}"), *ErrorMsg);

    return FString::Printf(TEXT("{\"success\":true,\"result\":{\"actor\":\"%s\",\"mesh\":\"%s\"}}"), *ActorName, *MeshPath);
}

FString HandleFindActorsByClass(const TSharedPtr<FJsonObject>& Params)
{
    FString ClassName = Params->GetStringField(TEXT("className"));
    bool bExactMatch = Params->HasField(TEXT("exactMatch"))
        ? Params->GetBoolField(TEXT("exactMatch"))
        : false;

    TArray<TSharedPtr<FJsonValue>> Actors;
    FEvent* DoneEvent = FPlatformProcess::GetSynchEventFromPool();

    AsyncTask(ENamedThreads::GameThread, [&]()
    {
        UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
        if (!World) { DoneEvent->Trigger(); return; }

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
        DoneEvent->Trigger();
    });

    DoneEvent->Wait();
    FPlatformProcess::ReturnSynchEventToPool(DoneEvent);

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
        if (Arr.Num() >= 3) { Location = FVector(Arr[0]->AsNumber(), Arr[1]->AsNumber(), Arr[2]->AsNumber()); }
    }

    FRotator Rotation(0, 0, 0);
    if (Params->HasField(TEXT("rotation")))
    {
        const TArray<TSharedPtr<FJsonValue>>& Arr = Params->GetArrayField(TEXT("rotation"));
        if (Arr.Num() >= 3) { Rotation = FRotator(Arr[0]->AsNumber(), Arr[1]->AsNumber(), Arr[2]->AsNumber()); }
    }

    FActorSpawnParameters SpawnParams;
    if (!ActorName.IsEmpty()) { SpawnParams.Name = FName(*ActorName); }

    AActor* SpawnedActor = nullptr;
    FEvent* DoneEvent = FPlatformProcess::GetSynchEventFromPool();

    AsyncTask(ENamedThreads::GameThread, [&]()
    {
        UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
        if (World)
            SpawnedActor = World->SpawnActor<AActor>(GeneratedClass, Location, Rotation, SpawnParams);
        DoneEvent->Trigger();
    });

    DoneEvent->Wait();
    FPlatformProcess::ReturnSynchEventToPool(DoneEvent);

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

    AActor* EffectActor = nullptr;
    FString ResultStr;
    FString ErrorMsg;
    FEvent* DoneEvent = FPlatformProcess::GetSynchEventFromPool();

    AsyncTask(ENamedThreads::GameThread, [&]()
    {
        EffectActor = World->SpawnActor<AActor>(AActor::StaticClass(), Location, Rotation, SpawnParams);
        if (!EffectActor) { ErrorMsg = TEXT("Failed to spawn effect container"); DoneEvent->Trigger(); return; }

        UParticleSystemComponent* PSC = nullptr;
        if (UParticleSystem* PSTemplate = Cast<UParticleSystem>(EffectAsset))
        {
            PSC = UGameplayStatics::SpawnEmitterAtLocation(
                World, PSTemplate, Location, Rotation, FVector(1.0f), bAutoDestroy);
        }
        else
        {
            PSC = NewObject<UParticleSystemComponent>(EffectActor);
            if (PSC)
            {
                PSC->RegisterComponent();
                PSC->SetWorldLocation(Location);
                PSC->SetWorldRotation(Rotation);
            }
        }

        if (!PSC) { EffectActor->Destroy(); ErrorMsg = TEXT("Could not create particle system"); DoneEvent->Trigger(); return; }

        PSC->AttachToComponent(EffectActor->GetRootComponent(), FAttachmentTransformRules::KeepWorldTransform);
        EffectActor->SetActorHiddenInGame(true);

        TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
        Result->SetStringField(TEXT("name"), PSC->GetName());
        Result->SetStringField(TEXT("asset"), AssetPath);

        TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ResultStr);
        FJsonSerializer::Serialize(Result.ToSharedRef(), Writer);
        DoneEvent->Trigger();
    });

    DoneEvent->Wait();
    FPlatformProcess::ReturnSynchEventToPool(DoneEvent);

    if (!ErrorMsg.IsEmpty())
        return FString::Printf(TEXT("{\"success\":false,\"error\":\"%s\"}"), *ErrorMsg);

    return FString::Printf(TEXT("{\"success\":true,\"result\":%s}"), *ResultStr);
}

FString HandleAddActorTag(const TSharedPtr<FJsonObject>& Params)
{
    FString ActorName = Params->GetStringField(TEXT("actorName"));
    FString Tag = Params->GetStringField(TEXT("tag"));

    FString ErrorMsg;
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

        Actor->Tags.AddUnique(FName(*Tag));
        DoneEvent->Trigger();
    });

    DoneEvent->Wait();
    FPlatformProcess::ReturnSynchEventToPool(DoneEvent);

    if (!ErrorMsg.IsEmpty())
        return FString::Printf(TEXT("{\"success\":false,\"error\":\"%s\"}"), *ErrorMsg);

    return FString::Printf(TEXT("{\"success\":true,\"result\":{\"actor\":\"%s\",\"tag\":\"%s\"}}"), *ActorName, *Tag);
}

#endif
