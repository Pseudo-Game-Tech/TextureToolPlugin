#include "TextureToolBrowserExtensions.h"
#include "IAssetTools.h"
#include "AssetToolsModule.h"
#include "TextureUtils.h"
#include "AssetData.h"
#include "Engine/Texture2D.h"
#include "Misc/MessageDialog.h"
#include "Framework/Commands/UIAction.h"
#include "Framework/MultiBox/MultiBoxExtender.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "ContentBrowserModule.h"
#include "EditorStyleSet.h"
#define LOCTEXT_NAMESPACE "FTextureUtils"

struct FContentBrowserSelectedAssetExtensionBase
{
public:
	TArray<struct FAssetData> SelectedAssets;

public:
	virtual void Execute() {}
	virtual ~FContentBrowserSelectedAssetExtensionBase() {}
};

struct FDownScaleTextureExtension : public FContentBrowserSelectedAssetExtensionBase
{
	void Execute() override
	{
		// Create sprites for any selected textures
		TArray<UTexture2D*> Textures;
		for (auto AssetIt = SelectedAssets.CreateConstIterator(); AssetIt; ++AssetIt)
		{
			const FAssetData& AssetData = *AssetIt;
			if (UTexture2D* Texture = Cast<UTexture2D>(AssetData.GetAsset()))
				Textures.Add(Texture);
		}

		TArray<UTexture2D*> TexturesNotAllowed;
		for (auto TextureIt = Textures.CreateConstIterator(); TextureIt; ++TextureIt)
		{
			if (!FTextureToolUtils::CanDownScaleTexture(*TextureIt))
				TexturesNotAllowed.Add(*TextureIt);
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
			return;
		}
		else
		{
			for (UTexture2D* Texture : Textures)
			{
				FTextureToolUtils::DownScaleTexture(Texture);
			}
		}
	}
};

class FTextureToolBrowserExtensions_Impl
{
public:
	static void ExecuteSelectedContentFunctor(TSharedPtr<FContentBrowserSelectedAssetExtensionBase> SelectedAssetFunctor)
	{
		SelectedAssetFunctor->Execute();
	}

	static void CreateSpriteActionsSubMenu(FMenuBuilder& MenuBuilder, TArray<FAssetData> SelectedAssets)
	{
		MenuBuilder.AddSubMenu(
			LOCTEXT("TextureToolActionsSubMenuLabel", "Texture Tools"),
			LOCTEXT("SpriteActionsSubMenuToolTip", "Utility actions for this texture."),
			FNewMenuDelegate::CreateStatic(&FTextureToolBrowserExtensions_Impl::PopulateSpriteActionsMenu, SelectedAssets),
			false,
			FSlateIcon(FEditorStyle::GetStyleSetName(), "ClassIcon.Texture2D")
		);
	}

	static void PopulateSpriteActionsMenu(FMenuBuilder& MenuBuilder, TArray<FAssetData> SelectedAssets)
	{
		//TODO Style
		//const FName PaperStyleSetName = FPaperStyle::Get()->GetStyleSetName();
		const FName PaperStyleSetName = FEditorStyle::GetStyleSetName();
		TSharedPtr<FDownScaleTextureExtension> TextureConfigFunctor = MakeShareable(new FDownScaleTextureExtension());
		TextureConfigFunctor->SelectedAssets = SelectedAssets;

		FUIAction Action_DownScaleTexture(
			FExecuteAction::CreateStatic(&FTextureToolBrowserExtensions_Impl::ExecuteSelectedContentFunctor, StaticCastSharedPtr<FContentBrowserSelectedAssetExtensionBase>(TextureConfigFunctor)));

		MenuBuilder.AddMenuEntry(
			LOCTEXT("CB_Extension_Texture_DownScale", "DownScale texture"),
			LOCTEXT("CB_Extension_Texture_DownScale_Tooltip", "Set texture half the texture size"),
			FSlateIcon(PaperStyleSetName, "ClassIcon.Texture2D"),
			Action_DownScaleTexture,
			NAME_None,
			EUserInterfaceActionType::Button);
	}

	static TSharedRef<FExtender> OnExtendContentBrowserAssetSelectionMenu(const TArray<FAssetData>& SelectedAssets)
	{
		TSharedRef<FExtender> Extender(new FExtender());

		// Run thru the assets to determine if any meet our criteria
		bool bAnyTextures = false;
		for (auto AssetIt = SelectedAssets.CreateConstIterator(); AssetIt; ++AssetIt)
		{
			const FAssetData& Asset = *AssetIt;
			bAnyTextures = bAnyTextures || (Asset.AssetClass == UTexture2D::StaticClass()->GetFName());
		}

		if (bAnyTextures)
		{
			// Add the sprite actions sub-menu extender
			Extender->AddMenuExtension(
				"GetAssetActions",
				EExtensionHook::After,
				nullptr,
				FMenuExtensionDelegate::CreateStatic(&FTextureToolBrowserExtensions_Impl::CreateSpriteActionsSubMenu, SelectedAssets));
		}

		return Extender;
	}

	static TArray<FContentBrowserMenuExtender_SelectedAssets>& GetExtenderDelegates()
	{
		FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"));
		return ContentBrowserModule.GetAllAssetViewContextMenuExtenders();
	}
};



//////////////////////////////////////////////////////////////////////////
// FPaperContentBrowserExtensions

static FContentBrowserMenuExtender_SelectedAssets ContentBrowserExtenderDelegate;
static FDelegateHandle ContentBrowserExtenderDelegateHandle;

void FTextureToolBrowserExtensions::InstallHooks()
{
	ContentBrowserExtenderDelegate = FContentBrowserMenuExtender_SelectedAssets::CreateStatic(&FTextureToolBrowserExtensions_Impl::OnExtendContentBrowserAssetSelectionMenu);

	TArray<FContentBrowserMenuExtender_SelectedAssets>& CBMenuExtenderDelegates = FTextureToolBrowserExtensions_Impl::GetExtenderDelegates();
	CBMenuExtenderDelegates.Add(ContentBrowserExtenderDelegate);
	ContentBrowserExtenderDelegateHandle = CBMenuExtenderDelegates.Last().GetHandle();
}

void FTextureToolBrowserExtensions::RemoveHooks()
{
	TArray<FContentBrowserMenuExtender_SelectedAssets>& CBMenuExtenderDelegates = FTextureToolBrowserExtensions_Impl::GetExtenderDelegates();
	CBMenuExtenderDelegates.RemoveAll([](const FContentBrowserMenuExtender_SelectedAssets& Delegate) { return Delegate.GetHandle() == ContentBrowserExtenderDelegateHandle; });
}

//////////////////////////////////////////////////////////////////////////
