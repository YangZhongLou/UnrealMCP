#include "CoreMinimal.h"
#include "Editor.h"
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
        GEditor->PlayInEditor(PIE_PlayInExistingProcess);
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
