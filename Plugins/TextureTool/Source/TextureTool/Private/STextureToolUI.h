#pragma once
#include "CoreMinimal.h"
#include "SlateFwd.h"
#include "Layout/Visibility.h"
#include "Input/Reply.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"

template<class ItemType> class SListView;
class UTexture2D;
class AActor;
class FAssetThumbnailPool;
class STableViewBase;
class IDetailsView;
class ITableRow;
class SBox;
struct FAssetData;
class IMenu;

enum class EToolMode
{
	TextureFinder,
	TextureMerger,
	TextureMergerBatch,
};

class STextureToolUI : public SCompoundWidget
{
private:
	struct FTextureListItem
	{
		TSoftObjectPtr<UTexture2D> Texture;
	};
	struct FNameListItem
	{
		FString Name;
	};
	using SNameListView = SListView<TSharedPtr<FNameListItem>>;
	using FNameItemArray = TArray<TSharedPtr<FNameListItem>>;

	using STextureListView = SListView<TSharedPtr<FTextureListItem>>;
	using FTextureItemArray = TArray<TSharedPtr<FTextureListItem>>;

public:
	SLATE_BEGIN_ARGS(STextureToolUI) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;
	void UpdateTextureListItems();
private:
	TSharedPtr<SWidget> CreateToolBarWidget();
	TSharedPtr<SWidget> CreateFinderWidget();
	TSharedPtr<SWidget> CreateMergerWidget();
	TSharedPtr<SWidget> CreateTextureContextMenu();
	void CreateDetailView();
	TSharedRef<ITableRow> OnGenerateWidgetForTextureListView(TSharedPtr<FTextureListItem> InItem, const TSharedRef<STableViewBase>& OwnerTable);

	bool HasSelectedTexture() const;
	bool HasSelectedActor() const;
	bool CanMerge() const;
	bool CanBatch() const;
	bool CanAutoKeyword() const;
	void OnDownScaleClicked();
	void OnResetSizeClicked();
	void OnBrowseToClicked();
	void OnFindActorClicked();
	FReply OnFindTextureClicked();
	FReply OnMergeClicked();
	FReply OnBatchClicked();
	FReply OnAutoSuffixClicked();
	void OpenTextureEditor(TSharedPtr<FTextureListItem> Item);

	FTextureItemArray TextureListItems;
	TSharedPtr<STextureListView> TextureListView;

	TSharedPtr<FAssetThumbnailPool> AssetThumbnailPool;
	TSharedPtr<IDetailsView> SettingsDetailsView;
	TSharedPtr<SWidget> FinderWidget;
	TSharedPtr<SWidget> MergerWidget;
	TSharedPtr<SBox> InlineContentHolder;
	EToolMode Mode;
};