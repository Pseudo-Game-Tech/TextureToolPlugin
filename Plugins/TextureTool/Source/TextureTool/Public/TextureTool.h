// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class STextureToolUI;

class FTextureToolModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	void OnPostEngineInit();
	virtual void ShutdownModule() override;

private:

	FDelegateHandle LevelEditorTabManagerChangedHandle;
};
const extern FName TextureToolTabName;