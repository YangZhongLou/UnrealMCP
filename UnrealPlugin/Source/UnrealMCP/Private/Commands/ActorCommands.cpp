#include "CoreMinimal.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "GameFramework/Actor.h"
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
        ActorClass = FindObject<UClass>(ANY_PACKAGE, *ClassName);
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
                if (It->GetName() == Name)
                {
                    Found = &(*It);
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
