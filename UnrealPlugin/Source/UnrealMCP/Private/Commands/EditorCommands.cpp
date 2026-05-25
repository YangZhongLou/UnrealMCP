#include "CoreMinimal.h"
#include "Editor.h"
#include "LevelEditorViewport.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"

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
