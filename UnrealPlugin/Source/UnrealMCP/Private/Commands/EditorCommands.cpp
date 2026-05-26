#include "CoreMinimal.h"
#include "Editor.h"
#include "Selection.h"
#include "EngineUtils.h"
#include "FileHelpers.h"
#include "LevelEditorViewport.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/PlatformFileManager.h"
#include "Engine/Engine.h"

FString HandleRunConsoleCommand(const TSharedPtr<FJsonObject>& Params)
{
    FString Command = Params->GetStringField(TEXT("command"));

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (World && World->GetGameInstance())
    {
        GEngine->Exec(World, *Command);
    }
    else
    {
        GEngine->Exec(GEditor->GetEditorWorldContext().World(), *Command);
    }

    return TEXT("{\"success\":true,\"result\":{\"executed\":true}}");
}

FString HandleSaveCurrentLevel(const TSharedPtr<FJsonObject>& Params)
{
    bool bSaved = false;
    if (GEditor)
    {
        UWorld* World = GEditor->GetEditorWorldContext().World();
        if (World)
        {
            FEditorFileUtils::SaveLevel(World->GetCurrentLevel());
            bSaved = true;
        }
    }

    if (bSaved)
    {
        return TEXT("{\"success\":true,\"result\":{\"saved\":true}}");
    }
    return TEXT("{\"success\":false,\"error\":\"Failed to save level\"}");
}

FString HandlePlayInEditor(const TSharedPtr<FJsonObject>& Params)
{
    if (GEditor)
    {
        GEditor->RequestPlaySession(FRequestPlaySessionParams());
        return TEXT("{\"success\":true,\"result\":{\"playing\":true}}");
    }
    return TEXT("{\"success\":false,\"error\":\"Editor not available\"}");
}

FString HandleStopPlayInEditor(const TSharedPtr<FJsonObject>& Params)
{
    if (GEditor)
    {
        GEditor->EndPlayMap();
        return TEXT("{\"success\":true,\"result\":{\"stopped\":true}}");
    }
    return TEXT("{\"success\":false,\"error\":\"Editor not available\"}");
}

FString HandleTakeScreenshot(const TSharedPtr<FJsonObject>& Params)
{
    FString Filename = Params->HasField(TEXT("filename"))
        ? Params->GetStringField(TEXT("filename"))
        : TEXT("screenshot");

    FScreenshotRequest::RequestScreenshot(Filename, false, false);

    TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
    Result->SetStringField(TEXT("path"), FString::Printf(TEXT("%s/Saved/Screenshots/%s.png"), *FPaths::ProjectDir(), *Filename));

    FString ResultStr;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ResultStr);
    FJsonSerializer::Serialize(Result.ToSharedRef(), Writer);

    return FString::Printf(TEXT("{\"success\":true,\"result\":%s}"), *ResultStr);
}

FString HandleGenerateCppClass(const TSharedPtr<FJsonObject>& Params)
{
    FString ClassName = Params->GetStringField(TEXT("className"));
    FString ParentClass = Params->GetStringField(TEXT("parentClass"));

    FString ModuleName = Params->HasField(TEXT("module"))
        ? Params->GetStringField(TEXT("module"))
        : FApp::GetProjectName();

    FString ProjectDir = FPaths::ProjectDir();
    FString SourceDir = FPaths::Combine(ProjectDir, TEXT("Source"), ModuleName);
    FString ClassDir = FPaths::Combine(SourceDir, ClassName);

    IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
    if (!PlatformFile.DirectoryExists(*ClassDir))
    {
        PlatformFile.CreateDirectoryTree(*ClassDir);
    }

    FString HeaderContent = FString::Printf(TEXT(
        "#pragma once\n\n"
        "#include \"CoreMinimal.h\"\n"
        "#include \"%s.h\"\n"
        "#include \"%s.generated.h\"\n\n"
        "UCLASS()\n"
        "class %s_API U%s : public %s\n"
        "{\n"
        "\tGENERATED_BODY()\n\n"
        "public:\n"
        "\tU%s();\n\n"
        "protected:\n"
        "\tvirtual void BeginPlay() override;\n\n"
        "public:\n"
        "\tvirtual void Tick(float DeltaTime) override;\n"
        "};\n"),
        *ParentClass, *ClassName,
        *ModuleName.ToUpper(), *ClassName, *ParentClass,
        *ClassName);

    FString CppContent = FString::Printf(TEXT(
        "#include \"%s.h\"\n\n"
        "U%s::U%s()\n"
        "{\n"
        "\tPrimaryComponentTick.bCanEverTick = true;\n"
        "}\n\n"
        "void U%s::BeginPlay()\n"
        "{\n"
        "\tSuper::BeginPlay();\n"
        "}\n\n"
        "void U%s::Tick(float DeltaTime)\n"
        "{\n"
        "\tSuper::Tick(DeltaTime);\n"
        "}\n"),
        *ClassName,
        *ClassName, *ClassName,
        *ClassName,
        *ClassName);

    FString HeaderPath = FPaths::Combine(ClassDir, ClassName + TEXT(".h"));
    FString CppPath = FPaths::Combine(ClassDir, ClassName + TEXT(".cpp"));

    if (!FFileHelper::SaveStringToFile(HeaderContent, *HeaderPath))
    {
        return FString::Printf(TEXT("{\"success\":false,\"error\":\"Failed to write header: %s\"}"), *HeaderPath);
    }

    if (!FFileHelper::SaveStringToFile(CppContent, *CppPath))
    {
        return FString::Printf(TEXT("{\"success\":false,\"error\":\"Failed to write source: %s\"}"), *CppPath);
    }

    TSharedPtr<FJsonObject> ResultObj = MakeShareable(new FJsonObject);
    ResultObj->SetStringField(TEXT("path"), ClassDir);
    ResultObj->SetStringField(TEXT("header"), HeaderPath);
    ResultObj->SetStringField(TEXT("source"), CppPath);

    FString ResultStr;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ResultStr);
    FJsonSerializer::Serialize(ResultObj.ToSharedRef(), Writer);

    return FString::Printf(TEXT("{\"success\":true,\"result\":%s}"), *ResultStr);
}

FString HandleGetCurrentLevel(const TSharedPtr<FJsonObject>& Params)
{
    if (!GEditor)
    {
        return TEXT("{\"success\":false,\"error\":\"Editor not available\"}");
    }

    UWorld* World = GEditor->GetEditorWorldContext().World();
    if (!World)
    {
        return TEXT("{\"success\":false,\"error\":\"No world available\"}");
    }

    FString LevelName = World->GetMapName();
    FString LevelPath = World->GetOutermost()->GetName();

    int32 ActorCount = 0;
    for (TActorIterator<AActor> It(World); It; ++It)
    {
        ActorCount++;
    }

    TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
    Result->SetStringField(TEXT("name"), LevelName);
    Result->SetStringField(TEXT("path"), LevelPath);
    Result->SetNumberField(TEXT("actor_count"), ActorCount);

    FString ResultStr;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ResultStr);
    FJsonSerializer::Serialize(Result.ToSharedRef(), Writer);

    return FString::Printf(TEXT("{\"success\":true,\"result\":%s}"), *ResultStr);
}

FString HandleFocusViewport(const TSharedPtr<FJsonObject>& Params)
{
    if (!GEditor)
    {
        return TEXT("{\"success\":false,\"error\":\"Editor not available\"}");
    }

    UWorld* World = GEditor->GetEditorWorldContext().World();
    if (!World)
    {
        return TEXT("{\"success\":false,\"error\":\"No world available\"}");
    }

    FVector TargetLocation = FVector::ZeroVector;

    if (Params->HasField(TEXT("actorName")))
    {
        FString ActorName = Params->GetStringField(TEXT("actorName"));
        AActor* Actor = nullptr;
        for (TActorIterator<AActor> It(World); It; ++It)
        {
            if (It->GetName() == ActorName)
            {
                Actor = *It;
                break;
            }
        }

        if (!Actor)
        {
            return FString::Printf(TEXT("{\"success\":false,\"error\":\"Actor not found: %s\"}"), *ActorName);
        }

        TargetLocation = Actor->GetActorLocation();
    }
    else if (Params->HasField(TEXT("location")))
    {
        const TArray<TSharedPtr<FJsonValue>>& Arr = Params->GetArrayField(TEXT("location"));
        if (Arr.Num() >= 3)
        {
            TargetLocation.X = Arr[0]->AsNumber();
            TargetLocation.Y = Arr[1]->AsNumber();
            TargetLocation.Z = Arr[2]->AsNumber();
        }
    }
    else
    {
        return TEXT("{\"success\":false,\"error\":\"Provide actorName or location\"}");
    }

    if (GCurrentLevelEditingViewportClient)
    {
        GCurrentLevelEditingViewportClient->SetViewLocation(TargetLocation);
        GCurrentLevelEditingViewportClient->Invalidate();
    }

    TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
    Result->SetStringField(TEXT("focused_at"), FString::Printf(TEXT("[%.0f, %.0f, %.0f]"),
        TargetLocation.X, TargetLocation.Y, TargetLocation.Z));

    FString ResultStr;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ResultStr);
    FJsonSerializer::Serialize(Result.ToSharedRef(), Writer);

    return FString::Printf(TEXT("{\"success\":true,\"result\":%s}"), *ResultStr);
}

FString HandleGetSelectedActors(const TSharedPtr<FJsonObject>& Params)
{
    if (!GEditor)
    {
        return TEXT("{\"success\":false,\"error\":\"Editor not available\"}");
    }

    TArray<TSharedPtr<FJsonValue>> Actors;
    USelection* Selection = GEditor->GetSelectedActors();
    if (Selection)
    {
        for (int32 i = 0; i < Selection->Num(); i++)
        {
            if (AActor* Actor = Cast<AActor>(Selection->GetSelectedObject(i)))
            {
                TSharedPtr<FJsonObject> Obj = MakeShareable(new FJsonObject);
                Obj->SetStringField(TEXT("name"), Actor->GetName());
                Obj->SetStringField(TEXT("class"), Actor->GetClass()->GetName());
                Actors.Add(MakeShareable(new FJsonValueObject(Obj)));
            }
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

FString HandleSelectActor(const TSharedPtr<FJsonObject>& Params)
{
    FString ActorName = Params->GetStringField(TEXT("actorName"));
    bool bAddToSelection = Params->HasField(TEXT("addToSelection"))
        ? Params->GetBoolField(TEXT("addToSelection"))
        : false;

    if (!GEditor)
    {
        return TEXT("{\"success\":false,\"error\":\"Editor not available\"}");
    }

    UWorld* World = GEditor->GetEditorWorldContext().World();
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

    if (!bAddToSelection)
    {
        GEditor->SelectNone(false, true);
    }
    GEditor->SelectActor(TargetActor, true, true, true);

    return FString::Printf(TEXT("{\"success\":true,\"result\":{\"selected\":\"%s\"}}"), *ActorName);
}
