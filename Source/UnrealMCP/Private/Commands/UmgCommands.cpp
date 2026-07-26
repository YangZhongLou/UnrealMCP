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
#include "Components/VerticalBox.h"
#include "Components/HorizontalBox.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Components/Border.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/HorizontalBoxSlot.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "UObject/SavePackage.h"
#include "Misc/App.h"
#include "Misc/PackageName.h"
#include "EditorAssetLibrary.h"

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
		if (Cast<UButton>(Widget))
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
		}
		if (UClass* const* Found = Map.Find(TypeName))
		{
			return *Found;
		}
		return UTextBlock::StaticClass();
	}

	static void ApplyStyle(UWidget* Widget, const TSharedPtr<FJsonObject>& Style)
	{
		if (!Widget || !Style.IsValid())
		{
			return;
		}
		FString ColorHex;
		if (Style->TryGetStringField(TEXT("background_color"), ColorHex))
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
#endif
