#pragma once
#include "CoreMinimal.h"

class UTexture2D;
class AActor;
class UTexture2D;

struct FTextureToolUtils
{
	static bool CanDownScaleTexture(UTexture2D* Texture);
	static void DownScaleTexture(UTexture2D* Texture);
	static void ResetTextureSize(UTexture2D* Texture);
	static TArray<UTexture2D*> FindTextures(AActor* Actor);
};