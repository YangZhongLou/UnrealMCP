#include "CoreMinimal.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "Components/ActorComponent.h"
#include "Editor.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

FString HandleGetActorComponents(const TSharedPtr<FJsonObject>& Params)
{
    FString ActorName = Params->GetStringField(TEXT("actorName"));

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

    TArray<TSharedPtr<FJsonValue>> Components;
    TSet<UActorComponent*> ComponentSet = TargetActor->GetComponents();
    for (UActorComponent* Comp : ComponentSet)
    {
        TSharedPtr<FJsonObject> Obj = MakeShareable(new FJsonObject);
        Obj->SetStringField(TEXT("name"), Comp->GetName());
        Obj->SetStringField(TEXT("class"), Comp->GetClass()->GetName());
        Components.Add(MakeShareable(new FJsonValueObject(Obj)));
    }

    TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
    Result->SetArrayField(TEXT("components"), Components);
    Result->SetNumberField(TEXT("count"), Components.Num());

    TSharedPtr<FJsonObject> Response = MakeShareable(new FJsonObject);
    Response->SetBoolField(TEXT("success"), true);
    Response->SetObjectField(TEXT("result"), Result);

    FString Out;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Out);
    FJsonSerializer::Serialize(Response.ToSharedRef(), Writer);
    return Out;
}

FString HandleAddComponent(const TSharedPtr<FJsonObject>& Params)
{
    FString ActorName = Params->GetStringField(TEXT("actorName"));
    FString ComponentClass = Params->GetStringField(TEXT("componentClass"));
    FString ComponentName = Params->HasField(TEXT("componentName"))
        ? Params->GetStringField(TEXT("componentName"))
        : TEXT("");

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

    UClass* CompClass = FindObject<UClass>(nullptr, *ComponentClass);
    if (!CompClass)
    {
        // Try with UActorComponent prefix
        FString FullName = FString::Printf(TEXT("U%s"), *ComponentClass);
        CompClass = FindObject<UClass>(nullptr, *FullName);
    }
    if (!CompClass || !CompClass->IsChildOf(UActorComponent::StaticClass()))
    {
        return FString::Printf(TEXT("{\"success\":false,\"error\":\"Component class not found: %s\"}"), *ComponentClass);
    }

    FName NewName = ComponentName.IsEmpty() ? NAME_None : FName(*ComponentName);
    UActorComponent* NewComp = NewObject<UActorComponent>(TargetActor, CompClass, NewName, RF_Transactional);
    if (!NewComp)
    {
        return TEXT("{\"success\":false,\"error\":\"Failed to create component\"}");
    }

    TargetActor->AddInstanceComponent(NewComp);
    NewComp->RegisterComponent();
    TargetActor->RerunConstructionScripts();

    TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
    Result->SetStringField(TEXT("component_name"), NewComp->GetName());
    Result->SetStringField(TEXT("component_class"), NewComp->GetClass()->GetName());
    Result->SetStringField(TEXT("actor"), ActorName);

    TSharedPtr<FJsonObject> Response = MakeShareable(new FJsonObject);
    Response->SetBoolField(TEXT("success"), true);
    Response->SetObjectField(TEXT("result"), Result);

    FString Out;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Out);
    FJsonSerializer::Serialize(Response.ToSharedRef(), Writer);
    return Out;
}
