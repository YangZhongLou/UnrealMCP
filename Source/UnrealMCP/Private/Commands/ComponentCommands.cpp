#if WITH_EDITOR
#include "CoreMinimal.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "Components/ActorComponent.h"
#include "Editor.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Async/Async.h"

FString HandleGetActorComponents(const TSharedPtr<FJsonObject>& Params)
{
    FString ActorName = Params->GetStringField(TEXT("actorName"));

    TArray<TSharedPtr<FJsonValue>> Components;
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

        TSet<UActorComponent*> ComponentSet = TargetActor->GetComponents();
        for (UActorComponent* Comp : ComponentSet)
        {
            TSharedPtr<FJsonObject> Obj = MakeShareable(new FJsonObject);
            Obj->SetStringField(TEXT("name"), Comp->GetName());
            Obj->SetStringField(TEXT("class"), Comp->GetClass()->GetName());
            Components.Add(MakeShareable(new FJsonValueObject(Obj)));
        }
        DoneEvent->Trigger();
    });

    DoneEvent->Wait();
    FPlatformProcess::ReturnSynchEventToPool(DoneEvent);

    if (!ErrorMsg.IsEmpty())
        return FString::Printf(TEXT("{\"success\":false,\"error\":\"%s\"}"), *ErrorMsg);

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

    FString ResultStr;
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

        UClass* CompClass = FindFirstObject<UClass>(*ComponentClass);
        if (!CompClass)
        {
            FString FullName = FString::Printf(TEXT("U%s"), *ComponentClass);
            CompClass = FindFirstObject<UClass>(*FullName);
        }
        if (!CompClass || !CompClass->IsChildOf(UActorComponent::StaticClass()))
        { ErrorMsg = FString::Printf(TEXT("Component class not found: %s"), *ComponentClass); DoneEvent->Trigger(); return; }

        FName NewName = ComponentName.IsEmpty() ? NAME_None : FName(*ComponentName);
        UActorComponent* NewComp = NewObject<UActorComponent>(TargetActor, CompClass, NewName, RF_Transactional);
        if (!NewComp) { ErrorMsg = TEXT("Failed to create component"); DoneEvent->Trigger(); return; }

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

        TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ResultStr);
        FJsonSerializer::Serialize(Response.ToSharedRef(), Writer);
        DoneEvent->Trigger();
    });

    DoneEvent->Wait();
    FPlatformProcess::ReturnSynchEventToPool(DoneEvent);

    if (!ErrorMsg.IsEmpty())
        return FString::Printf(TEXT("{\"success\":false,\"error\":\"%s\"}"), *ErrorMsg);
    return ResultStr;
}

FString HandleRemoveComponent(const TSharedPtr<FJsonObject>& Params)
{
    FString ActorName = Params->GetStringField(TEXT("actorName"));
    FString ComponentName = Params->GetStringField(TEXT("componentName"));

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

        TSet<UActorComponent*> ComponentSet = TargetActor->GetComponents();
        UActorComponent* TargetComp = nullptr;
        for (UActorComponent* Comp : ComponentSet)
        {
            if (Comp->GetName() == ComponentName) { TargetComp = Comp; break; }
        }
        if (!TargetComp) { ErrorMsg = FString::Printf(TEXT("Component not found: %s"), *ComponentName); DoneEvent->Trigger(); return; }

        TargetComp->DestroyComponent();
        TargetActor->RerunConstructionScripts();
        DoneEvent->Trigger();
    });

    DoneEvent->Wait();
    FPlatformProcess::ReturnSynchEventToPool(DoneEvent);

    if (!ErrorMsg.IsEmpty())
        return FString::Printf(TEXT("{\"success\":false,\"error\":\"%s\"}"), *ErrorMsg);

    return FString::Printf(TEXT("{\"success\":true,\"result\":{\"removed\":true,\"component\":\"%s\"}}"), *ComponentName);
}

#endif
