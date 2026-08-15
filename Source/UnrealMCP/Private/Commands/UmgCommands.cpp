#if WITH_EDITOR
#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Async/Async.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "WidgetBlueprintFactory.h"
#include "WidgetBlueprint.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Widget.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/NamedSlot.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Animation/WidgetAnimation.h"
#include "Animation/WidgetAnimationBinding.h"
#include "MovieScene.h"
#include "Channels/MovieSceneFloatChannel.h"
#include "Sections/MovieSceneFloatSection.h"
#include "Tracks/MovieSceneFloatTrack.h"
#include "K2Node_Event.h"
#include "K2Node_CallFunction.h"
#include "K2Node_VariableGet.h"
#include "K2Node_WidgetAnimationEvent.h"
#include "EdGraphSchema_K2.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphPin.h"
#include "Components/VerticalBox.h"
#include "Components/HorizontalBox.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Components/Border.h"
#include "Components/Slider.h"
#include "Components/ComboBoxString.h"
#include "Components/CheckBox.h"
#include "Components/EditableTextBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/HorizontalBoxSlot.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "UObject/SavePackage.h"
#include "Misc/App.h"
#include "Misc/PackageName.h"
#include "Misc/FileHelper.h"
#include "EditorAssetLibrary.h"
#include "Editor.h"
#include "Slate/WidgetRenderer.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "ImageUtils.h"
#include "Styling/SlateBrush.h"
#include "Styling/SlateTypes.h"

namespace HtmlUmgGen
{
	static FLinearColor ParseHexColor(const FString& Hex, float DefaultA = 1.0f)
	{
		FString H = Hex.TrimStartAndEnd();
		if (H.StartsWith(TEXT("#")))
		{
			H.RightChopInline(1);
		}
		auto Nibble = [](TCHAR C) -> int32
		{
			if (C >= '0' && C <= '9') { return C - '0'; }
			if (C >= 'a' && C <= 'f') { return C - 'a' + 10; }
			if (C >= 'A' && C <= 'F') { return C - 'A' + 10; }
			return 0;
		};
		if (H.Len() == 6)
		{
			const int32 R = Nibble(H[0]) * 16 + Nibble(H[1]);
			const int32 G = Nibble(H[2]) * 16 + Nibble(H[3]);
			const int32 B = Nibble(H[4]) * 16 + Nibble(H[5]);
			return FLinearColor(R / 255.0f, G / 255.0f, B / 255.0f, DefaultA);
		}
		if (H.Len() == 8)
		{
			const int32 R = Nibble(H[0]) * 16 + Nibble(H[1]);
			const int32 G = Nibble(H[2]) * 16 + Nibble(H[3]);
			const int32 B = Nibble(H[4]) * 16 + Nibble(H[5]);
			const int32 A = Nibble(H[6]) * 16 + Nibble(H[7]);
			return FLinearColor(R / 255.0f, G / 255.0f, B / 255.0f, A / 255.0f);
		}
		return FLinearColor(0.08f, 0.07f, 0.06f, DefaultA);
	}

	static FAnchors ParseAnchors(const FString& Key)
	{
		if (Key.Equals(TEXT("top-right"), ESearchCase::IgnoreCase))
		{
			return FAnchors(1.f, 0.f, 1.f, 0.f);
		}
		if (Key.Equals(TEXT("bottom-left"), ESearchCase::IgnoreCase))
		{
			return FAnchors(0.f, 1.f, 0.f, 1.f);
		}
		if (Key.Equals(TEXT("bottom-right"), ESearchCase::IgnoreCase))
		{
			return FAnchors(1.f, 1.f, 1.f, 1.f);
		}
		if (Key.Equals(TEXT("fill"), ESearchCase::IgnoreCase))
		{
			return FAnchors(0.f, 0.f, 1.f, 1.f);
		}
		if (Key.Equals(TEXT("center"), ESearchCase::IgnoreCase))
		{
			return FAnchors(0.5f, 0.5f, 0.5f, 0.5f);
		}
		return FAnchors(0.f, 0.f, 0.f, 0.f); // top-left
	}

	static bool TryParseVisibility(const FString& Key, ESlateVisibility& OutVisibility)
	{
		if (Key.Equals(TEXT("visible"), ESearchCase::IgnoreCase))
		{
			OutVisibility = ESlateVisibility::Visible;
			return true;
		}
		if (Key.Equals(TEXT("collapsed"), ESearchCase::IgnoreCase))
		{
			OutVisibility = ESlateVisibility::Collapsed;
			return true;
		}
		if (Key.Equals(TEXT("hidden"), ESearchCase::IgnoreCase))
		{
			OutVisibility = ESlateVisibility::Hidden;
			return true;
		}
		if (Key.Equals(TEXT("hit-test-invisible"), ESearchCase::IgnoreCase))
		{
			OutVisibility = ESlateVisibility::HitTestInvisible;
			return true;
		}
		if (Key.Equals(TEXT("self-hit-test-invisible"), ESearchCase::IgnoreCase))
		{
			OutVisibility = ESlateVisibility::SelfHitTestInvisible;
			return true;
		}
		return false;
	}

	/** Explicit visibility wins; otherwise panels pass empty hits, text ignores hits, chrome images block. */
	static ESlateVisibility DefaultVisibilityFor(UWidget* Widget)
	{
		if (!Widget)
		{
			return ESlateVisibility::Visible;
		}
		if (Cast<UButton>(Widget)
			|| Cast<USlider>(Widget)
			|| Cast<UComboBoxString>(Widget)
			|| Cast<UCheckBox>(Widget))
		{
			return ESlateVisibility::Visible;
		}
		if (Cast<UTextBlock>(Widget))
		{
			return ESlateVisibility::HitTestInvisible;
		}
		if (Cast<UImage>(Widget))
		{
			// Opaque chrome rects should still swallow map clicks under panels.
			return ESlateVisibility::Visible;
		}
		if (Cast<UPanelWidget>(Widget))
		{
			return ESlateVisibility::SelfHitTestInvisible;
		}
		return ESlateVisibility::Visible;
	}

	static void ApplyVisibility(UWidget* Widget, const TSharedPtr<FJsonObject>& JsonObj)
	{
		if (!Widget)
		{
			return;
		}
		FString VisibilityKey;
		ESlateVisibility Visibility = DefaultVisibilityFor(Widget);
		if (JsonObj.IsValid()
			&& JsonObj->TryGetStringField(TEXT("visibility"), VisibilityKey)
			&& TryParseVisibility(VisibilityKey, Visibility))
		{
			Widget->SetVisibility(Visibility);
			return;
		}
		Widget->SetVisibility(Visibility);
	}

	/** Absolute design-space (x,y,w,h) → Canvas slot position for corner anchors. */
	static FVector2D AbsoluteToSlotPosition(
		float X, float Y, float W, float H,
		const FAnchors& Anchors,
		float CanvasW, float CanvasH)
	{
		const bool bRight = FMath::IsNearlyEqual(Anchors.Minimum.X, 1.f) && FMath::IsNearlyEqual(Anchors.Maximum.X, 1.f);
		const bool bBottom = FMath::IsNearlyEqual(Anchors.Minimum.Y, 1.f) && FMath::IsNearlyEqual(Anchors.Maximum.Y, 1.f);
		const bool bCenterX = FMath::IsNearlyEqual(Anchors.Minimum.X, 0.5f) && FMath::IsNearlyEqual(Anchors.Maximum.X, 0.5f);
		const bool bCenterY = FMath::IsNearlyEqual(Anchors.Minimum.Y, 0.5f) && FMath::IsNearlyEqual(Anchors.Maximum.Y, 0.5f);
		const bool bFillX = !FMath::IsNearlyEqual(Anchors.Minimum.X, Anchors.Maximum.X);
		const bool bFillY = !FMath::IsNearlyEqual(Anchors.Minimum.Y, Anchors.Maximum.Y);

		if (bFillX || bFillY)
		{
			return FVector2D(X, Y);
		}

		float PosX = X;
		float PosY = Y;
		if (bRight)
		{
			PosX = X - CanvasW;
		}
		else if (bCenterX)
		{
			PosX = X - CanvasW * 0.5f;
		}
		if (bBottom)
		{
			PosY = Y - CanvasH;
		}
		else if (bCenterY)
		{
			PosY = Y - CanvasH * 0.5f;
		}
		(void)W;
		(void)H;
		return FVector2D(PosX, PosY);
	}

	static UClass* ResolveWidgetClass(const FString& TypeName)
	{
		static TMap<FString, UClass*> Map;
		if (Map.Num() == 0)
		{
			Map.Add(TEXT("CanvasPanel"), UCanvasPanel::StaticClass());
			Map.Add(TEXT("VerticalBox"), UVerticalBox::StaticClass());
			Map.Add(TEXT("HorizontalBox"), UHorizontalBox::StaticClass());
			Map.Add(TEXT("Overlay"), UOverlay::StaticClass());
			Map.Add(TEXT("TextBlock"), UTextBlock::StaticClass());
			Map.Add(TEXT("Button"), UButton::StaticClass());
			Map.Add(TEXT("Image"), UImage::StaticClass());
			Map.Add(TEXT("ProgressBar"), UProgressBar::StaticClass());
			Map.Add(TEXT("Border"), UBorder::StaticClass());
			Map.Add(TEXT("Slider"), USlider::StaticClass());
			Map.Add(TEXT("ComboBoxString"), UComboBoxString::StaticClass());
			Map.Add(TEXT("CheckBox"), UCheckBox::StaticClass());
			Map.Add(TEXT("EditableTextBox"), UEditableTextBox::StaticClass());
		}
		if (UClass* const* Found = Map.Find(TypeName))
		{
			return *Found;
		}
		return UTextBlock::StaticClass();
	}

	static FString ResolveBrushObjectPath(const FString& InPath)
	{
		FString Path = InPath.TrimStartAndEnd();
		if (Path.IsEmpty() || Path.Contains(TEXT(".")))
		{
			return Path;
		}
		const FString AssetName = FPackageName::GetShortName(Path);
		return FString::Printf(TEXT("%s.%s"), *Path, *AssetName);
	}

	static void ApplyTextureBrush(UWidget* Widget, UTexture2D* Texture)
	{
		if (!Widget || !Texture)
		{
			return;
		}
		if (UImage* Img = Cast<UImage>(Widget))
		{
			FSlateBrush Brush;
			Brush.SetResourceObject(Texture);
			Brush.ImageSize = FVector2D(
				static_cast<float>(Texture->GetSizeX()),
				static_cast<float>(Texture->GetSizeY()));
			Brush.DrawAs = ESlateBrushDrawType::Image;
			Brush.TintColor = FSlateColor(FLinearColor::White);
			Brush.Tiling = ESlateBrushTileType::NoTile;
			Img->SetBrush(Brush);
			Img->SetColorAndOpacity(FLinearColor::White);
			return;
		}
		if (UButton* Btn = Cast<UButton>(Widget))
		{
			FSlateBrush Brush;
			Brush.SetResourceObject(Texture);
			Brush.ImageSize = FVector2D(256.f, 64.f);
			Brush.DrawAs = ESlateBrushDrawType::Box;
			Brush.Margin = FMargin(0.14f, 0.16f, 0.14f, 0.16f);
			Brush.TintColor = FSlateColor(FLinearColor::White);
			Brush.Tiling = ESlateBrushTileType::NoTile;

			FSlateBrush Hovered = Brush;
			Hovered.TintColor = FSlateColor(FLinearColor(1.08f, 1.08f, 1.08f, 1.f));
			FSlateBrush Pressed = Brush;
			Pressed.TintColor = FSlateColor(FLinearColor(0.92f, 0.92f, 0.92f, 1.f));

			FButtonStyle Style = Btn->GetStyle();
			Style.Normal = Brush;
			Style.Hovered = Hovered;
			Style.Pressed = Pressed;
			Style.Disabled = Brush;
			Style.NormalPadding = FMargin(0.f);
			Style.PressedPadding = FMargin(0.f);
			Btn->SetStyle(Style);
		}
	}

	static void ApplyStyle(UWidget* Widget, const TSharedPtr<FJsonObject>& Style)
	{
		if (!Widget || !Style.IsValid())
		{
			return;
		}

		FString BrushPath;
		const bool bHasBrush = Style->TryGetStringField(TEXT("brush"), BrushPath) && !BrushPath.IsEmpty();
		if (bHasBrush)
		{
			const FString ObjectPath = ResolveBrushObjectPath(BrushPath);
			if (UTexture2D* Tex = LoadObject<UTexture2D>(nullptr, *ObjectPath))
			{
				ApplyTextureBrush(Widget, Tex);
			}
		}

		FString ColorHex;
		// Flat fill only when no brush (brush is the visual truth for Image / Button chrome).
		if (!bHasBrush && Style->TryGetStringField(TEXT("background_color"), ColorHex))
		{
			double Opacity = 0.92;
			if (!Style->TryGetNumberField(TEXT("opacity"), Opacity))
			{
				FString OpacityText;
				if (Style->TryGetStringField(TEXT("opacity"), OpacityText))
				{
					Opacity = FCString::Atod(*OpacityText);
				}
			}
			const FLinearColor C = ParseHexColor(
				ColorHex,
				FMath::Clamp(static_cast<float>(Opacity), 0.0f, 1.0f));
			if (UImage* Img = Cast<UImage>(Widget))
			{
				Img->SetColorAndOpacity(C);
			}
			else if (UBorder* Border = Cast<UBorder>(Widget))
			{
				Border->SetBrushColor(C);
			}
			else if (UButton* Btn = Cast<UButton>(Widget))
			{
				FButtonStyle BtnStyle = Btn->GetStyle();
				BtnStyle.Normal.TintColor = FSlateColor(C);
				BtnStyle.Hovered.TintColor = FSlateColor(C * 1.1f);
				BtnStyle.Pressed.TintColor = FSlateColor(C * 0.9f);
				Btn->SetStyle(BtnStyle);
			}
		}

		FString TextColor;
		double FontSize = 14.0;
		Style->TryGetNumberField(TEXT("font_size"), FontSize);
		if (Style->TryGetStringField(TEXT("color"), TextColor))
		{
			if (UTextBlock* Text = Cast<UTextBlock>(Widget))
			{
				Text->SetColorAndOpacity(FSlateColor(ParseHexColor(TextColor)));
				FSlateFontInfo Font = Text->GetFont();
				Font.Size = static_cast<int32>(FontSize);
				Text->SetFont(Font);
			}
		}
	}

	static void SetupCanvasChild(
		UWidget* Child,
		const TSharedPtr<FJsonObject>& JsonObj,
		float CanvasW,
		float CanvasH)
	{
		UCanvasPanelSlot* Slot = Cast<UCanvasPanelSlot>(Child->Slot);
		if (!Slot)
		{
			return;
		}
		double X = 0, Y = 0, W = 100, H = 40;
		JsonObj->TryGetNumberField(TEXT("x"), X);
		JsonObj->TryGetNumberField(TEXT("y"), Y);
		JsonObj->TryGetNumberField(TEXT("width"), W);
		JsonObj->TryGetNumberField(TEXT("height"), H);
		double ZOrder = 0.0;
		JsonObj->TryGetNumberField(TEXT("z_order"), ZOrder);

		FString AnchorKey = TEXT("top-left");
		JsonObj->TryGetStringField(TEXT("anchors"), AnchorKey);
		const FAnchors Anchors = ParseAnchors(AnchorKey);
		Slot->SetAnchors(Anchors);
		Slot->SetAlignment(FVector2D(0.f, 0.f));
		Slot->SetAutoSize(false);
		Slot->SetZOrder(static_cast<int32>(ZOrder));
		if (AnchorKey.Equals(TEXT("fill"), ESearchCase::IgnoreCase))
		{
			Slot->SetOffsets(FMargin(0.f));
		}
		else
		{
			const FVector2D Pos = AbsoluteToSlotPosition(
				static_cast<float>(X), static_cast<float>(Y),
				static_cast<float>(W), static_cast<float>(H),
				Anchors, CanvasW, CanvasH);
			Slot->SetPosition(Pos);
			Slot->SetSize(FVector2D(static_cast<float>(W), static_cast<float>(H)));
		}
	}

	static UWidget* BuildWidgetRecursive(
		UWidgetTree* Tree,
		UPanelWidget* Parent,
		const TSharedPtr<FJsonObject>& JsonObj,
		float CanvasW,
		float CanvasH,
		int32& OutCount)
	{
		if (!Tree || !JsonObj.IsValid())
		{
			return nullptr;
		}

		FString TypeName = TEXT("TextBlock");
		JsonObj->TryGetStringField(TEXT("type"), TypeName);
		FString Name = TEXT("Widget");
		JsonObj->TryGetStringField(TEXT("name"), Name);

		UClass* Class = ResolveWidgetClass(TypeName);
		UWidget* Widget = Tree->ConstructWidget<UWidget>(Class, FName(*Name));
		if (!Widget)
		{
			return nullptr;
		}
		++OutCount;
		ApplyVisibility(Widget, JsonObj);

		FString Text;
		if (JsonObj->TryGetStringField(TEXT("text"), Text) && !Text.IsEmpty())
		{
			if (UTextBlock* TB = Cast<UTextBlock>(Widget))
			{
				TB->SetText(FText::FromString(Text));
			}
			else if (UButton* Btn = Cast<UButton>(Widget))
			{
				UTextBlock* Label = Tree->ConstructWidget<UTextBlock>(
					UTextBlock::StaticClass(), FName(*(Name + TEXT("_Label"))));
				Label->SetText(FText::FromString(Text));
				Label->SetJustification(ETextJustify::Center);
				Label->SetVisibility(ESlateVisibility::HitTestInvisible);
				Btn->SetContent(Label);
				++OutCount;
			}
		}

		const TSharedPtr<FJsonObject>* StyleObj = nullptr;
		if (JsonObj->TryGetObjectField(TEXT("style"), StyleObj) && StyleObj && StyleObj->IsValid())
		{
			ApplyStyle(Widget, *StyleObj);
			if (UButton* Btn = Cast<UButton>(Widget))
			{
				if (UTextBlock* Label = Cast<UTextBlock>(Btn->GetContent()))
				{
					ApplyStyle(Label, *StyleObj);
				}
			}
		}

		if (Parent)
		{
			Parent->AddChild(Widget);
			if (Cast<UCanvasPanel>(Parent))
			{
				SetupCanvasChild(Widget, JsonObj, CanvasW, CanvasH);
			}
			else if (UVerticalBoxSlot* VSlot = Cast<UVerticalBoxSlot>(Widget->Slot))
			{
				VSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			}
			else if (UHorizontalBoxSlot* HSlot = Cast<UHorizontalBoxSlot>(Widget->Slot))
			{
				HSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			}
			else if (UOverlaySlot* OSlot = Cast<UOverlaySlot>(Widget->Slot))
			{
				OSlot->SetHorizontalAlignment(HAlign_Fill);
				OSlot->SetVerticalAlignment(VAlign_Fill);
			}
		}

		UPanelWidget* AsPanel = Cast<UPanelWidget>(Widget);
		const TArray<TSharedPtr<FJsonValue>>* Children = nullptr;
		if (AsPanel && JsonObj->TryGetArrayField(TEXT("children"), Children) && Children)
		{
			for (const TSharedPtr<FJsonValue>& ChildVal : *Children)
			{
				const TSharedPtr<FJsonObject>* ChildObj = nullptr;
				if (ChildVal.IsValid() && ChildVal->TryGetObject(ChildObj) && ChildObj && ChildObj->IsValid())
				{
					BuildWidgetRecursive(Tree, AsPanel, *ChildObj, CanvasW, CanvasH, OutCount);
				}
			}
		}
		return Widget;
	}

	static bool DeleteExisting(const FString& PackagePath, FString& OutError)
	{
		if (!UEditorAssetLibrary::DoesAssetExist(PackagePath))
		{
			return true;
		}
		if (!UEditorAssetLibrary::DeleteAsset(PackagePath))
		{
			OutError = FString::Printf(TEXT("无法删除已有资产: %s"), *PackagePath);
			return false;
		}
		return true;
	}
}

FString HandleGenerateUmgWidget(const TSharedPtr<FJsonObject>& Params)
{
	if (!Params.IsValid())
	{
		return TEXT("{\"success\":false,\"error\":\"Missing params\"}");
	}

	TSharedPtr<FJsonObject> Tree = Params;
	const TSharedPtr<FJsonObject>* Nested = nullptr;
	if (Params->TryGetObjectField(TEXT("widget_tree"), Nested) && Nested && Nested->IsValid())
	{
		Tree = *Nested;
	}

	FString ErrorMsg;
	TSharedPtr<FJsonObject> ResponseJson;
	FEvent* DoneEvent = FPlatformProcess::GetSynchEventFromPool();

	AsyncTask(ENamedThreads::GameThread, [&]()
	{
		// UE 5.8: socket-thread → GameThread AsyncTask arrives without FAppTime TLS.
		// Asset replacement enqueues renderer work; seed the GT time context so those
		// render tasks inherit a valid FAppTime instead of Ensuring in AppTime.cpp.
		FApp::SetCurrentTime(FApp::GetCurrentTime());

		const TArray<TSharedPtr<FJsonValue>>* Widgets = nullptr;
		if (!Tree.IsValid()
			|| !Tree->TryGetArrayField(TEXT("widgets"), Widgets)
			|| !Widgets
			|| Widgets->Num() == 0)
		{
			ErrorMsg = TEXT("widget_tree.widgets 为空");
			DoneEvent->Trigger();
			return;
		}

		const TSharedPtr<FJsonObject>* RootObj = nullptr;
		if (!(*Widgets)[0]->TryGetObject(RootObj) || !RootObj || !RootObj->IsValid())
		{
			ErrorMsg = TEXT("widgets[0] 不是对象");
			DoneEvent->Trigger();
			return;
		}

		FString RootType = TEXT("CanvasPanel");
		(*RootObj)->TryGetStringField(TEXT("type"), RootType);
		UClass* RootClass = HtmlUmgGen::ResolveWidgetClass(RootType);
		if (!RootClass || !RootClass->IsChildOf(UPanelWidget::StaticClass()))
		{
			ErrorMsg = FString::Printf(TEXT("根控件必须是 PanelWidget: %s"), *RootType);
			DoneEvent->Trigger();
			return;
		}
		FString RootName = TEXT("RootCanvas");
		(*RootObj)->TryGetStringField(TEXT("name"), RootName);

		float CanvasW = 1920.f;
		float CanvasH = 1080.f;
		Tree->TryGetNumberField(TEXT("canvas_width"), CanvasW);
		Tree->TryGetNumberField(TEXT("canvas_height"), CanvasH);

		FString OutputPath = TEXT("/Game/UI/HUD");
		Tree->TryGetStringField(TEXT("output_path"), OutputPath);
		FString BlueprintName = TEXT("WBP_FromHtml");
		Tree->TryGetStringField(TEXT("blueprint_name"), BlueprintName);
		if (!OutputPath.StartsWith(TEXT("/Game/"))
			|| BlueprintName.IsEmpty()
			|| BlueprintName.Contains(TEXT("/")))
		{
			ErrorMsg = FString::Printf(
				TEXT("无效输出位置: output_path=%s blueprint_name=%s"),
				*OutputPath,
				*BlueprintName);
			DoneEvent->Trigger();
			return;
		}

		const FString PackagePath = OutputPath / BlueprintName;
		if (!HtmlUmgGen::DeleteExisting(PackagePath, ErrorMsg))
		{
			DoneEvent->Trigger();
			return;
		}

		UWidgetBlueprintFactory* Factory = NewObject<UWidgetBlueprintFactory>();
		Factory->ParentClass = UUserWidget::StaticClass();

		IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
		UObject* NewAsset = AssetTools.CreateAsset(
			BlueprintName,
			OutputPath,
			UWidgetBlueprint::StaticClass(),
			Factory);

		UWidgetBlueprint* WidgetBP = Cast<UWidgetBlueprint>(NewAsset);
		if (!WidgetBP || !WidgetBP->WidgetTree)
		{
			ErrorMsg = TEXT("Failed to create WidgetBlueprint");
			DoneEvent->Trigger();
			return;
		}

		int32 Count = 0;
		UWidget* Root = WidgetBP->WidgetTree->ConstructWidget<UWidget>(
			RootClass, FName(*RootName));
		HtmlUmgGen::ApplyVisibility(Root, *RootObj);
		WidgetBP->WidgetTree->RootWidget = Root;
		Count = 1;

		UPanelWidget* RootPanel = Cast<UPanelWidget>(Root);
		const TArray<TSharedPtr<FJsonValue>>* Children = nullptr;
		if (RootPanel && (*RootObj)->TryGetArrayField(TEXT("children"), Children) && Children)
		{
			for (const TSharedPtr<FJsonValue>& ChildVal : *Children)
			{
				const TSharedPtr<FJsonObject>* ChildObj = nullptr;
				if (ChildVal.IsValid() && ChildVal->TryGetObject(ChildObj) && ChildObj && ChildObj->IsValid())
				{
					HtmlUmgGen::BuildWidgetRecursive(
						WidgetBP->WidgetTree, RootPanel, *ChildObj, CanvasW, CanvasH, Count);
				}
			}
		}

		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBP);
		FKismetEditorUtilities::CompileBlueprint(WidgetBP);

		const FString PackageFilename = FPackageName::LongPackageNameToFilename(
			PackagePath, FPackageName::GetAssetPackageExtension());
		UPackage* Package = WidgetBP->GetOutermost();
		Package->MarkPackageDirty();
		FSavePackageArgs SaveArgs;
		SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
		if (!UPackage::SavePackage(Package, WidgetBP, *PackageFilename, SaveArgs))
		{
			ErrorMsg = FString::Printf(TEXT("保存 WidgetBlueprint 失败: %s"), *PackageFilename);
			DoneEvent->Trigger();
			return;
		}

		TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
		Result->SetStringField(TEXT("blueprint_path"), PackagePath);
		Result->SetStringField(TEXT("blueprint_name"), BlueprintName);
		Result->SetNumberField(TEXT("widget_count"), Count);
		Result->SetNumberField(TEXT("canvas_width"), CanvasW);
		Result->SetNumberField(TEXT("canvas_height"), CanvasH);

		ResponseJson = MakeShareable(new FJsonObject);
		ResponseJson->SetBoolField(TEXT("success"), true);
		ResponseJson->SetObjectField(TEXT("result"), Result);
		DoneEvent->Trigger();
	});

	DoneEvent->Wait();
	FPlatformProcess::ReturnSynchEventToPool(DoneEvent);

	if (!ErrorMsg.IsEmpty())
	{
		ErrorMsg.ReplaceInline(TEXT("\""), TEXT("'"));
		return FString::Printf(TEXT("{\"success\":false,\"error\":\"%s\"}"), *ErrorMsg);
	}

	FString Out;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Out);
	FJsonSerializer::Serialize(ResponseJson.ToSharedRef(), Writer);
	return Out;
}

FString HandleRenderWidgetPreview(const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath = Params->HasField(TEXT("path")) ? Params->GetStringField(TEXT("path")) : TEXT("");
	if (AssetPath.IsEmpty())
	{
		return TEXT("{\"success\":false,\"error\":\"missing 'path' (e.g. /Game/UI/Menus/WBP_PauseMenu_FromHtml)\"}");
	}
	// Normalize to a fully-qualified class path: /Game/UI/.../WBP_X -> /Game/UI/.../WBP_X.WBP_X_C
	if (!AssetPath.Contains(TEXT(".")))
	{
		const FString ObjName = FPackageName::GetShortName(AssetPath);
		AssetPath = FString::Printf(TEXT("%s.%s_C"), *AssetPath, *ObjName);
	}

	const int32 Width = Params->HasField(TEXT("width")) ? (int32)Params->GetNumberField(TEXT("width")) : 1920;
	const int32 Height = Params->HasField(TEXT("height")) ? (int32)Params->GetNumberField(TEXT("height")) : 1080;
	const FString Filename = Params->HasField(TEXT("filename")) ? Params->GetStringField(TEXT("filename")) : TEXT("wbp_preview");

	FString Directory = Params->HasField(TEXT("directory"))
		? Params->GetStringField(TEXT("directory"))
		: FPaths::ScreenShotDir();
	FPaths::RemoveDuplicateSlashes(Directory);
	FPaths::NormalizeDirectoryName(Directory);
	if (!Directory.IsEmpty())
	{
		IFileManager::Get().MakeDirectory(*Directory, true);
	}
	const FString FullPath = Directory / (Filename + TEXT(".png"));
	const FString TreePath = Directory / (Filename + TEXT(".tree.json"));

	bool bOk = false;
	int32 WidgetCount = 0;
	FString ErrorMsg;
	TSharedPtr<FJsonObject> ResponseJson;

	FEvent* DoneEvent = FPlatformProcess::GetSynchEventFromPool();
	AsyncTask(ENamedThreads::GameThread, [&]()
	{
		// See HandleTakeScreenshot: re-seed FAppTime TLS so render-thread forks get a
		// valid time context (FWidgetRenderer::DrawWidget flushes rendering commands).
		FApp::SetCurrentTime(FApp::GetCurrentTime());

		UClass* WbpClass = LoadClass<UUserWidget>(nullptr, *AssetPath);
		if (!WbpClass)
		{
			ErrorMsg = FString::Printf(TEXT("widget blueprint class not found: %s"), *AssetPath);
			DoneEvent->Trigger();
			return;
		}

		// Prefer a real CreateWidget in the editor world; fall back to a transient
		// instance (FromHtml shells are pure layout, no GetWorld dependencies).
		UUserWidget* Preview = nullptr;
		if (GEditor)
		{
			if (UWorld* EditorWorld = GEditor->GetEditorWorldContext().World())
			{
				Preview = CreateWidget<UUserWidget>(EditorWorld, WbpClass);
			}
		}
		if (!Preview)
		{
			Preview = NewObject<UUserWidget>(GetTransientPackage(), WbpClass);
		}
		if (!Preview)
		{
			ErrorMsg = TEXT("failed to instantiate widget for preview");
			DoneEvent->Trigger();
			return;
		}

		if (Preview->WidgetTree)
		{
			// Count widgets and dump the design-time tree (name/type/text/visibility/
			// canvas layout) next to the PNG. Pixels can be unreadable (white-on-white
			// shells); the tree JSON is the machine-checkable contract source.
			Preview->WidgetTree->ForEachWidget([&WidgetCount](UWidget*) { ++WidgetCount; });

			TArray<TSharedPtr<FJsonValue>> TreeArr;
			Preview->WidgetTree->ForEachWidget([&TreeArr](UWidget* W)
			{
				if (!W)
				{
					return;
				}
				TSharedPtr<FJsonObject> WObj = MakeShareable(new FJsonObject);
				WObj->SetStringField(TEXT("name"), W->GetName());
				WObj->SetStringField(TEXT("type"), W->GetClass()->GetName());
				if (UWidget* Parent = W->GetParent())
				{
					WObj->SetStringField(TEXT("parent"), Parent->GetName());
				}
				if (const UTextBlock* TB = Cast<UTextBlock>(W))
				{
					WObj->SetStringField(TEXT("text"), TB->GetText().ToString());
				}
				WObj->SetStringField(TEXT("visibility"),
					StaticEnum<ESlateVisibility>()->GetNameStringByValue((int64)W->GetVisibility()));
				if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(W->Slot))
				{
					const FVector2D Pos = CanvasSlot->GetPosition();
					const FVector2D Size = CanvasSlot->GetSize();
					const FAnchors Anch = CanvasSlot->GetAnchors();
					TSharedPtr<FJsonObject> Layout = MakeShareable(new FJsonObject);
					Layout->SetNumberField(TEXT("x"), Pos.X);
					Layout->SetNumberField(TEXT("y"), Pos.Y);
					Layout->SetNumberField(TEXT("w"), Size.X);
					Layout->SetNumberField(TEXT("h"), Size.Y);
					Layout->SetNumberField(TEXT("anchor_min_x"), Anch.Minimum.X);
					Layout->SetNumberField(TEXT("anchor_min_y"), Anch.Minimum.Y);
					Layout->SetNumberField(TEXT("anchor_max_x"), Anch.Maximum.X);
					Layout->SetNumberField(TEXT("anchor_max_y"), Anch.Maximum.Y);
					Layout->SetBoolField(TEXT("auto_size"), CanvasSlot->GetAutoSize());
					WObj->SetObjectField(TEXT("canvas"), Layout);
				}
				TreeArr.Add(MakeShareable(new FJsonValueObject(WObj)));
			});

			TSharedPtr<FJsonObject> TreeRoot = MakeShareable(new FJsonObject);
			TreeRoot->SetStringField(TEXT("blueprint_path"), AssetPath);
			TreeRoot->SetNumberField(TEXT("width"), Width);
			TreeRoot->SetNumberField(TEXT("height"), Height);
			TreeRoot->SetArrayField(TEXT("widgets"), TreeArr);
			FString TreeOut;
			TSharedRef<TJsonWriter<>> TreeWriter = TJsonWriterFactory<>::Create(&TreeOut);
			FJsonSerializer::Serialize(TreeRoot.ToSharedRef(), TreeWriter);
			FFileHelper::SaveStringToFile(TreeOut, *TreePath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		}

		TSharedRef<SWidget> SlateWidget = Preview->TakeWidget();
		FWidgetRenderer Renderer(true);
		UTextureRenderTarget2D* RenderTarget = Renderer.DrawWidget(SlateWidget, FVector2D(Width, Height));
		if (!RenderTarget)
		{
			ErrorMsg = TEXT("FWidgetRenderer::DrawWidget returned null");
		}
		else
		{
			FlushRenderingCommands();
			FArchive* FileWriter = IFileManager::Get().CreateFileWriter(*FullPath);
			if (FileWriter)
			{
				bOk = FImageUtils::ExportRenderTarget2DAsPNG(RenderTarget, *FileWriter);
				FileWriter->Close();
				delete FileWriter;
				if (!bOk)
				{
					ErrorMsg = TEXT("ExportRenderTarget2DAsPNG failed");
				}
			}
			else
			{
				ErrorMsg = FString::Printf(TEXT("failed to create file writer for %s"), *FullPath);
			}
		}

		Preview->MarkAsGarbage();

		if (bOk)
		{
			FString JsonPath = FullPath;
			JsonPath.ReplaceInline(TEXT("\\"), TEXT("/"));
			TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
			Result->SetStringField(TEXT("blueprint_path"), AssetPath);
			Result->SetStringField(TEXT("file"), JsonPath);
			Result->SetNumberField(TEXT("width"), Width);
			Result->SetNumberField(TEXT("height"), Height);
			Result->SetNumberField(TEXT("widgets"), WidgetCount);

			ResponseJson = MakeShareable(new FJsonObject);
			ResponseJson->SetBoolField(TEXT("success"), true);
			ResponseJson->SetObjectField(TEXT("result"), Result);
		}
		DoneEvent->Trigger();
	});

	DoneEvent->Wait();
	FPlatformProcess::ReturnSynchEventToPool(DoneEvent);

	if (!ErrorMsg.IsEmpty())
	{
		ErrorMsg.ReplaceInline(TEXT("\""), TEXT("'"));
		return FString::Printf(TEXT("{\"success\":false,\"error\":\"%s\"}"), *ErrorMsg);
	}

	FString Out;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Out);
	FJsonSerializer::Serialize(ResponseJson.ToSharedRef(), Writer);
	return Out;
}

namespace TitleHostMotion
{
	static constexpr float LogoFadeInDuration = 0.85f;
	static constexpr float LogoBreathePeriod = 3.2f;
	static constexpr float LogoBreatheAmplitude = 0.06f;
	static constexpr float HintBreathePeriod = 1.6f;
	static constexpr float HintBreatheBase = 0.55f;
	static constexpr float HintBreatheAmplitude = 0.40f;

	static float LogoBreatheOpacityAtPhase(float Phase01)
	{
		const float Sin = FMath::Sin(Phase01 * 2.f * PI);
		return 1.f - LogoBreatheAmplitude * (0.5f + 0.5f * Sin);
	}

	static float HintBreatheOpacityAtPhase(float Phase01)
	{
		const float Sin = FMath::Sin(Phase01 * 2.f * PI);
		return HintBreatheBase + HintBreatheAmplitude * (0.5f + 0.5f * Sin);
	}

	static UWidgetAnimation* FindOrCreateOpacityAnimation(
		UWidgetBlueprint* WidgetBP,
		FName AnimName,
		FName TargetWidgetName,
		UClass* TargetWidgetClass,
		const TArray<TPair<float, float>>& TimeOpacityKeys,
		float DurationSeconds)
	{
		if (!WidgetBP || TargetWidgetName.IsNone() || !TargetWidgetClass || TimeOpacityKeys.Num() < 2 || DurationSeconds <= 0.f)
		{
			return nullptr;
		}

		UWidgetAnimation* Existing = nullptr;
		for (UWidgetAnimation* Anim : WidgetBP->Animations)
		{
			if (Anim && Anim->GetFName() == AnimName)
			{
				Existing = Anim;
				break;
			}
		}

		UWidgetAnimation* NewAnim = Existing;
		if (!NewAnim)
		{
			NewAnim = NewObject<UWidgetAnimation>(WidgetBP, AnimName, RF_Transactional);
			WidgetBP->Animations.Add(NewAnim);
			WidgetBP->OnVariableAdded(AnimName);
		}

		NewAnim->SetDisplayLabel(AnimName.ToString());
		NewAnim->AnimationBindings.Reset();

		UMovieScene* MovieScene = NewObject<UMovieScene>(NewAnim, AnimName, RF_Transactional);
		NewAnim->MovieScene = MovieScene;

		const FFrameRate TickResolution(60, 1);
		MovieScene->SetTickResolutionDirectly(TickResolution);
		MovieScene->SetDisplayRate(FFrameRate(60, 1));
		const FFrameNumber EndFrame = TickResolution.AsFrameNumber(DurationSeconds);
		MovieScene->SetPlaybackRange(TRange<FFrameNumber>(FFrameNumber(0), EndFrame));

		const FGuid BindingId = MovieScene->AddPossessable(TargetWidgetName.ToString(), TargetWidgetClass);

		FWidgetAnimationBinding AnimBinding;
		AnimBinding.WidgetName = TargetWidgetName;
		AnimBinding.SlotWidgetName = NAME_None;
		AnimBinding.AnimationGuid = BindingId;
		AnimBinding.bIsRootWidget = false;
		NewAnim->AnimationBindings.Add(AnimBinding);

		UMovieSceneFloatTrack* FloatTrack = MovieScene->AddTrack<UMovieSceneFloatTrack>(BindingId);
		if (!FloatTrack)
		{
			return nullptr;
		}
		FloatTrack->SetPropertyNameAndPath(TEXT("RenderOpacity"), TEXT("RenderOpacity"));

		UMovieSceneSection* NewSection = FloatTrack->CreateNewSection();
		UMovieSceneFloatSection* FloatSection = Cast<UMovieSceneFloatSection>(NewSection);
		if (!FloatSection)
		{
			return nullptr;
		}
		FloatSection->SetRange(TRange<FFrameNumber>(FFrameNumber(0), EndFrame));
		FloatTrack->AddSection(*FloatSection);

		FMovieSceneFloatChannel& Channel = FloatSection->GetChannel();
		Channel.Reset();
		for (const TPair<float, float>& Key : TimeOpacityKeys)
		{
			const FFrameNumber Frame = TickResolution.AsFrameNumber(Key.Key);
			Channel.AddCubicKey(Frame, Key.Value);
		}

		return NewAnim;
	}

	static UK2Node_CallFunction* AddPlayAnimationCall(
		UEdGraph* Graph,
		UFunction* PlayAnimFn,
		int32 PosX,
		int32 PosY,
		int32 NumLoops)
	{
		UK2Node_CallFunction* CallNode = NewObject<UK2Node_CallFunction>(Graph);
		CallNode->SetFromFunction(PlayAnimFn);
		CallNode->AllocateDefaultPins();
		CallNode->NodePosX = PosX;
		CallNode->NodePosY = PosY;
		CallNode->CreateNewGuid();
		Graph->AddNode(CallNode);

		if (UEdGraphPin* LoopsPin = CallNode->FindPin(TEXT("NumLoopsToPlay")))
		{
			const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();
			Schema->TrySetDefaultValue(*LoopsPin, FString::FromInt(NumLoops));
		}
		return CallNode;
	}

	static UK2Node_VariableGet* AddAnimVarGet(
		UEdGraph* Graph,
		UWidgetBlueprint* WidgetBP,
		FName AnimVarName,
		int32 PosX,
		int32 PosY)
	{
		UK2Node_VariableGet* GetNode = NewObject<UK2Node_VariableGet>(Graph);
		// SelfMember resolves against the hosting Widget BP animation variables.
		GetNode->VariableReference.SetSelfMember(AnimVarName);
		GetNode->AllocateDefaultPins();
		GetNode->NodePosX = PosX;
		GetNode->NodePosY = PosY;
		GetNode->CreateNewGuid();
		Graph->AddNode(GetNode);
		return GetNode;
	}

	static void ConnectPins(UEdGraphPin* A, UEdGraphPin* B)
	{
		if (!A || !B)
		{
			return;
		}
		const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();
		if (!Schema->TryCreateConnection(A, B))
		{
			A->MakeLinkTo(B);
		}
	}

	static UK2Node_Event* FindOrAddConstructEvent(UEdGraph* Graph)
	{
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (UK2Node_Event* EventNode = Cast<UK2Node_Event>(Node))
			{
				if (EventNode->GetFunctionName() == TEXT("Construct")
					&& EventNode->EventReference.GetMemberParentClass() == UUserWidget::StaticClass())
				{
					return EventNode;
				}
			}
		}

		UFunction* ConstructFn = UUserWidget::StaticClass()->FindFunctionByName(TEXT("Construct"));
		UK2Node_Event* ConstructEvent = NewObject<UK2Node_Event>(Graph);
		ConstructEvent->EventReference.SetExternalMember(ConstructFn->GetFName(), UUserWidget::StaticClass());
		ConstructEvent->bOverrideFunction = true;
		ConstructEvent->AllocateDefaultPins();
		ConstructEvent->NodePosX = 0;
		ConstructEvent->NodePosY = 0;
		ConstructEvent->CreateNewGuid();
		Graph->AddNode(ConstructEvent);
		return ConstructEvent;
	}

	static void WireTitleMotionGraph(
		UWidgetBlueprint* WidgetBP,
		UWidgetAnimation* IntroAnim,
		UWidgetAnimation* LogoBreatheAnim,
		UWidgetAnimation* HintBreatheAnim)
	{
		if (!WidgetBP || WidgetBP->UbergraphPages.Num() == 0)
		{
			return;
		}

		UEdGraph* Graph = WidgetBP->UbergraphPages[0];
		UFunction* PlayAnimFn = UUserWidget::StaticClass()->FindFunctionByName(TEXT("PlayAnimation"));
		if (!Graph || !PlayAnimFn)
		{
			return;
		}

		// Remove previously authored TitleHost motion nodes (tagged by NodeComment).
		TArray<UEdGraphNode*> ToRemove;
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (Node && Node->NodeComment == TEXT("TitleHostMotion"))
			{
				ToRemove.Add(Node);
			}
		}
		for (UEdGraphNode* Node : ToRemove)
		{
			Node->BreakAllNodeLinks();
			Graph->RemoveNode(Node);
		}

		auto Tag = [](UK2Node* Node)
		{
			Node->NodeComment = TEXT("TitleHostMotion");
			Node->bCommentBubbleVisible = false;
		};

		UK2Node_Event* ConstructEvent = FindOrAddConstructEvent(Graph);
		Tag(ConstructEvent);

		UK2Node_CallFunction* PlayHint = AddPlayAnimationCall(Graph, PlayAnimFn, 400, -120, 0);
		UK2Node_VariableGet* GetHint = AddAnimVarGet(Graph, WidgetBP, HintBreatheAnim->GetFName(), 200, -40);
		Tag(PlayHint);
		Tag(GetHint);
		ConnectPins(ConstructEvent->FindPin(UEdGraphSchema_K2::PN_Then), PlayHint->FindPin(UEdGraphSchema_K2::PN_Execute));
		{
			UEdGraphPin* AnimOut = GetHint->FindPin(HintBreatheAnim->GetFName());
			if (!AnimOut) { AnimOut = GetHint->GetValuePin(); }
			ConnectPins(AnimOut, PlayHint->FindPin(TEXT("InAnimation")));
		}

		UK2Node_CallFunction* PlayIntro = AddPlayAnimationCall(Graph, PlayAnimFn, 400, 160, 1);
		UK2Node_VariableGet* GetIntro = AddAnimVarGet(Graph, WidgetBP, IntroAnim->GetFName(), 200, 240);
		Tag(PlayIntro);
		Tag(GetIntro);
		ConnectPins(PlayHint->FindPin(UEdGraphSchema_K2::PN_Then), PlayIntro->FindPin(UEdGraphSchema_K2::PN_Execute));
		{
			UEdGraphPin* AnimOut = GetIntro->FindPin(IntroAnim->GetFName());
			if (!AnimOut) { AnimOut = GetIntro->GetValuePin(); }
			ConnectPins(AnimOut, PlayIntro->FindPin(TEXT("InAnimation")));
		}

		UK2Node_WidgetAnimationEvent* IntroFinished = NewObject<UK2Node_WidgetAnimationEvent>(Graph);
		IntroFinished->SourceWidgetBlueprint = WidgetBP;
		IntroFinished->AnimationPropertyName = IntroAnim->GetMovieScene()->GetFName();
		IntroFinished->Action = EWidgetAnimationEvent::Finished;
		IntroFinished->CustomFunctionName = FName(*FString::Printf(
			TEXT("WidgetAnimationEvt_%s_%s"),
			*IntroFinished->AnimationPropertyName.ToString(),
			*IntroFinished->GetName()));
		IntroFinished->AllocateDefaultPins();
		IntroFinished->NodePosX = 0;
		IntroFinished->NodePosY = 420;
		IntroFinished->CreateNewGuid();
		Graph->AddNode(IntroFinished);
		Tag(IntroFinished);

		UK2Node_CallFunction* PlayBreathe = AddPlayAnimationCall(Graph, PlayAnimFn, 400, 420, 0);
		UK2Node_VariableGet* GetBreathe = AddAnimVarGet(Graph, WidgetBP, LogoBreatheAnim->GetFName(), 200, 500);
		Tag(PlayBreathe);
		Tag(GetBreathe);
		ConnectPins(IntroFinished->FindPin(UEdGraphSchema_K2::PN_Then), PlayBreathe->FindPin(UEdGraphSchema_K2::PN_Execute));
		{
			UEdGraphPin* AnimOut = GetBreathe->FindPin(LogoBreatheAnim->GetFName());
			if (!AnimOut) { AnimOut = GetBreathe->GetValuePin(); }
			ConnectPins(AnimOut, PlayBreathe->FindPin(TEXT("InAnimation")));
		}
	}
}

FString HandleAuthorTitleScreenHostMotion(const TSharedPtr<FJsonObject>& Params)
{
	const FString AssetPath = Params.IsValid() && Params->HasField(TEXT("path"))
		? Params->GetStringField(TEXT("path"))
		: TEXT("/Game/UI/Widgets/WBP_TitleScreen");

	FString ErrorMsg;
	TSharedPtr<FJsonObject> ResponseJson;
	FEvent* DoneEvent = FPlatformProcess::GetSynchEventFromPool();

	AsyncTask(ENamedThreads::GameThread, [&]()
	{
		UWidgetBlueprint* WidgetBP = LoadObject<UWidgetBlueprint>(nullptr, *AssetPath);
		if (!WidgetBP)
		{
			// Create host if missing.
			UWidgetBlueprintFactory* Factory = NewObject<UWidgetBlueprintFactory>();
			if (UClass* Parent = LoadClass<UUserWidget>(nullptr, TEXT("/Script/ClanSimulator.TitleScreenWidget")))
			{
				Factory->ParentClass = Parent;
			}
			IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
			UObject* NewAsset = AssetTools.CreateAsset(
				TEXT("WBP_TitleScreen"),
				TEXT("/Game/UI/Widgets"),
				UWidgetBlueprint::StaticClass(),
				Factory);
			WidgetBP = Cast<UWidgetBlueprint>(NewAsset);
		}

		if (!WidgetBP || !WidgetBP->WidgetTree)
		{
			ErrorMsg = TEXT("WBP_TitleScreen missing or has no WidgetTree");
			DoneEvent->Trigger();
			return;
		}

		if (WidgetBP->ParentClass && WidgetBP->ParentClass->GetName().Contains(TEXT("FromHtml")))
		{
			ErrorMsg = TEXT("ANTI-PATTERN: host must NOT inherit FromHtml");
			DoneEvent->Trigger();
			return;
		}

		WidgetBP->Modify();
		UWidgetTree* Tree = WidgetBP->WidgetTree;
		Tree->Modify();

		UCanvasPanel* RootCanvas = Cast<UCanvasPanel>(Tree->RootWidget);
		if (!RootCanvas)
		{
			RootCanvas = Tree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("RootCanvas"));
			Tree->RootWidget = RootCanvas;
		}
		RootCanvas->SetVisibility(ESlateVisibility::SelfHitTestInvisible);

		// Shell widgets: BindWidgetOptional on C++ host — mark bIsVariable and register Guid map
		// so compile does not Ensure-fail (raw ConstructWidget alone is insufficient).
		auto EnsureNamedChild = [&](FName WidgetName, UClass* Class, TFunctionRef<void(UWidget*)> Configure) -> UWidget*
		{
			UWidget* Existing = Tree->FindWidget(WidgetName);
			if (!Existing)
			{
				Existing = Tree->ConstructWidget<UWidget>(Class, WidgetName);
				if (UCanvasPanelSlot* Slot = RootCanvas->AddChildToCanvas(Existing))
				{
					if (WidgetName == TEXT("HtmlSlot"))
					{
						Slot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
						Slot->SetOffsets(FMargin(0.f));
						Slot->SetZOrder(0);
					}
					else if (WidgetName == TEXT("TitleLogo"))
					{
						Slot->SetAnchors(FAnchors(0.5f, 0.5f, 0.5f, 0.5f));
						Slot->SetAlignment(FVector2D(0.5f, 0.5f));
						Slot->SetPosition(FVector2D(0.f, -130.f));
						Slot->SetSize(FVector2D(560.f, 740.f));
						Slot->SetAutoSize(false);
						Slot->SetZOrder(10);
					}
					else if (WidgetName == TEXT("HintText"))
					{
						Slot->SetAnchors(FAnchors(0.5f, 0.5f, 0.5f, 0.5f));
						Slot->SetAlignment(FVector2D(0.5f, 0.5f));
						Slot->SetPosition(FVector2D(0.f, 332.f));
						Slot->SetSize(FVector2D(400.f, 40.f));
						Slot->SetAutoSize(false);
						Slot->SetZOrder(11);
					}
				}
			}
			Existing->bIsVariable = true;
			if (!WidgetBP->WidgetVariableNameToGuidMap.Contains(WidgetName))
			{
				WidgetBP->WidgetVariableNameToGuidMap.Add(WidgetName, FGuid::NewGuid());
			}
			Configure(Existing);
			return Existing;
		};

		EnsureNamedChild(TEXT("HtmlSlot"), UNamedSlot::StaticClass(), [](UWidget* W)
		{
			W->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		});
		EnsureNamedChild(TEXT("TitleLogo"), UImage::StaticClass(), [](UWidget* W)
		{
			W->SetVisibility(ESlateVisibility::HitTestInvisible);
			W->SetRenderOpacity(0.f);
		});
		EnsureNamedChild(TEXT("HintText"), UTextBlock::StaticClass(), [](UWidget* W)
		{
			W->SetVisibility(ESlateVisibility::HitTestInvisible);
			W->SetRenderOpacity(TitleHostMotion::HintBreatheBase);
			if (UTextBlock* TB = Cast<UTextBlock>(W))
			{
				TB->SetText(FText::FromString(TEXT("按任意键继续")));
			}
		});

		// Drop orphan animations not in Animations array (e.g. prior Python NewObject).
		TArray<UObject*> ChildAnims;
		GetObjectsWithOuter(WidgetBP, ChildAnims, EGetObjectsFlags::None);
		for (UObject* Obj : ChildAnims)
		{
			if (UWidgetAnimation* Anim = Cast<UWidgetAnimation>(Obj))
			{
				if (!WidgetBP->Animations.Contains(Anim))
				{
					Anim->Rename(nullptr, GetTransientPackage(), REN_DontCreateRedirectors);
				}
			}
		}

		TArray<TPair<float, float>> IntroKeys;
		IntroKeys.Add(TPair<float, float>(0.f, 0.f));
		IntroKeys.Add(TPair<float, float>(TitleHostMotion::LogoFadeInDuration * 0.5f, 0.5f));
		IntroKeys.Add(TPair<float, float>(TitleHostMotion::LogoFadeInDuration, 1.f));

		TArray<TPair<float, float>> LogoBreatheKeys;
		for (int32 i = 0; i <= 4; ++i)
		{
			const float T = TitleHostMotion::LogoBreathePeriod * (static_cast<float>(i) / 4.f);
			LogoBreatheKeys.Add(TPair<float, float>(
				T, TitleHostMotion::LogoBreatheOpacityAtPhase(static_cast<float>(i) / 4.f)));
		}

		TArray<TPair<float, float>> HintKeys;
		for (int32 i = 0; i <= 4; ++i)
		{
			const float T = TitleHostMotion::HintBreathePeriod * (static_cast<float>(i) / 4.f);
			HintKeys.Add(TPair<float, float>(
				T, TitleHostMotion::HintBreatheOpacityAtPhase(static_cast<float>(i) / 4.f)));
		}

		UWidgetAnimation* Intro = TitleHostMotion::FindOrCreateOpacityAnimation(
			WidgetBP, TEXT("Anim_TitleLogoIntro"), TEXT("TitleLogo"), UImage::StaticClass(),
			IntroKeys, TitleHostMotion::LogoFadeInDuration);
		UWidgetAnimation* LogoBreathe = TitleHostMotion::FindOrCreateOpacityAnimation(
			WidgetBP, TEXT("Anim_TitleLogoBreathe"), TEXT("TitleLogo"), UImage::StaticClass(),
			LogoBreatheKeys, TitleHostMotion::LogoBreathePeriod);
		UWidgetAnimation* HintBreathe = TitleHostMotion::FindOrCreateOpacityAnimation(
			WidgetBP, TEXT("Anim_HintBreathe"), TEXT("HintText"), UTextBlock::StaticClass(),
			HintKeys, TitleHostMotion::HintBreathePeriod);

		if (!Intro || !LogoBreathe || !HintBreathe)
		{
			ErrorMsg = TEXT("Failed to author one or more Widget Animations");
			DoneEvent->Trigger();
			return;
		}

		TitleHostMotion::WireTitleMotionGraph(WidgetBP, Intro, LogoBreathe, HintBreathe);

		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBP);
		FKismetEditorUtilities::CompileBlueprint(WidgetBP);
		if (WidgetBP->Status == BS_Error)
		{
			ErrorMsg = TEXT("WBP_TitleScreen compile failed after authoring");
			DoneEvent->Trigger();
			return;
		}

		const FString PackageFilename = FPackageName::LongPackageNameToFilename(
			WidgetBP->GetOutermost()->GetName(), FPackageName::GetAssetPackageExtension());
		FSavePackageArgs SaveArgs;
		SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
		UPackage::SavePackage(WidgetBP->GetOutermost(), WidgetBP, *PackageFilename, SaveArgs);

		TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
		Result->SetStringField(TEXT("path"), AssetPath);
		Result->SetBoolField(TEXT("intro"), true);
		Result->SetBoolField(TEXT("logo_breathe"), true);
		Result->SetBoolField(TEXT("hint_breathe"), true);
		Result->SetStringField(TEXT("play_graph"), TEXT("Construct->Hint loop + Intro; Intro Finished->Logo Breathe loop"));

		ResponseJson = MakeShareable(new FJsonObject);
		ResponseJson->SetBoolField(TEXT("success"), true);
		ResponseJson->SetObjectField(TEXT("result"), Result);
		DoneEvent->Trigger();
	});

	DoneEvent->Wait();
	FPlatformProcess::ReturnSynchEventToPool(DoneEvent);

	if (!ErrorMsg.IsEmpty())
	{
		ErrorMsg.ReplaceInline(TEXT("\""), TEXT("'"));
		return FString::Printf(TEXT("{\"success\":false,\"error\":\"%s\"}"), *ErrorMsg);
	}

	FString Out;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Out);
	FJsonSerializer::Serialize(ResponseJson.ToSharedRef(), Writer);
	return Out;
}
#endif
