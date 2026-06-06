#pragma once

#include "CoreMinimal.h"
#include "Widgets/SWidget.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Notifications/SProgressBar.h"
#include "Widgets/SOverlay.h"
#include "Styling/SlateBrush.h"
#include "Dom/JsonObject.h"

/** Widget 树节点描述 */
struct FWidgetNodeDesc
{
    FString Type;
    FString Id;
    TMap<FString, FString> Properties;
    TArray<FWidgetNodeDesc> Children;
};

/** UMG Widget 序列化器 — JSON ↔ SWidget */
class UNREALMCP_API UmgSerializer
{
public:
    /** 从 JSON 解析 Widget 树描述 */
    static bool ParseWidgetTree(const TSharedPtr<FJsonObject>& JsonRoot, FWidgetNodeDesc& OutRoot);

    /** 将 Widget 树描述序列化为 JSON */
    static TSharedPtr<FJsonObject> SerializeWidgetTree(const FWidgetNodeDesc& Root);

    /** 构建 SWidget 树（返回根 Widget 和 ID→Widget 映射） */
    static TSharedPtr<SWidget> BuildWidget(const FWidgetNodeDesc& NodeDesc, TMap<FString, TSharedPtr<SWidget>>& OutIdMap);

    /** 修改已构建 Widget 的属性 */
    static bool SetWidgetProperty(const TSharedPtr<SWidget>& Widget, const FString& PropertyPath, const FString& Value);

    /** 获取 Widget 层级结构（用于 get_umg_hierarchy） */
    static TSharedPtr<FJsonObject> GetHierarchy(const TSharedPtr<SWidget>& Widget, const TMap<FString, TSharedPtr<SWidget>>& IdMap);

private:
    static TSharedPtr<SWidget> BuildTextBlock(const FWidgetNodeDesc& Desc);
    static TSharedPtr<SWidget> BuildButton(const FWidgetNodeDesc& Desc);
    static TSharedPtr<SWidget> BuildImage(const FWidgetNodeDesc& Desc);
    static TSharedPtr<SWidget> BuildProgressBar(const FWidgetNodeDesc& Desc);
    static TSharedPtr<SWidget> BuildBorder(const FWidgetNodeDesc& Desc);
    static TSharedPtr<SWidget> BuildVerticalBox(const FWidgetNodeDesc& Desc, TMap<FString, TSharedPtr<SWidget>>& OutIdMap);
    static TSharedPtr<SWidget> BuildHorizontalBox(const FWidgetNodeDesc& Desc, TMap<FString, TSharedPtr<SWidget>>& OutIdMap);
    static TSharedPtr<SWidget> BuildScrollBox(const FWidgetNodeDesc& Desc, TMap<FString, TSharedPtr<SWidget>>& OutIdMap);
    static TSharedPtr<SWidget> BuildOverlay(const FWidgetNodeDesc& Desc, TMap<FString, TSharedPtr<SWidget>>& OutIdMap);

    static void ApplySlotProperties(const TSharedRef<SWidget>& Child, const TMap<FString, FString>& Properties);
    static FLinearColor ParseColor(const FString& ColorStr);
    static bool ParsePadding(const FString& PaddingStr, FMargin& OutMargin);
};
