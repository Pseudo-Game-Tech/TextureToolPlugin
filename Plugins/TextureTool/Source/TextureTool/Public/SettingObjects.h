#pragma once
#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "SettingObjects.generated.h"

UENUM()
enum class EChannel
{
	R,G,B,A
};

class UTexture2D;

USTRUCT()
struct FTextureChannelSrc
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, Category = Source, meta = (NoResetToDefault))
	UTexture2D* Texture;
	UPROPERTY(EditAnywhere, Category = Source)
	EChannel Channel;
	UPROPERTY(EditAnywhere, Category = Source)
	bool Optional = true;
	UPROPERTY(EditAnywhere, Category = Source)
	FString Keyword;
};


UCLASS()
class UTextureMergeSettings : public UObject
{
	GENERATED_BODY()
public:
	UTextureMergeSettings();
	static UTextureMergeSettings* Get();
	/** Texture to which Painting should be Applied */
	UPROPERTY(EditAnywhere, Category = Merge, meta = (DisplayName = "R Channel Source"))
	FTextureChannelSrc R;

	UPROPERTY(EditAnywhere, Category = Merge, meta = (DisplayName = "G Channel Source"))
	FTextureChannelSrc G;

	UPROPERTY(EditAnywhere, Category = Merge, meta = (DisplayName = "B Channel Source"))
	FTextureChannelSrc B;

	UPROPERTY(EditAnywhere, Category = Merge, meta = (DisplayName = "A Channel Source"))
	FTextureChannelSrc A;

	UPROPERTY(EditAnywhere, Category = Merge, meta = (ContentDir))
	FDirectoryPath InputDirectory;

	UPROPERTY(EditAnywhere, Category = Merge)
	bool bRecursive;

	UPROPERTY(EditAnywhere, Category = Merge)
	FTextureChannelSrc ReplaceTexture;

	bool CanMerge() const;
	bool CanBatch() const;
	bool CanAutoKeyword() const;
	bool Match(TArray<FString>& Names);
	void Merge();
	void Batch(const TArray<FString>& MatchedNames);
	void AutoKeyword();
};