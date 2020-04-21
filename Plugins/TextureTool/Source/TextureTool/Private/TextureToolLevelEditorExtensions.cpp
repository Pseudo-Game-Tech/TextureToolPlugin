#include "TextureToolLevelEditorExtensions.h"
#include "IAssetTools.h"
#include "AssetToolsModule.h"
#include "TextureUtils.h"
#include "AssetData.h"
#include "Engine/Texture2D.h"
#include "Misc/MessageDialog.h"
#include "Framework/Commands/UIAction.h"
#include "Framework/MultiBox/MultiBoxExtender.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "GameFramework/Actor.h"
#include "Components/MeshComponent.h"
#include "Components/DecalComponent.h"
#include "Editor.h"
#include "LevelEditor.h"
#include "Engine/Selection.h"
#include "TextureTool.h"
#include "STextureToolUI.h"
#include "Widgets/Docking/SDockTab.h"
#define LOCTEXT_NAMESPACE "FTextureUtils"

class FTextureToolLevelEditorExtentions_Impl
{
public:
	static void ShowReferencedTextures()
	{
		auto Tab = FGlobalTabmanager::Get()->InvokeTab(TextureToolTabName);
		auto UI = StaticCastSharedRef<STextureToolUI>(Tab->GetContent());
		UI->UpdateTextureListItems();
	}

	static void CreateSpriteActionsMenuEntries(FMenuBuilder& MenuBuilder)
	{
		MenuBuilder.BeginSection("TextureTool", LOCTEXT("TextureToolLevelEditorHeading", "TextureTool"));

		FUIAction Action_ShowTextures(FExecuteAction::CreateStatic(&FTextureToolLevelEditorExtentions_Impl::ShowReferencedTextures));

		MenuBuilder.AddMenuEntry(
			LOCTEXT("MenuExtensionFindTexture", "Find Textures"),
			LOCTEXT("MenuExtensionFindTextures_Tooltip", "Find all textures used by selected actors"),
			FSlateIcon(),
			Action_ShowTextures,
			NAME_None,
			EUserInterfaceActionType::Button);

		MenuBuilder.EndSection();
	}

	static TSharedRef<FExtender> OnExtendLevelEditorMenu(const TSharedRef<FUICommandList> CommandList, TArray<AActor*> SelectedActors)
	{
		TSharedRef<FExtender> Extender(new FExtender());

		// Run thru the actors to determine if any meet our criteria
		bool bWithMaterial = false;

		for (AActor* Actor : SelectedActors)
		{
			for (UActorComponent* Component : Actor->GetComponents())
			{
				if (Component)
				{
					if (Component->IsA(UMeshComponent::StaticClass()) || Component->IsA(UDecalComponent::StaticClass()))
					{
						bWithMaterial = true;
					}
				}
			}

		}

		if (bWithMaterial)
		{
			// Add the sprite actions sub-menu extender
			Extender->AddMenuExtension(
				"ActorType",
				EExtensionHook::Before,
				nullptr,
				FMenuExtensionDelegate::CreateStatic(&FTextureToolLevelEditorExtentions_Impl::CreateSpriteActionsMenuEntries));
		}

		return Extender;
	}
};



//////////////////////////////////////////////////////////////////////////
// FPaperContentBrowserExtensions

static FLevelEditorModule::FLevelViewportMenuExtender_SelectedActors LevelEditorMenuExtenderDelegate;
static FDelegateHandle LevelEditorExtenderDelegateHandle;

void FTextureToolLevelEditorExtentions::InstallHooks()
{
	LevelEditorMenuExtenderDelegate = FLevelEditorModule::FLevelViewportMenuExtender_SelectedActors::CreateStatic(&FTextureToolLevelEditorExtentions_Impl::OnExtendLevelEditorMenu);

	FLevelEditorModule& LevelEditorModule = FModuleManager::Get().LoadModuleChecked<FLevelEditorModule>("LevelEditor");
	auto& MenuExtenders = LevelEditorModule.GetAllLevelViewportContextMenuExtenders();
	MenuExtenders.Add(LevelEditorMenuExtenderDelegate);
	LevelEditorExtenderDelegateHandle = MenuExtenders.Last().GetHandle();
}

void FTextureToolLevelEditorExtentions::RemoveHooks()
{
	// Remove level viewport context menu extenders
	if (FModuleManager::Get().IsModuleLoaded("LevelEditor"))
	{
		FLevelEditorModule& LevelEditorModule = FModuleManager::LoadModuleChecked<FLevelEditorModule>("LevelEditor");
		LevelEditorModule.GetAllLevelViewportContextMenuExtenders().RemoveAll([&](const FLevelEditorModule::FLevelViewportMenuExtender_SelectedActors& Delegate) {
			return Delegate.GetHandle() == LevelEditorExtenderDelegateHandle;
			});
	}
}

//////////////////////////////////////////////////////////////////////////
