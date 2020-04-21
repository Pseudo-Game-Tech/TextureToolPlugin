#include "STextureToolUI.h"
#include "SlateOptMacros.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Views/SListView.h"
#include "Engine/Texture2D.h"
#include "TextureUtils.h"
#include "Editor.h"
#include "LevelEditor.h"
#include "Engine/Selection.h"
#include "ObjectTools.h"
#include "Misc/MessageDialog.h"
#include "AssetRegistryModule.h"
#include "ContentBrowserModule.h"
#include "IContentBrowserSingleton.h"
#include "NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Misc/ScopedSlowTask.h"
#include "ReferencedAssetsUtils.h"
#include "Engine/LevelStreaming.h"
#include "EngineUtils.h"
#include "AssetThumbnail.h"
#include "MultiBoxBuilder.h"
#include "Widgets/Views/STableViewBase.h"
#include "Widgets/Views/STableRow.h"
#include "EditorStyle.h"
#include "PropertyEditorModule.h"
#include "IDetailsView.h"
#include "SettingObjects.h"
#include "IDetailCustomization.h"
#include "IPropertyTypeCustomization.h"
#include "IDetailRootObjectCustomization.h"
#include "Widgets/Layout/SBox.h"
#define LOCTEXT_NAMESPACE "TextureToolUI"

void STextureToolUI::Construct(const FArguments& InArgs)
{
	AssetThumbnailPool = MakeShareable(new FAssetThumbnailPool(24));
	CreateDetailView();
	Mode = EToolMode::TextureFinder;
	ChildSlot.Padding(FMargin(8))
	[
		SNew(SVerticalBox)
		/** Toolbar containing buttons to switch between different paint modes */
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(SBorder).BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder")).HAlign(HAlign_Center)
			[
				CreateToolBarWidget()->AsShared()
			]
		]
		+ SVerticalBox::Slot().FillHeight(1.0f).Padding(0.f, 5.f, 0.f, 0.f)
		[
			SAssignNew(InlineContentHolder, SBox)
		]
	];
	FinderWidget = CreateFinderWidget()->AsShared();
	MergerWidget = CreateMergerWidget()->AsShared();
	InlineContentHolder->SetContent(FinderWidget->AsShared());
}

void STextureToolUI::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	AssetThumbnailPool->Tick(InDeltaTime);
}

static bool IsAContentBrowserAsset(UObject* Object, FString& OutFailureReason)
{
	if (Object == nullptr || Object->IsPendingKill())
	{
		OutFailureReason = TEXT("The Asset is not valid.");
		return false;
	}

	if (Cast<UField>(Object))
	{
		OutFailureReason = FString::Printf(TEXT("The object is of the base class type '%s'"), *Object->GetName());
		return false;
	}

	if (!ObjectTools::IsObjectBrowsable(Object))
	{
		OutFailureReason = FString::Printf(TEXT("The object %s is not an asset."), *Object->GetName());
		return false;
	}

	UPackage* NewPackage = Object->GetOutermost();

	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	FAssetData AssetData = AssetRegistryModule.Get().GetAssetByObjectPath(NewPackage->GetFName());
	if (!AssetData.IsValid())
	{
		OutFailureReason = FString::Printf(TEXT("The AssetData '%s' could not be found in the Content Browser."), *NewPackage->GetName());
		return false;
	}

	return true;
}

void STextureToolUI::UpdateTextureListItems()
{
	TArray<UObject*> SelectedActors;
	GEditor->GetSelectedActors()->GetSelectedObjects(AActor::StaticClass(), /*out*/ SelectedActors);
	TArray<UObject*> Textures;
	for (auto Object : SelectedActors)
	{
		auto Actor = Cast<AActor>(Object);
		auto FoundTextures = FTextureToolUtils::FindTextures(Actor);
		for (auto Texture : FoundTextures)
		{
			FString FailureReason;
			if (!IsAContentBrowserAsset(Texture, FailureReason))
			{
				UE_LOG(LogTemp, Warning, TEXT("Find runtime texture %s, Size = (%d, %d)."), *FailureReason, Texture->GetSizeX(), Texture->GetSizeY());
			}
			else
			{
				Textures.AddUnique(Texture);
				UE_LOG(LogTemp, Warning, TEXT("Find texture asset %s, Size = (%d, %d)"), *Texture->GetPathName(), Texture->GetSizeX(), Texture->GetSizeY());
			}
		}
	}
	TextureListItems.Reset();
	for (auto Texture : Textures)
	{
		TSharedPtr<FTextureListItem> Item = MakeShared<FTextureListItem>();
		Item->Texture = Texture;
		TextureListItems.Add(Item);
	}
	if (TextureListView.IsValid())
		TextureListView->RequestListRefresh();
}

TSharedPtr<SWidget> STextureToolUI::CreateToolBarWidget()
{
	FToolBarBuilder ModeSwitchButtons(MakeShareable(new FUICommandList()), FMultiBoxCustomization::None);
	{
		FSlateIcon ColorPaintIcon(FEditorStyle::GetStyleSetName(), "LevelEditor.MeshPaintMode.ColorPaint");
		ModeSwitchButtons.AddToolBarButton(FUIAction(FExecuteAction::CreateLambda([=]()
			{
				InlineContentHolder->SetContent(FinderWidget->AsShared());
				Mode = EToolMode::TextureFinder;
			}), FCanExecuteAction(), FIsActionChecked::CreateLambda([=]() -> bool { return Mode == EToolMode::TextureFinder; })), NAME_None, LOCTEXT("Mode.TextureFinding", "Find"), LOCTEXT("Mode.TextureFinding.Tooltip", "Texture finding mode allows find and downscale textures"), ColorPaintIcon, EUserInterfaceActionType::ToggleButton);

		FSlateIcon WeightPaintIcon(FEditorStyle::GetStyleSetName(), "LevelEditor.MeshPaintMode.WeightPaint");
		ModeSwitchButtons.AddToolBarButton(FUIAction(FExecuteAction::CreateLambda([=]()
			{
				InlineContentHolder->SetContent(MergerWidget->AsShared());
				SettingsDetailsView->SetObject(UTextureMergeSettings::Get());
				Mode = EToolMode::TextureMerger;
			}), FCanExecuteAction(), FIsActionChecked::CreateLambda([=]() -> bool { return Mode == EToolMode::TextureMerger; })), NAME_None, LOCTEXT("Mode.TextureMerging", "Merge"), LOCTEXT("Mode.TextureMerging.Tooltip", "Texture merging mode allows merge texture channels"), WeightPaintIcon, EUserInterfaceActionType::ToggleButton);
	}

	return ModeSwitchButtons.MakeWidget();
}

TSharedPtr<SWidget> STextureToolUI::CreateFinderWidget()
{
	return SNew(SVerticalBox)
	+ SVerticalBox::Slot().FillHeight(1.f)
	[
		SNew(SBorder).BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
		[
			SAssignNew(TextureListView, STextureListView)
			.ItemHeight(96)
			.ListItemsSource(&TextureListItems)
			.OnGenerateRow(this, &STextureToolUI::OnGenerateWidgetForTextureListView)
			.OnContextMenuOpening(this, &STextureToolUI::CreateTextureContextMenu)
			.OnMouseButtonDoubleClick(this, &STextureToolUI::OpenTextureEditor)
			.HeaderRow
			(
				SNew(SHeaderRow)
				+ SHeaderRow::Column("TextureName").VAlignCell(VAlign_Center)
				.DefaultLabel(LOCTEXT("TextureName", "Texture"))
				.FillWidth(400)
				+SHeaderRow::Column("TextureSize").VAlignCell(VAlign_Center)
				.DefaultLabel(LOCTEXT("TextureSize", "Size"))
				.FillWidth(200) //TODO Sort
				+ SHeaderRow::Column("TextureSourceSize").VAlignCell(VAlign_Center)
				.DefaultLabel(LOCTEXT("TextureSourceSize", "SourceSize"))
				.FillWidth(200)
			)
		]
	]
	+ SVerticalBox::Slot().AutoHeight().Padding(0.f,10.f,0.f,0.f)
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().FillWidth(1.f)
		+ SHorizontalBox::Slot().FillWidth(0.15f)
		[
			SNew(SBox)
			[
				SNew(SButton)
				.Text(LOCTEXT("FindTexture", "Find Texture"))
				.HAlign(HAlign_Center)
				.ToolTipText(LOCTEXT("FindTextureTip", "Find textures used by selected actors"))
				.OnClicked(this, &STextureToolUI::OnFindTextureClicked)
			]
		]
	];
}

TSharedPtr<SWidget> STextureToolUI::CreateMergerWidget()
{
	return SNew(SVerticalBox)
	+SVerticalBox::Slot().FillHeight(1.f)
	[
		SNew(SScrollBox)
		+ SScrollBox::Slot()
		[
			SettingsDetailsView->AsShared()
		]
	]
	+SVerticalBox::Slot().AutoHeight().Padding(0.f,10.f,0.f,0.f)
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().FillWidth(1.f)
		+ SHorizontalBox::Slot().FillWidth(0.2f)
		[
			SNew(SButton).HAlign(HAlign_Center)
			.IsEnabled(this, &STextureToolUI::CanAutoKeyword)
			.Text(LOCTEXT("AutoKeyword", "Auto Keyword"))
			.ToolTipText(LOCTEXT("AutoKeywordTip", "Automatic fill Keyword"))
			.OnClicked(this, &STextureToolUI::OnAutoSuffixClicked)
			
		]	
		+ SHorizontalBox::Slot().FillWidth(0.2f).Padding(10.f, 0)
		[
			SNew(SButton).HAlign(HAlign_Center)
			.IsEnabled(this, &STextureToolUI::CanBatch)
			.Text(LOCTEXT("BatchMergeTextures", "Batch"))
			.ToolTipText(LOCTEXT("MergeTexturesTip", "Remap textures channels to new texture"))
			.OnClicked(this, &STextureToolUI::OnBatchClicked)
		]
		+ SHorizontalBox::Slot().FillWidth(0.2f)
		[
			SNew(SButton).HAlign(HAlign_Center)
			.IsEnabled(this, &STextureToolUI::CanMerge)
			.Text(LOCTEXT("MergeTextures", "Merge"))
			.ToolTipText(LOCTEXT("MergeTexturesTip", "Remap textures channels to new texture"))
			.OnClicked(this, &STextureToolUI::OnMergeClicked)
		]
	];
}

TSharedPtr<SWidget> STextureToolUI::CreateTextureContextMenu()
{
	FMenuBuilder MenuBuilder(true, NULL);
	MenuBuilder.BeginSection("FindAction", LOCTEXT("FindAction", "Find"));
	{
		FUIAction Action = FUIAction(FExecuteAction::CreateSP(this, &STextureToolUI::OnFindActorClicked));
		const FText Label = LOCTEXT("FindActorButtonLabel", "Find Actor");
		const FText ToolTipText = LOCTEXT("FindActorButtonTooltip", "Find actors using selected textures");
		MenuBuilder.AddMenuEntry(Label, ToolTipText, FSlateIcon(), Action);
	}

	{
		FUIAction Action = FUIAction(FExecuteAction::CreateSP(this, &STextureToolUI::OnBrowseToClicked));
		const FText Label = LOCTEXT("BrowseToButtonLabel", "Browse To");
		const FText ToolTipText = LOCTEXT("BrowseToButtonTooltip", "Browse to selected textures");
		MenuBuilder.AddMenuEntry(Label, ToolTipText, FSlateIcon(), Action);
	}
	MenuBuilder.EndSection();
	MenuBuilder.BeginSection("ModifyAction", LOCTEXT("ModifyAction", "Modify"));
	{
		FUIAction Action = FUIAction(FExecuteAction::CreateSP(this, &STextureToolUI::OnDownScaleClicked));
		const FText Label = LOCTEXT("DownScaleButtonLabel", "DownScale Size");
		const FText ToolTipText = LOCTEXT("DownScaleButtonTooltip", "DownScale selected textures by two");
		MenuBuilder.AddMenuEntry(Label, ToolTipText, FSlateIcon(), Action);
	}

	{
		FUIAction Action = FUIAction(FExecuteAction::CreateSP(this, &STextureToolUI::OnResetSizeClicked));
		const FText Label = LOCTEXT("ResetSizeButtonLabel", "Reset Size");
		const FText ToolTipText = LOCTEXT("ResetSizeButtonTooltip", "Reset selected textures' size to max");
		MenuBuilder.AddMenuEntry(Label, ToolTipText, FSlateIcon(), Action);
	}
	MenuBuilder.EndSection();
	return MenuBuilder.MakeWidget();
}

class FTextureToolSettingsRootObjectCustomization : public IDetailRootObjectCustomization
{
public:
	/** IDetailRootObjectCustomization interface */
	virtual TSharedPtr<SWidget> CustomizeObjectHeader(const UObject* InRootObject) override {
		return SNullWidget::NullWidget;
	}
	virtual bool IsObjectVisible(const UObject* InRootObject) const override { return true; }
	virtual bool ShouldDisplayHeader(const UObject* InRootObject) const override { return false; }
};

void STextureToolUI::CreateDetailView()
{
	FPropertyEditorModule& EditModule = FModuleManager::Get().GetModuleChecked<FPropertyEditorModule>("PropertyEditor");

	FDetailsViewArgs DetailsViewArgs(
		/*bUpdateFromSelection=*/ false,
		/*bLockable=*/ false,
		/*bAllowSearch=*/ false,
		FDetailsViewArgs::HideNameArea,
		/*bHideSelectionTip=*/ true,
		/*InNotifyHook=*/ nullptr,
		/*InSearchInitialKeyFocus=*/ false,
		/*InViewIdentifier=*/ NAME_None);
	DetailsViewArgs.DefaultsOnlyVisibility = EEditDefaultsOnlyNodeVisibility::Automatic;
	DetailsViewArgs.bShowOptions = false;
	DetailsViewArgs.bAllowMultipleTopLevelObjects = true;

	SettingsDetailsView = EditModule.CreateDetailView(DetailsViewArgs);
	SettingsDetailsView->SetRootObjectCustomizationInstance(MakeShareable(new FTextureToolSettingsRootObjectCustomization));
	SettingsDetailsView->SetObject(UTextureMergeSettings::Get());
}

const int32 ThumbnailSize = 72;

TSharedRef<ITableRow> STextureToolUI::OnGenerateWidgetForTextureListView(TSharedPtr<FTextureListItem> InItem, const TSharedRef<STableViewBase>& OwnerTable)
{
	class STextureItemWidget : public SMultiColumnTableRow<TSharedPtr<FTextureListItem>>
	{
	public:
		SLATE_BEGIN_ARGS(STextureItemWidget) {}
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTable, TSharedPtr<FTextureListItem> InListItem, TSharedPtr<FAssetThumbnailPool> Pool)
		{
			Item = InListItem;
			AssetThumbnailPool = Pool;
			SMultiColumnTableRow<TSharedPtr<FTextureListItem>>::Construct(FSuperRowType::FArguments(), InOwnerTable);
		}

	private:
		TSharedRef<SWidget> GenerateWidgetForColumn(const FName& ColumnName)
		{
			if (ColumnName == "TextureName")
			{
				TSharedRef<FAssetThumbnail> AssetThumbnail = MakeShared<FAssetThumbnail>(Item->Texture.Get(), ThumbnailSize, ThumbnailSize, AssetThumbnailPool);
				FAssetThumbnailConfig ThumbnailConfig;
				ThumbnailConfig.bAllowFadeIn = false;
				return SNew(SHorizontalBox) 
					+ SHorizontalBox::Slot().AutoWidth().VAlign(EVerticalAlignment::VAlign_Center)
					[
						SNew(SBox)
						.WidthOverride(ThumbnailSize)
						.HeightOverride(ThumbnailSize)
						[
							AssetThumbnail->MakeThumbnailWidget(ThumbnailConfig)
						]
					]
					+ SHorizontalBox::Slot().AutoWidth().Padding(10.f, 0.f).VAlign(EVerticalAlignment::VAlign_Center)
					[
						SNew(STextBlock)
						.Text(FText::FromString(Item->Texture->GetPathName()))
					];
			}
			else if (ColumnName == "TextureSize")
			{
				return SNew(STextBlock)
					.Text_Lambda([=]() 
						{
							static auto Format = FTextFormat::FromString("{0} x {1}");
							return FText::Format(Format, Item->Texture->GetSizeX(), Item->Texture->GetSizeY());
						});
			}
			else if (ColumnName == "TextureSourceSize")
			{
				static auto Format = FTextFormat::FromString("{0} x {1}");
				auto Text = FText::Format(Format, Item->Texture->Source.GetSizeX(), Item->Texture->Source.GetSizeY());
				return
					SNew(STextBlock)
					.Text(Text);
			}
			else
			{
				return SNew(STextBlock).Text(LOCTEXT("UnknownColumn", "Unknown Column"));
			}
		}
		TSharedPtr<FTextureListItem> Item;
		TSharedPtr<FAssetThumbnailPool> AssetThumbnailPool;
	};

	return SNew(STextureItemWidget, OwnerTable, InItem, AssetThumbnailPool);
}

bool STextureToolUI::HasSelectedTexture() const
{
	return TextureListView->GetNumItemsSelected() > 0;
}

bool STextureToolUI::HasSelectedActor() const
{
	return GEditor->GetSelectedActors()->Num() > 0;
}

bool STextureToolUI::CanMerge() const
{
	auto Setting = UTextureMergeSettings::Get();
	return Setting->CanMerge();
}

bool STextureToolUI::CanBatch() const
{
	auto Setting = UTextureMergeSettings::Get();
	return Setting->CanBatch();
}

bool STextureToolUI::CanAutoKeyword() const
{
	auto Setting = UTextureMergeSettings::Get();
	return Setting->CanAutoKeyword();
}

void STextureToolUI::OnDownScaleClicked()
{
	if (TextureListView->GetNumItemsSelected() > 0)
	{
		FTextureItemArray Array;
		TextureListView->GetSelectedItems(Array);
		TArray<UTexture2D*> TexturesNotAllowed;
		for (auto& Item : Array)
		{
			if (!FTextureToolUtils::CanDownScaleTexture(Item->Texture.Get()))
				TexturesNotAllowed.Add(Item->Texture.Get());
		}

		if (TexturesNotAllowed.Num() != 0)
		{
			FString Names;
			for (UTexture2D* Texture : TexturesNotAllowed)
			{
				Names.Append(Texture->GetName());
				Names.AppendChar('\n');
			}
			auto ErrorText = FText::Format(FTextFormat::FromString("Maximum Texture Size cannot be changed for this texture as it is a non power of two size. Change the Power of Two Mode to allow it to be padded to a power of two.\n {0}"), FText::FromString(Names));
			FMessageDialog::Open(EAppMsgType::Ok, ErrorText);
		}
		else
		{
			for (auto& Item : Array)
			{
				FTextureToolUtils::DownScaleTexture(Item->Texture.Get());
			}
		}
		TextureListView->RequestListRefresh();
	}
}

void STextureToolUI::OnResetSizeClicked()
{
	if (TextureListView->GetNumItemsSelected() > 0)
	{
		FTextureItemArray Array;
		TextureListView->GetSelectedItems(Array);
		for (auto& Item : Array)
		{
			FTextureToolUtils::ResetTextureSize(Item->Texture.Get());
		}
		TextureListView->RequestListRefresh();
	}
}

void STextureToolUI::OnBrowseToClicked()
{
	if (TextureListView->GetNumItemsSelected() > 0)
	{
		FTextureItemArray Array;
		TextureListView->GetSelectedItems(Array);
		TArray<UObject*> Textures;
		for (auto& Item : Array)
			Textures.Add(Item->Texture.Get());
		FContentBrowserModule& ContentBrowserModule = FModuleManager::Get().LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
		ContentBrowserModule.Get().SyncBrowserToAssets(Textures, false, true);
	}

}


/** Generates a reference graph of the world and can then find actors referencing specified objects */
struct WorldReferenceGenerator : public FFindReferencedAssets
{
	void BuildReferencingData()
	{
		MarkAllObjects();

		const int32 MaxRecursionDepth = 0;
		const bool bIncludeClasses = true;
		const bool bIncludeDefaults = false;
		const bool bReverseReferenceGraph = true;


		UWorld* World = GWorld;

		// Generate the reference graph for the world
		FReferencedAssets* WorldReferencer = new(Referencers)FReferencedAssets(World);
		FFindAssetsArchive(World, WorldReferencer->AssetList, &ReferenceGraph, MaxRecursionDepth, bIncludeClasses, bIncludeDefaults, bReverseReferenceGraph);

		// Also include all the streaming levels in the results
		for (ULevelStreaming* StreamingLevel : World->GetStreamingLevels())
		{
			if (StreamingLevel)
			{
				if (ULevel* Level = StreamingLevel->GetLoadedLevel())
				{
					// Generate the reference graph for each streamed in level
					FReferencedAssets* LevelReferencer = new(Referencers) FReferencedAssets(Level);
					FFindAssetsArchive(Level, LevelReferencer->AssetList, &ReferenceGraph, MaxRecursionDepth, bIncludeClasses, bIncludeDefaults, bReverseReferenceGraph);
				}
			}
		}

		TArray<UObject*> ReferencedObjects;
		// Special case for blueprints
		for (AActor* Actor : FActorRange(World))
		{
			ReferencedObjects.Reset();
			Actor->GetReferencedContentObjects(ReferencedObjects);
			for (UObject* Reference : ReferencedObjects)
			{
				TSet<UObject*>& Objects = ReferenceGraph.FindOrAdd(Reference);
				Objects.Add(Actor);
			}
		}
	}

	void MarkAllObjects()
	{
		// Mark all objects so we don't get into an endless recursion
		for (FObjectIterator It; It; ++It)
		{
			It->Mark(OBJECTMARK_TagExp);
		}
	}

	void Generate(const UObject* AssetToFind, TSet<const UObject*>& OutObjects)
	{
		// Don't examine visited objects
		if (!AssetToFind->HasAnyMarks(OBJECTMARK_TagExp))
		{
			return;
		}

		AssetToFind->UnMark(OBJECTMARK_TagExp);

		// Return once we find a parent object that is an actor
		if (AssetToFind->IsA(AActor::StaticClass()))
		{
			OutObjects.Add(AssetToFind);
			return;
		}

		// Traverse the reference graph looking for actor objects
		TSet<UObject*>* ReferencingObjects = ReferenceGraph.Find(AssetToFind);
		if (ReferencingObjects)
		{
			for (TSet<UObject*>::TConstIterator SetIt(*ReferencingObjects); SetIt; ++SetIt)
			{
				Generate(*SetIt, OutObjects);
			}
		}
	}
};

void STextureToolUI::OnFindActorClicked()
{
	if (TextureListView->GetNumItemsSelected() > 0)
	{
		FTextureItemArray Array;
		TextureListView->GetSelectedItems(Array);
		TArray<UObject*> AssetsToFind;
		const bool SkipRedirectors = true;
		for (auto Item : Array)
			AssetsToFind.Add(Item->Texture.Get());

		const bool NoteSelectionChange = true;
		const bool DeselectBSPSurfs = true;
		const bool WarnAboutManyActors = false;
		GEditor->SelectNone(NoteSelectionChange, DeselectBSPSurfs, WarnAboutManyActors);


		FScopedSlowTask SlowTask(2 + AssetsToFind.Num(), NSLOCTEXT("AssetContextMenu", "FindAssetInWorld", "Finding actors that use this asset..."));
		SlowTask.MakeDialog();

		CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);

		TSet<const UObject*> OutObjects;
		WorldReferenceGenerator ObjRefGenerator;

		SlowTask.EnterProgressFrame();
		ObjRefGenerator.BuildReferencingData();

		for (UObject* AssetToFind : AssetsToFind)
		{
			SlowTask.EnterProgressFrame();
			ObjRefGenerator.MarkAllObjects();
			ObjRefGenerator.Generate(AssetToFind, OutObjects);
		}

		SlowTask.EnterProgressFrame();

		if (OutObjects.Num() > 0)
		{
			const bool InSelected = true;
			const bool Notify = false;

			// Select referencing actors
			for (const UObject* Object : OutObjects)
			{
				GEditor->SelectActor(const_cast<AActor*>(CastChecked<AActor>(Object)), InSelected, Notify);
			}

			GEditor->NoteSelectionChange();
		}
		else
		{
			FNotificationInfo Info(LOCTEXT("NoReferencingActorsFound", "No actors found."));
			Info.ExpireDuration = 3.0f;
			FSlateNotificationManager::Get().AddNotification(Info);
		}
	}
}

FReply STextureToolUI::OnFindTextureClicked()
{
	UpdateTextureListItems();
	return FReply::Handled();
}

FReply STextureToolUI::OnMergeClicked()
{
	auto Setting = UTextureMergeSettings::Get();
	Setting->Merge();
	return FReply::Handled();
}

FReply STextureToolUI::OnAutoSuffixClicked()
{
	auto Setting = UTextureMergeSettings::Get();
	Setting->AutoKeyword();
	return FReply::Handled();
}

void STextureToolUI::OpenTextureEditor(TSharedPtr<FTextureListItem> Item)
{
	FAssetEditorManager::Get().OpenEditorForAsset(Item->Texture.Get());
}

class SBatchMergeDialog : public SCompoundWidget
{
public:
	using ItemType = TSharedPtr<FString>;
	using ItemArray = TArray<ItemType>;
	using ListView = SListView<ItemType>;
public:
	SLATE_BEGIN_ARGS(SBatchMergeDialog) {}
	SLATE_END_ARGS()
	void Construct(const FArguments& InArgs, TArray<FString> InNames)
	{
		Names = MoveTemp(InNames);
		for (auto& Name : Names)
			Items.Add(MakeShared<FString>(Name));
		ListWidget = SNew(ListView)
			.ListItemsSource(&Items)
			.SelectionMode(ESelectionMode::None)
			.OnGenerateRow(this, &SBatchMergeDialog::OnGenerateWidgetForListView);
		ChildSlot
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().FillHeight(1.f)
			[
				SNew(SBorder).BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew( SBorder )
						.BorderImage( FEditorStyle::GetBrush( "DetailsView.CategoryTop" ) )
						.BorderBackgroundColor( FLinearColor( .6, .6, .6, 1.0f ) )
						.Padding(3.0f)
						[
							SNew( STextBlock )
							.Text( LOCTEXT( "MatchedNames", "Matched Names" ) )
							.Font( FEditorStyle::GetFontStyle( "BoldFont" ) )
							.ShadowOffset( FVector2D( 1.0f, 1.0f ) )
						]
					]
					+ SVerticalBox::Slot()
					.FillHeight(1.0f)
					[
						SNew(SScrollBox)
						+ SScrollBox::Slot()
						[
							ListWidget.ToSharedRef()
						]
					]
				]
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f,10.f,0.f,0.f)
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot().FillWidth(1.f)
				+SHorizontalBox::Slot().FillWidth(0.2f)
				[
					SNew(SButton).Text(LOCTEXT("ConfirmMerge", "Merge")).HAlign(HAlign_Center)
					.OnClicked(this, &SBatchMergeDialog::OnConfirmClicked)
				]
				+ SHorizontalBox::Slot().FillWidth(0.2f).Padding(10.f, 0.f)
				[
					SNew(SButton).Text(LOCTEXT("CancleMerge", "Cancle")).HAlign(HAlign_Center)
					.OnClicked(this, &SBatchMergeDialog::OnCancleClicked)
				]
			]
		];
	}
private:
	TArray<FString> Names;
	ItemArray Items;
	TSharedPtr<ListView> ListWidget;
	TSharedRef<ITableRow> OnGenerateWidgetForListView(TSharedPtr<FString> InItem, const TSharedRef<STableViewBase>& OwnerTable)
	{
		return SNew(STableRow<TSharedPtr<FString>>, OwnerTable)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.f)
			[
				SNew(STextBlock).Text(FText::FromString(*InItem))
			]
			+ SHorizontalBox::Slot().AutoWidth()
			[
				SNew(SButton).Text(LOCTEXT("RemoveMatched", "Remove"))
				.OnClicked(this, &SBatchMergeDialog::OnRemoveClicked, InItem)
			]
		];
	}

	void CloseDialog()
	{
		TSharedPtr<SWindow> ContainingWindow = FSlateApplication::Get().FindWidgetWindow(AsShared());

		if (ContainingWindow.IsValid())
		{
			ContainingWindow->RequestDestroyWindow();
		}
	}

	FReply OnConfirmClicked()
	{
		auto Setting = UTextureMergeSettings::Get();
		Setting->Batch(Names);
		CloseDialog();
		return FReply::Handled();
	}
	FReply OnCancleClicked()
	{
		CloseDialog();
		return FReply::Handled();
	}

	FReply OnRemoveClicked(TSharedPtr<FString> InItem)
	{
		Items.Remove(InItem);
		Names.Remove(*InItem);
		ListWidget->RequestListRefresh();
		return FReply::Handled();
	}
};



FReply STextureToolUI::OnBatchClicked()
{
	auto Setting = UTextureMergeSettings::Get();
	TArray<FString> Matched;
	if(!Setting->Match(Matched))
		return FReply::Handled();

	const FVector2D DEFAULT_WINDOW_SIZE = FVector2D(600, 400);
	auto Window = SNew(SWindow)
		.Title(LOCTEXT("BatchTitle", "Batch Merge"))
		.ClientSize(DEFAULT_WINDOW_SIZE);
	auto Dialog = SNew(SBatchMergeDialog, MoveTemp(Matched));
	Window->SetContent(Dialog);

	GEditor->EditorAddModalWindow(Window);
	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
