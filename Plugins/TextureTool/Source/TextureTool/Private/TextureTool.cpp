// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "TextureTool.h"
#include "Editor.h"
#include "TextureToolBrowserExtensions.h"
#include "TextureToolLevelEditorExtensions.h"
#include "LevelEditor.h"
#include "EditorStyle.h"
#include "WorkspaceMenuStructure.h"
#include "WorkspaceMenuStructureModule.h"
#include "Widgets/Docking/SDockTab.h"
#include "STextureToolUI.h"
#include "PropertyEditorModule.h"
#include "TextureMergeSettingsCustomization.h"
#include "Editor/DetailCustomizations/Public/DetailCustomizations.h"
#define LOCTEXT_NAMESPACE "FTextureToolModule"

const FName TextureToolTabName(TEXT("TextureTool"));

TSharedRef<SDockTab> SpawnTextureToolTab(const FSpawnTabArgs& SpawnTabArgs)
{
	TSharedPtr<SDockTab> MajorTab;
	SAssignNew(MajorTab, SDockTab)
		.Icon(FEditorStyle::Get().GetBrush("ClassViewer.TabIcon"))
		.TabRole(ETabRole::NomadTab);

	MajorTab->SetContent(SNew(STextureToolUI));

	return MajorTab.ToSharedRef();
}

void FTextureToolModule::StartupModule()
{
	FCoreDelegates::OnPostEngineInit.AddRaw(this, &FTextureToolModule::OnPostEngineInit);

	FLevelEditorModule& LevelEditorModule = FModuleManager::LoadModuleChecked<FLevelEditorModule>(TEXT("LevelEditor"));
	// register standalone UI
	auto RegisterTabSpawner = []()
	{
		FGlobalTabmanager::Get()->RegisterNomadTabSpawner(TextureToolTabName, FOnSpawnTab::CreateStatic(&SpawnTextureToolTab))
			.SetGroup(WorkspaceMenu::GetMenuStructure().GetLevelEditorCategory())
			.SetDisplayName(LOCTEXT("TextureToolTabTitle", "Texture Tool"))
			.SetTooltipText(LOCTEXT("TextureToolTooltipText", "Open the Texture Tool tab."))
			.SetIcon(FSlateIcon());
	};
	FLevelEditorModule* LocalLevelEditorModule = FModuleManager::GetModulePtr<FLevelEditorModule>(TEXT("LevelEditor"));
	if (LocalLevelEditorModule && LocalLevelEditorModule->GetLevelEditorTabManager())
	{
		RegisterTabSpawner();
	}
	else
	{
		LevelEditorTabManagerChangedHandle = LevelEditorModule.OnTabManagerChanged().AddLambda(RegisterTabSpawner);
	}
	static FName PropertyEditor("PropertyEditor");
	FPropertyEditorModule& PropertyModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>(PropertyEditor);
	PropertyModule.RegisterCustomPropertyTypeLayout("TextureChannelSrc", FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FTextureChannelSourceCustomization::MakeInstance));
	PropertyModule.RegisterCustomClassLayout("TextureMergeSettings", FOnGetDetailCustomizationInstance::CreateStatic(&FTextureMergeSettingDetail::MakeInstance));
	PropertyModule.NotifyCustomizationModuleChanged();
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
}

void FTextureToolModule::OnPostEngineInit()
{
	if (!IsRunningCommandlet())
	{
		FTextureToolBrowserExtensions::InstallHooks();
		FTextureToolLevelEditorExtentions::InstallHooks();
	}
}

void FTextureToolModule::ShutdownModule()
{
	FCoreDelegates::OnPostEngineInit.RemoveAll(this);
	if (!IsRunningCommandlet())
	{
		FTextureToolBrowserExtensions::RemoveHooks();
		FTextureToolLevelEditorExtentions::RemoveHooks();
	}
	FLevelEditorModule& LevelEditorModule = FModuleManager::LoadModuleChecked<FLevelEditorModule>(TEXT("LevelEditor"));
	if(LevelEditorTabManagerChangedHandle.IsValid())
		LevelEditorModule.OnTabContentChanged().Remove(LevelEditorTabManagerChangedHandle);

	static FName PropertyEditor("PropertyEditor");
	FPropertyEditorModule& PropertyModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>(PropertyEditor);
	PropertyModule.UnregisterCustomPropertyTypeLayout("TextureChannelSrc");
	PropertyModule.UnregisterCustomClassLayout("TextureMergeSettings");
	PropertyModule.NotifyCustomizationModuleChanged();
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FTextureToolModule, TextureTool)