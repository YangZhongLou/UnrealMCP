#include "Umg/UmgSerializer.h"
#include "Widgets/Layout/SBox.h"
#include "Styling/SlateTypes.h"
#include "Styling/CoreStyle.h"

DEFINE_LOG_CATEGORY_STATIC(LogUmgSerializer, Log, All);

static FWidgetNodeDesc ParseNode(const TSharedPtr<FJsonObject>& Obj)
{
    FWidgetNodeDesc Desc;
    Desc.Type = Obj->GetStringField(TEXT("type"));
    Desc.Id = Obj->GetStringField(TEXT("id"));

    if (Obj->HasField(TEXT("properties")))
    {
        TSharedPtr<FJsonObject> Props = Obj->GetObjectField(TEXT("properties"));
        for (const auto& Pair : Props->Values)
        {
            Desc.Properties.Add(FString(Pair.Key), Pair.Value->AsString());
        }
    }

    if (Obj->HasField(TEXT("children")))
    {
        const TArray<TSharedPtr<FJsonValue>>* ChildrenArray = nullptr;
        if (Obj->TryGetArrayField(TEXT("children"), ChildrenArray))
        {
            for (const auto& ChildVal : *ChildrenArray)
            {
                if (ChildVal->Type == EJson::Object)
                {
                    Desc.Children.Add(ParseNode(ChildVal->AsObject()));
                }
            }
        }
    }

    return Desc;
}

static TSharedPtr<FJsonObject> SerializeNode(const FWidgetNodeDesc& Node)
{
    TSharedPtr<FJsonObject> Obj = MakeShareable(new FJsonObject);
    Obj->SetStringField(TEXT("type"), Node.Type);
    Obj->SetStringField(TEXT("id"), Node.Id);

    if (Node.Properties.Num() > 0)
    {
        TSharedPtr<FJsonObject> Props = MakeShareable(new FJsonObject);
        for (const auto& Pair : Node.Properties)
        {
            Props->SetStringField(Pair.Key, Pair.Value);
        }
        Obj->SetObjectField(TEXT("properties"), Props);
    }

    if (Node.Children.Num() > 0)
    {
        TArray<TSharedPtr<FJsonValue>> ChildrenArray;
        for (const auto& Child : Node.Children)
        {
            ChildrenArray.Add(MakeShareable(new FJsonValueObject(SerializeNode(Child))));
        }
        Obj->SetArrayField(TEXT("children"), ChildrenArray);
    }

    return Obj;
}

bool UmgSerializer::ParseWidgetTree(const TSharedPtr<FJsonObject>& JsonRoot, FWidgetNodeDesc& OutRoot)
{
    if (!JsonRoot.IsValid() || !JsonRoot->HasField(TEXT("root")))
    {
        UE_LOG(LogUmgSerializer, Error, TEXT("ParseWidgetTree: missing 'root' field"));
        return false;
    }

    TSharedPtr<FJsonObject> RootObj = JsonRoot->GetObjectField(TEXT("root"));
    if (!RootObj.IsValid())
    {
        return false;
    }

    OutRoot = ParseNode(RootObj);
    return true;
}

TSharedPtr<FJsonObject> UmgSerializer::SerializeWidgetTree(const FWidgetNodeDesc& Root)
{
    TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
    Result->SetObjectField(TEXT("root"), SerializeNode(Root));
    return Result;
}

TSharedPtr<SWidget> UmgSerializer::BuildWidget(const FWidgetNodeDesc& NodeDesc, TMap<FString, TSharedPtr<SWidget>>& OutIdMap)
{
    TSharedPtr<SWidget> Widget;

    if (NodeDesc.Type == TEXT("TextBlock"))
    {
        Widget = BuildTextBlock(NodeDesc);
    }
    else if (NodeDesc.Type == TEXT("Button"))
    {
        Widget = BuildButton(NodeDesc);
    }
    else if (NodeDesc.Type == TEXT("Image"))
    {
        Widget = BuildImage(NodeDesc);
    }
    else if (NodeDesc.Type == TEXT("ProgressBar"))
    {
        Widget = BuildProgressBar(NodeDesc);
    }
    else if (NodeDesc.Type == TEXT("Border"))
    {
        Widget = BuildBorder(NodeDesc);
    }
    else if (NodeDesc.Type == TEXT("VerticalBox"))
    {
        Widget = BuildVerticalBox(NodeDesc, OutIdMap);
    }
    else if (NodeDesc.Type == TEXT("HorizontalBox"))
    {
        Widget = BuildHorizontalBox(NodeDesc, OutIdMap);
    }
    else if (NodeDesc.Type == TEXT("ScrollBox"))
    {
        Widget = BuildScrollBox(NodeDesc, OutIdMap);
    }
    else if (NodeDesc.Type == TEXT("Overlay"))
    {
        Widget = BuildOverlay(NodeDesc, OutIdMap);
    }
    else
    {
        UE_LOG(LogUmgSerializer, Warning, TEXT("Unknown widget type: %s"), *NodeDesc.Type);
        return SNew(SBox);
    }

    if (!NodeDesc.Id.IsEmpty())
    {
        OutIdMap.Add(NodeDesc.Id, Widget);
    }

    return Widget;
}

TSharedPtr<SWidget> UmgSerializer::BuildTextBlock(const FWidgetNodeDesc& Desc)
{
    FString Text = Desc.Properties.FindRef(TEXT("Text"));
    int32 FontSize = 16;
    FString FontSizeStr = Desc.Properties.FindRef(TEXT("Font.Size"));
    if (!FontSizeStr.IsEmpty())
    {
        FontSize = FCString::Atoi(*FontSizeStr);
    }

    FLinearColor Color = FLinearColor::White;
    FString ColorStr = Desc.Properties.FindRef(TEXT("Color"));
    if (!ColorStr.IsEmpty())
    {
        Color = ParseColor(ColorStr);
    }

    return SNew(STextBlock)
        .Text(FText::FromString(Text))
        .Font(FCoreStyle::GetDefaultFontStyle("Regular", FontSize))
        .ColorAndOpacity(Color);
}

TSharedPtr<SWidget> UmgSerializer::BuildButton(const FWidgetNodeDesc& Desc)
{
    FString Text = Desc.Properties.FindRef(TEXT("Text"));
    FLinearColor BgColor = FLinearColor::Gray;
    FString BgColorStr = Desc.Properties.FindRef(TEXT("BackgroundColor"));
    if (!BgColorStr.IsEmpty())
    {
        BgColor = ParseColor(BgColorStr);
    }

    return SNew(SButton)
        .Content()
        [
            SNew(STextBlock)
            .Text(FText::FromString(Text))
        ]
        .ButtonColorAndOpacity(BgColor);
}

TSharedPtr<SWidget> UmgSerializer::BuildImage(const FWidgetNodeDesc& Desc)
{
    FLinearColor BrushColor = FLinearColor::White;
    FString ColorStr = Desc.Properties.FindRef(TEXT("Brush.Color"));
    if (!ColorStr.IsEmpty())
    {
        BrushColor = ParseColor(ColorStr);
    }

    FSlateBrush Brush;
    Brush.TintColor = BrushColor;
    Brush.DrawAs = ESlateBrushDrawType::Box;

    return SNew(SImage).Image(&Brush);
}

TSharedPtr<SWidget> UmgSerializer::BuildProgressBar(const FWidgetNodeDesc& Desc)
{
    float Percent = 0.5f;
    FString PercentStr = Desc.Properties.FindRef(TEXT("Percent"));
    if (!PercentStr.IsEmpty())
    {
        Percent = FCString::Atof(*PercentStr);
    }

    FLinearColor FillColor = FLinearColor::Green;
    FString FillColorStr = Desc.Properties.FindRef(TEXT("FillColor"));
    if (!FillColorStr.IsEmpty())
    {
        FillColor = ParseColor(FillColorStr);
    }

    return SNew(SProgressBar)
        .Percent(Percent)
        .FillColorAndOpacity(FillColor);
}

TSharedPtr<SWidget> UmgSerializer::BuildBorder(const FWidgetNodeDesc& Desc)
{
    FLinearColor BgColor = FLinearColor::Transparent;
    FString BgColorStr = Desc.Properties.FindRef(TEXT("BackgroundColor"));
    if (!BgColorStr.IsEmpty())
    {
        BgColor = ParseColor(BgColorStr);
    }

    FMargin Padding(0);
    FString PaddingStr = Desc.Properties.FindRef(TEXT("Padding"));
    if (!PaddingStr.IsEmpty())
    {
        ParsePadding(PaddingStr, Padding);
    }

    return SNew(SBorder)
        .BorderBackgroundColor(BgColor)
        .Padding(Padding);
}

TSharedPtr<SWidget> UmgSerializer::BuildVerticalBox(const FWidgetNodeDesc& Desc, TMap<FString, TSharedPtr<SWidget>>& OutIdMap)
{
    TSharedPtr<SVerticalBox> Box = SNew(SVerticalBox);

    for (const auto& ChildDesc : Desc.Children)
    {
        TSharedPtr<SWidget> ChildWidget = BuildWidget(ChildDesc, OutIdMap);
        if (ChildWidget.IsValid())
        {
            Box->AddSlot()
            [
                ChildWidget.ToSharedRef()
            ];
            ApplySlotProperties(ChildWidget.ToSharedRef(), ChildDesc.Properties);
        }
    }

    return Box;
}

TSharedPtr<SWidget> UmgSerializer::BuildHorizontalBox(const FWidgetNodeDesc& Desc, TMap<FString, TSharedPtr<SWidget>>& OutIdMap)
{
    TSharedPtr<SHorizontalBox> Box = SNew(SHorizontalBox);

    for (const auto& ChildDesc : Desc.Children)
    {
        TSharedPtr<SWidget> ChildWidget = BuildWidget(ChildDesc, OutIdMap);
        if (ChildWidget.IsValid())
        {
            Box->AddSlot()
            [
                ChildWidget.ToSharedRef()
            ];
            ApplySlotProperties(ChildWidget.ToSharedRef(), ChildDesc.Properties);
        }
    }

    return Box;
}

TSharedPtr<SWidget> UmgSerializer::BuildScrollBox(const FWidgetNodeDesc& Desc, TMap<FString, TSharedPtr<SWidget>>& OutIdMap)
{
    TSharedPtr<SScrollBox> Scroll = SNew(SScrollBox);

    for (const auto& ChildDesc : Desc.Children)
    {
        TSharedPtr<SWidget> ChildWidget = BuildWidget(ChildDesc, OutIdMap);
        if (ChildWidget.IsValid())
        {
            Scroll->AddSlot()
            [
                ChildWidget.ToSharedRef()
            ];
        }
    }

    return Scroll;
}

TSharedPtr<SWidget> UmgSerializer::BuildOverlay(const FWidgetNodeDesc& Desc, TMap<FString, TSharedPtr<SWidget>>& OutIdMap)
{
    TSharedPtr<SOverlay> Overlay = SNew(SOverlay);

    for (const auto& ChildDesc : Desc.Children)
    {
        TSharedPtr<SWidget> ChildWidget = BuildWidget(ChildDesc, OutIdMap);
        if (ChildWidget.IsValid())
        {
            Overlay->AddSlot()
            [
                ChildWidget.ToSharedRef()
            ];
            ApplySlotProperties(ChildWidget.ToSharedRef(), ChildDesc.Properties);
        }
    }

    return Overlay;
}

void UmgSerializer::ApplySlotProperties(const TSharedRef<SWidget>& Child, const TMap<FString, FString>& Properties)
{
    FString HAlignStr = Properties.FindRef(TEXT("Slot.HAlign"));
    if (!HAlignStr.IsEmpty())
    {
        if (HAlignStr == TEXT("Left"))
        {
            // Note: HAlign is set during slot creation, can't change after
            // This is a limitation for now
        }
    }

    FString PaddingStr = Properties.FindRef(TEXT("Slot.Padding"));
    if (!PaddingStr.IsEmpty())
    {
        FMargin Margin(0);
        if (ParsePadding(PaddingStr, Margin))
        {
            // Slot padding can't be changed after creation easily
            // Would need to reconstruct the widget tree
        }
    }
}

bool UmgSerializer::SetWidgetProperty(const TSharedPtr<SWidget>& Widget, const FString& PropertyPath, const FString& Value)
{
    if (!Widget.IsValid())
    {
        return false;
    }

    if (PropertyPath == TEXT("Text"))
    {
        TSharedPtr<STextBlock> TextBlock = StaticCastSharedPtr<STextBlock>(Widget);
        if (TextBlock.IsValid())
        {
            TextBlock->SetText(FText::FromString(Value));
            return true;
        }
    }
    else if (PropertyPath == TEXT("Color"))
    {
        // Slate colors are immutable after creation for most widgets
        // Would need to rebuild the widget
        return false;
    }
    else if (PropertyPath == TEXT("Percent"))
    {
        TSharedPtr<SProgressBar> ProgressBar = StaticCastSharedPtr<SProgressBar>(Widget);
        if (ProgressBar.IsValid())
        {
            ProgressBar->SetPercent(FCString::Atof(*Value));
            return true;
        }
    }

    return false;
}

TSharedPtr<FJsonObject> UmgSerializer::GetHierarchy(const TSharedPtr<SWidget>& Widget, const TMap<FString, TSharedPtr<SWidget>>& IdMap)
{
    TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);

    // Build reverse map: Widget -> ID
    TMap<TSharedPtr<SWidget>, FString> ReverseMap;
    for (const auto& Pair : IdMap)
    {
        ReverseMap.Add(Pair.Value, Pair.Key);
    }

    if (Widget.IsValid())
    {
        FString* FoundId = ReverseMap.Find(Widget);
        if (FoundId)
        {
            Result->SetStringField(TEXT("id"), *FoundId);
        }

        FString WidgetType = TEXT("Unknown");
        if (Widget->GetType() == TEXT("STextBlock"))
        {
            WidgetType = TEXT("TextBlock");
        }
        else if (Widget->GetType() == TEXT("SButton"))
        {
            WidgetType = TEXT("Button");
        }
        else if (Widget->GetType() == TEXT("SImage"))
        {
            WidgetType = TEXT("Image");
        }
        else if (Widget->GetType() == TEXT("SProgressBar"))
        {
            WidgetType = TEXT("ProgressBar");
        }
        else if (Widget->GetType() == TEXT("SBorder"))
        {
            WidgetType = TEXT("Border");
        }
        else if (Widget->GetType() == TEXT("SVerticalBox"))
        {
            WidgetType = TEXT("VerticalBox");
        }
        else if (Widget->GetType() == TEXT("SHorizontalBox"))
        {
            WidgetType = TEXT("HorizontalBox");
        }
        else if (Widget->GetType() == TEXT("SScrollBox"))
        {
            WidgetType = TEXT("ScrollBox");
        }
        else if (Widget->GetType() == TEXT("SOverlay"))
        {
            WidgetType = TEXT("Overlay");
        }
        Result->SetStringField(TEXT("type"), WidgetType);
    }

    return Result;
}

FLinearColor UmgSerializer::ParseColor(const FString& ColorStr)
{
    if (ColorStr.StartsWith(TEXT("#")))
    {
        FString Hex = ColorStr.Mid(1);
        if (Hex.Len() == 6)
        {
            int32 R = FParse::HexNumber(*Hex.Mid(0, 2));
            int32 G = FParse::HexNumber(*Hex.Mid(2, 2));
            int32 B = FParse::HexNumber(*Hex.Mid(4, 2));
            return FLinearColor(R / 255.0f, G / 255.0f, B / 255.0f);
        }
        else if (Hex.Len() == 8)
        {
            int32 R = FParse::HexNumber(*Hex.Mid(0, 2));
            int32 G = FParse::HexNumber(*Hex.Mid(2, 2));
            int32 B = FParse::HexNumber(*Hex.Mid(4, 2));
            int32 A = FParse::HexNumber(*Hex.Mid(6, 2));
            return FLinearColor(R / 255.0f, G / 255.0f, B / 255.0f, A / 255.0f);
        }
    }
    return FLinearColor::White;
}

bool UmgSerializer::ParsePadding(const FString& PaddingStr, FMargin& OutMargin)
{
    TArray<FString> Parts;
    PaddingStr.ParseIntoArray(Parts, TEXT(","));

    if (Parts.Num() == 1)
    {
        float All = FCString::Atof(*Parts[0]);
        OutMargin = FMargin(All);
        return true;
    }
    else if (Parts.Num() == 4)
    {
        OutMargin = FMargin(
            FCString::Atof(*Parts[0]),
            FCString::Atof(*Parts[1]),
            FCString::Atof(*Parts[2]),
            FCString::Atof(*Parts[3]));
        return true;
    }

    return false;
}
