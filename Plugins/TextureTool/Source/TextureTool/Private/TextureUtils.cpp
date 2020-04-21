#include "TextureUtils.h"
#include "Engine/Texture2D.h"
#include "GameFramework/Actor.h"
#include "Components/MeshComponent.h"
#include "Components/DecalComponent.h"

bool FTextureToolUtils::CanDownScaleTexture(UTexture2D* Texture2D)
{
	if (!Texture2D->Source.IsPowerOfTwo() && Texture2D->PowerOfTwoMode == ETexturePowerOfTwoSetting::None)
		return false;
	return true;
}

void FTextureToolUtils::DownScaleTexture(UTexture2D* Texture2D)
{
	auto OldSize = Texture2D->MaxTextureSize == 0 ? Texture2D->GetSizeX() : Texture2D->MaxTextureSize;
	FPropertyChangedEvent EditMaxSizeEvent(UTexture2D::StaticClass()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(UTexture2D, MaxTextureSize)));
	Texture2D->MaxTextureSize = OldSize / 2;
	Texture2D->PostEditChangeProperty(EditMaxSizeEvent);
	Texture2D->Modify();
}

void FTextureToolUtils::ResetTextureSize(UTexture2D* Texture2D)
{
	FPropertyChangedEvent EditMaxSizeEvent(UTexture2D::StaticClass()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(UTexture2D, MaxTextureSize)));
	Texture2D->MaxTextureSize = 0;
	Texture2D->PostEditChangeProperty(EditMaxSizeEvent);
	Texture2D->Modify();
}

TArray<UTexture2D*> FTextureToolUtils::FindTextures(AActor* Actor)
{
	TArray<UTexture2D*> Textures;

	auto GetTexture2Ds = [&](UMaterialInterface* Material)
	{
		TArray<UTexture*> MaterialTextures;
		Material->GetUsedTextures(MaterialTextures, EMaterialQualityLevel::Num, false, GMaxRHIFeatureLevel, false);

		for (int32 TexIndex = 0; TexIndex < MaterialTextures.Num(); TexIndex++)
		{
			UTexture* Texture = MaterialTextures[TexIndex];
			if (Texture == NULL)
				continue;
			if (Texture->IsA(UTexture2D::StaticClass()))
			{
				UTexture2D* Tex2D = (UTexture2D*)Texture;
				Textures.Add(Tex2D);
			}
		}
	};

	for (UActorComponent* Component : Actor->GetComponents())
	{
		if (Component)
		{
			if (auto MeshComponent = Cast<UMeshComponent>(Component))
			{
				TArray<UMaterialInterface*> Materials = MeshComponent->GetMaterials();
				for (auto Material : Materials)
					GetTexture2Ds(Material);
			}
			if (auto DecalComponent = Cast<UDecalComponent>(Component))
			{
				UMaterialInterface* Material = DecalComponent->GetDecalMaterial();
				GetTexture2Ds(Material);
			}
		}
	}
	return Textures;
}
