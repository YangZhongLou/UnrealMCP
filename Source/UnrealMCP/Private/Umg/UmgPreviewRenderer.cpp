#include "Umg/UmgPreviewRenderer.h"
#include "Widgets/SWindow.h"
#include "Framework/Application/SlateApplication.h"

DEFINE_LOG_CATEGORY_STATIC(LogUmgPreviewRenderer, Log, All);

UmgPreviewRenderer::UmgPreviewRenderer()
{
}

UmgPreviewRenderer::~UmgPreviewRenderer()
{
    CloseAllPreviews();
}

bool UmgPreviewRenderer::CreatePreview(const FString& PreviewId, const TSharedPtr<SWidget>& WidgetTree, int32 Width, int32 Height)
{
    FScopeLock Lock(&PreviewsLock);

    if (Previews.Contains(PreviewId))
    {
        UE_LOG(LogUmgPreviewRenderer, Warning, TEXT("Preview %s already exists, closing old one"), *PreviewId);
        ClosePreview(PreviewId);
    }

    if (!WidgetTree.IsValid())
    {
        UE_LOG(LogUmgPreviewRenderer, Error, TEXT("Cannot create preview %s: WidgetTree is null"), *PreviewId);
        return false;
    }

    TSharedPtr<SWindow> Window = SNew(SWindow)
        .Title(FText::FromString(FString::Printf(TEXT("MCP Preview: %s"), *PreviewId)))
        .ClientSize(FVector2D(Width, Height))
        .AutoCenter(EAutoCenter::PreferredWorkArea)
        .SizingRule(ESizingRule::UserSized)
        .IsInitiallyMaximized(false)
        .SupportsMaximize(true)
        .SupportsMinimize(true);

    Window->SetContent(WidgetTree.ToSharedRef());

    FSlateApplication::Get().AddWindow(Window.ToSharedRef());

    FPreviewInstance Instance;
    Instance.Window = Window;
    Instance.RootWidget = WidgetTree;

    Previews.Add(PreviewId, MoveTemp(Instance));

    UE_LOG(LogUmgPreviewRenderer, Log, TEXT("Created preview %s (%dx%d)"), *PreviewId, Width, Height);
    return true;
}

bool UmgPreviewRenderer::UpdatePreview(const FString& PreviewId, const TSharedPtr<SWidget>& WidgetTree)
{
    FScopeLock Lock(&PreviewsLock);

    FPreviewInstance* Instance = Previews.Find(PreviewId);
    if (!Instance)
    {
        UE_LOG(LogUmgPreviewRenderer, Warning, TEXT("Cannot update preview %s: not found"), *PreviewId);
        return false;
    }

    if (!WidgetTree.IsValid())
    {
        return false;
    }

    if (Instance->Window.IsValid())
    {
        Instance->Window->SetContent(WidgetTree.ToSharedRef());
        Instance->RootWidget = WidgetTree;
    }

    return true;
}

bool UmgPreviewRenderer::ClosePreview(const FString& PreviewId)
{
    FScopeLock Lock(&PreviewsLock);

    FPreviewInstance* Instance = Previews.Find(PreviewId);
    if (!Instance)
    {
        return false;
    }

    if (Instance->Window.IsValid())
    {
        Instance->Window->RequestDestroyWindow();
    }

    Previews.Remove(PreviewId);
    UE_LOG(LogUmgPreviewRenderer, Log, TEXT("Closed preview %s"), *PreviewId);
    return true;
}

TSharedPtr<SWidget> UmgPreviewRenderer::GetPreviewWidget(const FString& PreviewId) const
{
    FScopeLock Lock(const_cast<FCriticalSection*>(&PreviewsLock));

    const FPreviewInstance* Instance = Previews.Find(PreviewId);
    if (Instance)
    {
        return Instance->RootWidget;
    }

    return nullptr;
}

TMap<FString, TSharedPtr<SWidget>>* UmgPreviewRenderer::GetIdMap(const FString& PreviewId)
{
    FScopeLock Lock(&PreviewsLock);

    FPreviewInstance* Instance = Previews.Find(PreviewId);
    if (Instance)
    {
        return &Instance->IdMap;
    }

    return nullptr;
}

void UmgPreviewRenderer::CloseAllPreviews()
{
    FScopeLock Lock(&PreviewsLock);

    for (auto& Pair : Previews)
    {
        if (Pair.Value.Window.IsValid())
        {
            Pair.Value.Window->RequestDestroyWindow();
        }
    }

    Previews.Empty();
}

bool UmgPreviewRenderer::HasPreview(const FString& PreviewId) const
{
    FScopeLock Lock(const_cast<FCriticalSection*>(&PreviewsLock));
    return Previews.Contains(PreviewId);
}
