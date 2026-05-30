#pragma once

#include "CoreMinimal.h"
#include "Widgets/SWindow.h"

/** UMG 预览窗口管理器 */
class UNREALMCP_API UmgPreviewRenderer
{
public:
    UmgPreviewRenderer();
    ~UmgPreviewRenderer();

    /** 创建预览窗口并渲染 Widget 树 */
    bool CreatePreview(const FString& PreviewId, const TSharedPtr<SWidget>& WidgetTree, int32 Width = 400, int32 Height = 300);

    /** 更新预览窗口中的 Widget 树 */
    bool UpdatePreview(const FString& PreviewId, const TSharedPtr<SWidget>& WidgetTree);

    /** 关闭预览窗口 */
    bool ClosePreview(const FString& PreviewId);

    /** 获取预览窗口中的 Widget 根节点 */
    TSharedPtr<SWidget> GetPreviewWidget(const FString& PreviewId) const;

    /** 获取预览窗口的 ID→Widget 映射 */
    TMap<FString, TSharedPtr<SWidget>>* GetIdMap(const FString& PreviewId);

    /** 关闭所有预览窗口 */
    void CloseAllPreviews();

    /** 检查预览窗口是否存在 */
    bool HasPreview(const FString& PreviewId) const;

private:
    struct FPreviewInstance
    {
        TSharedPtr<SWindow> Window;
        TSharedPtr<SWidget> RootWidget;
        TMap<FString, TSharedPtr<SWidget>> IdMap;
    };

    TMap<FString, FPreviewInstance> Previews;
    FCriticalSection PreviewsLock;
};
