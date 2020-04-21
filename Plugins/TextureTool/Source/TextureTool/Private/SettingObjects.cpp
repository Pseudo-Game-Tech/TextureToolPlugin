#include "SettingObjects.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/Package.h"

#include "AssetData.h"
#include "AssetToolsModule.h"
#include "Factories/Factory.h"
#include "Widgets/Input/SEditableText.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "EditorDirectories.h"
#include "PackageTools.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Views/SListView.h"
#include "ObjectTools.h"
#include "Misc/MessageDialog.h"
#include "AssetRegistryModule.h"
#include "ContentBrowserModule.h"
#include "IContentBrowserSingleton.h"
#include "Editor.h"
#include "Widgets/Views/SListView.h"
#include "IDetailGroup.h"
#include "Widgets/Images/SImage.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Input/SComboButton.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Kismet/KismetStringLibrary.h"
#include "Misc/FeedbackContext.h"
#define LOCTEXT_NAMESPACE "TextureToolUI"

UTextureMergeSettings* UTextureMergeSettings::Get()
{
	static UTextureMergeSettings* Settings = nullptr;
	if (!Settings)
	{
		Settings = DuplicateObject<UTextureMergeSettings>(GetMutableDefault<UTextureMergeSettings>(), GetTransientPackage());
		Settings->AddToRoot();
	}

	return Settings;
}

UTextureMergeSettings::UTextureMergeSettings()
{
	ReplaceTexture.Optional = false;
}

bool MergeTextures(UTextureRenderTarget2D* RT,
	UTexture2D* R, UTexture2D* G, UTexture2D* B, UTexture2D* A, FText& FailReason
	)
{
	int32 SizeX, SizeY;
	auto GetSize = [](UTexture2D* T)
	{
		return FIntPoint(T->GetSizeX(), T->GetSizeY());
	};
	bool SizeMatched = true;
	FIntPoint Size = FIntPoint::ZeroValue;
	if (R)
		Size = GetSize(R);

	if (G)
	{
		auto NSize = GetSize(G);
		if (Size != NSize && Size != FIntPoint::ZeroValue)
		{
			SizeMatched = false;
		}
		Size = NSize;
	}

	if (B)
	{
		auto NSize = GetSize(B);
		if (Size != NSize && Size != FIntPoint::ZeroValue)
		{
			SizeMatched = false;
		}
		Size = NSize;
	}

	if (A)
	{
		auto NSize = GetSize(A);
		if (Size != NSize && Size != FIntPoint::ZeroValue)
		{
			SizeMatched = false;
		}
	}


	SizeX = Size.X; SizeY = Size.Y;
	if (!SizeMatched)
	{
		FailReason = LOCTEXT("SizeNotMatch", "Source textures' size does not match!");
		return false;
	}
	const bool bIsValidSize = (SizeX != 0 && !(SizeX & (SizeX - 1)) &&
		SizeY != 0 && !(SizeY & (SizeY - 1)));
	if (!bIsValidSize || Size == FIntPoint::ZeroValue)
	{
		FailReason = LOCTEXT("SizeNotValid", "Source textures' size is not valid, must be power of two!");
		return false;
	}

	check(RT);
	if (RT->SizeX != SizeX || RT->SizeY != SizeY)
	{
		RT->InitAutoFormat(SizeX, SizeY);
		RT->UpdateResourceImmediate(true);
	}

	auto MergeShader = LoadObject<UMaterialInterface>(nullptr, TEXT("Material'/TextureTool/Shader/MergeTexture.MergeTexture'"));
	auto Merger = UMaterialInstanceDynamic::Create(MergeShader, GetTransientPackage());

	static FName TextureParamR("TextureR");
	static FName TextureParamG("TextureG");
	static FName TextureParamB("TextureB");
	static FName TextureParamA("TextureA");
	static FName ChannelParamR("ChannelR");
	static FName ChannelParamG("ChannelG");
	static FName ChannelParamB("ChannelB");
	static FName ChannelParamA("ChannelA");
	auto Setting = UTextureMergeSettings::Get();
	Merger->SetTextureParameterValue(TextureParamR, R);
	Merger->SetTextureParameterValue(TextureParamG, G);
	Merger->SetTextureParameterValue(TextureParamB, B);
	Merger->SetTextureParameterValue(TextureParamA, A);
	Merger->SetScalarParameterValue(ChannelParamR, (float)Setting->R.Channel);
	Merger->SetScalarParameterValue(ChannelParamG, (float)Setting->G.Channel);
	Merger->SetScalarParameterValue(ChannelParamB, (float)Setting->B.Channel);
	Merger->SetScalarParameterValue(ChannelParamA, (float)Setting->A.Channel);
	auto World = GEditor->GetEditorWorldContext().World();
	UKismetRenderingLibrary::DrawMaterialToRenderTarget(World, RT, Merger);

	return true;
}

bool UTextureMergeSettings::CanMerge() const
{
	int32 I = 0;
	if (R.Texture && R.Optional)
		++I;
	if (G.Texture && G.Optional)
		++I;
	if (B.Texture && B.Optional)
		++I;
	if (A.Texture && A.Optional)
		++I;
	return I > 0;
}

bool UTextureMergeSettings::CanBatch() const
{
	return !InputDirectory.Path.IsEmpty();
}

bool UTextureMergeSettings::CanAutoKeyword() const
{
	int32 I = 0;
	if (R.Texture && R.Optional)
		++I;
	if (G.Texture && G.Optional)
		++I;
	if (B.Texture && B.Optional)
		++I;
	if (A.Texture && A.Optional)
		++I;
	if (ReplaceTexture.Texture && ReplaceTexture.Optional)
		++I;
	return I > 1;
}

bool UTextureMergeSettings::Match(TArray<FString>& Names)
{
	FString SrcPath = InputDirectory.Path;
	if (SrcPath.IsEmpty())
	{
		FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("SrcPathEmpty", "Input directory is empty!"));
		return false;
	}
	IAssetRegistry& AssetRegistry = FModuleManager::Get().LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
	TArray<FAssetData> AssetsToSearch;
	AssetRegistry.GetAssetsByPath(FName(*SrcPath), AssetsToSearch, bRecursive);

	struct MergeGroup
	{
		FAssetData R;
		FAssetData G;
		FAssetData B;
		FAssetData A;
		FAssetData O;
		int32 Size = 0;
	};

	FString Name;
	FString AssetName;
	int32 InPos;
	TMap<FString, MergeGroup> MatchMap;

	auto GetName = [&](FTextureChannelSrc& Src)
	{
		if (!Src.Optional)
			return FString();
		if (Src.Keyword.IsEmpty())
			return AssetName;
		else
		{
			InPos = AssetName.Find(Src.Keyword, ESearchCase::CaseSensitive, ESearchDir::FromEnd);
			if (InPos < 0)
				return FString();
			return AssetName.Left(InPos) + TEXT("***") + AssetName.RightChop(InPos + Src.Keyword.Len());
		}
	};
	static const FName Texture2DClass("Texture2D");
	for (auto& Asset : AssetsToSearch)
	{
		if (Asset.AssetClass != Texture2DClass)
			continue;
		AssetName = Asset.PackageName.ToString();
		AssetName = AssetName.RightChop(SrcPath.Len() + 1);

		Name = GetName(R);
		if (!Name.IsEmpty())
		{
			auto& Group = MatchMap.FindOrAdd(Name);
			++Group.Size; Group.R = Asset;
		}
		Name = GetName(G);
		if (!Name.IsEmpty())
		{
			auto& Group = MatchMap.FindOrAdd(Name);
			++Group.Size; Group.G = Asset;
		}
		Name = GetName(B);
		if (!Name.IsEmpty())
		{
			auto& Group = MatchMap.FindOrAdd(Name);
			++Group.Size; Group.B = Asset;
		}
		Name = GetName(A);
		if (!Name.IsEmpty())
		{
			auto& Group = MatchMap.FindOrAdd(Name);
			++Group.Size; Group.A = Asset;
		}
		Name = GetName(ReplaceTexture);
		if (!Name.IsEmpty())
		{
			auto& Group = MatchMap.FindOrAdd(Name);
			++Group.Size; Group.O = Asset;
		}
	}

	int32 ValidSize = R.Optional + G.Optional + B.Optional + A.Optional + ReplaceTexture.Optional;
	for (auto& Pair : MatchMap)
	{
		if (Pair.Value.Size == ValidSize)
			Names.Add(Pair.Key);
	}
	return Names.Num() > 0;
}

void UTextureMergeSettings::Merge()
{
	FText FailureReason;

	FAssetToolsModule& AssetToolsModule = FModuleManager::Get().LoadModuleChecked<FAssetToolsModule>("AssetTools");
	FString AssetPath;

	const FString DefaultFilesystemDirectory = FEditorDirectories::Get().GetLastDirectory(ELastDirectory::NEW_ASSET);
	if (DefaultFilesystemDirectory.IsEmpty() || !FPackageName::TryConvertFilenameToLongPackageName(DefaultFilesystemDirectory, AssetPath))
	{
		// No saved path, just use the game content root
		AssetPath = TEXT("/Game");
	}

	FString DefaultName;
	EObjectFlags Flags;
	if (R.Texture)
	{
		R.Texture->GetName(DefaultName);
		Flags = R.Texture->GetFlags();
	}
	else if (G.Texture)
	{
		G.Texture->GetName(DefaultName);
		Flags = G.Texture->GetFlags();
	}
	else if (B.Texture)
	{
		B.Texture->GetName(DefaultName);
		Flags = B.Texture->GetFlags();
	}
	else if (A.Texture)
	{
		A.Texture->GetName(DefaultName);
		Flags = A.Texture->GetFlags();
	}

	FString PackageName;
	FString AssetName;
	AssetToolsModule.Get().CreateUniqueAssetName(AssetPath / DefaultName, TEXT(""), PackageName, AssetName);

	FSaveAssetDialogConfig SaveAssetDialogConfig;
	SaveAssetDialogConfig.DialogTitleOverride = LOCTEXT("SaveAssetDialogTitle", "Save Asset As");
	SaveAssetDialogConfig.DefaultPath = AssetPath;
	SaveAssetDialogConfig.DefaultAssetName = AssetName;
	SaveAssetDialogConfig.ExistingAssetPolicy = ESaveAssetDialogExistingAssetPolicy::Disallow;

	FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
	FString SaveObjectPath = ContentBrowserModule.Get().CreateModalSaveAssetDialog(SaveAssetDialogConfig);

	if (SaveObjectPath.IsEmpty())
		return;

	const FString SavePackageName = FPackageName::ObjectPathToPackageName(SaveObjectPath);
	const FString SavePackagePath = FPaths::GetPath(SavePackageName);
	const FString SaveAssetName = FPaths::GetBaseFilename(SavePackageName);
	FEditorDirectories::Get().SetLastDirectory(ELastDirectory::NEW_ASSET, SavePackagePath);
	PackageName = UPackageTools::SanitizePackageName(SavePackagePath / AssetName);

	UTextureRenderTarget2D* RT = NewObject<UTextureRenderTarget2D>(GetTransientPackage());
	RT->RenderTargetFormat = RTF_RGBA16f;
	if (!MergeTextures(RT,
		R.Optional ? R.Texture : nullptr,
		G.Optional ? G.Texture : nullptr,
		B.Optional ? B.Texture : nullptr,
		A.Optional ? A.Texture : nullptr,
		FailureReason
		))
	{
		FMessageDialog::Open(EAppMsgType::Ok, FailureReason);
		return;
	}
	UTexture2D* ST = RT->ConstructTexture2D(CreatePackage(NULL, *PackageName), SaveAssetName, Flags, CTF_Default, NULL);
	TArray<UObject*> Results;
	if (ST)
	{
		// package needs saving
		ST->MarkPackageDirty();

		// Notify the asset registry
		FAssetRegistryModule::AssetCreated(ST);

		Results.Add(ST);
	}
	else
	{
		FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("FailToSave", "Fail to save output texture!"));
		return;
	}
	if (ReplaceTexture.Optional && IsValid(ReplaceTexture.Texture))
	{
		TArray<UObject*> ToReplace;
		ToReplace.Add(ReplaceTexture.Texture);
		ObjectTools::ConsolidateObjects(ST, ToReplace);
		ReplaceTexture.Texture = nullptr;
	}
	Results.Add(ST);
	ContentBrowserModule.Get().SyncBrowserToAssets(Results);
	return;
}

void UTextureMergeSettings::Batch(const TArray<FString>& MatchedNames)
{
	FString SrcPath = InputDirectory.Path;
	if (SrcPath.IsEmpty())
	{
		FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("SrcPathEmpty", "Input directory is empty!"));
		return;
	}
	FAssetToolsModule& AssetToolsModule = FModuleManager::Get().LoadModuleChecked<FAssetToolsModule>("AssetTools");
	FString AssetPath;

	const FString DefaultFilesystemDirectory = FEditorDirectories::Get().GetLastDirectory(ELastDirectory::NEW_ASSET);
	if (DefaultFilesystemDirectory.IsEmpty() || !FPackageName::TryConvertFilenameToLongPackageName(DefaultFilesystemDirectory, AssetPath))
	{
		// No saved path, just use the game content root
		AssetPath = TEXT("/Game");
	}

	FString DefaultName = "Merged";
	FString PackageName;
	FString AssetName;
	AssetToolsModule.Get().CreateUniqueAssetName(AssetPath / DefaultName, TEXT(""), PackageName, AssetName);

	FSaveAssetDialogConfig SaveAssetDialogConfig;
	SaveAssetDialogConfig.DialogTitleOverride = LOCTEXT("SaveAssetDialogTitle_Batch", "Save Asset With Suffix");
	SaveAssetDialogConfig.DefaultPath = AssetPath;
	SaveAssetDialogConfig.DefaultAssetName = AssetName;
	SaveAssetDialogConfig.ExistingAssetPolicy = ESaveAssetDialogExistingAssetPolicy::AllowButWarn;

	FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
	FString SaveObjectPath = ContentBrowserModule.Get().CreateModalSaveAssetDialog(SaveAssetDialogConfig);

	if (SaveObjectPath.IsEmpty())
	{
		return;
	}

	const FString SavePackageName = FPackageName::ObjectPathToPackageName(SaveObjectPath);
	const FString SavePackagePath = FPaths::GetPath(SavePackageName);
	const FString SaveKeyword = FPaths::GetBaseFilename(SavePackageName);
	FEditorDirectories::Get().SetLastDirectory(ELastDirectory::NEW_ASSET, SavePackagePath);
	PackageName = UPackageTools::SanitizePackageName(SavePackagePath / AssetName);

	IAssetRegistry& AssetRegistry = FModuleManager::Get().LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
	FText FailureReason;
	UTextureRenderTarget2D* RT = NewObject<UTextureRenderTarget2D>(GetTransientPackage());
	RT->RenderTargetFormat = RTF_RGBA16f;
	GWarn->BeginSlowTask(LOCTEXT("PerformBatchMerge", "Performing Merge"), true, false);
	int32 Size = MatchedNames.Num();
	int32 I = 0;
	TArray<UObject*> Results;
	for (auto& Name : MatchedNames)
	{
		GWarn->StatusUpdate(I, Size, FText::FromString(Name));
		auto GetTexture = [&](FTextureChannelSrc& Src) -> UTexture2D*
		{
			if (!Src.Optional)
				return nullptr;
			auto RelativePath = Name.Replace(TEXT("***"), *Src.Keyword);
			auto PackageName = SrcPath / RelativePath;
			auto ObjectName = FPaths::GetBaseFilename(PackageName);
			auto ObjectPath = PackageName + TEXT(".") + ObjectName;
			FAssetData Texture = AssetRegistry.GetAssetByObjectPath(*(ObjectPath));
			return (UTexture2D*)Texture.GetAsset();
		};
		auto TR = GetTexture(R);
		auto TG = GetTexture(G);
		auto TB = GetTexture(B);
		auto TA = GetTexture(A);
		if (!MergeTextures(RT, TR, TG, TB, TA, FailureReason))
		{
			UE_LOG(LogTemp, Error, TEXT("%s merge failed due to %s"), *Name, *FailureReason.ToString());
			continue;
		}
		EObjectFlags Flags;
		if (TR)
			Flags = TR->GetFlags();
		else if (TG)
			Flags = TG->GetFlags();
		else if (TB)
			Flags = TB->GetFlags();
		else if (TA)
			Flags = TA->GetFlags();
		const FString AssetName = Name.Replace(TEXT("***"), *SaveKeyword);
		const FString SavePackageName = FPackageName::ObjectPathToPackageName(SavePackagePath / AssetName);
		FEditorDirectories::Get().SetLastDirectory(ELastDirectory::NEW_ASSET, SavePackagePath);
		const FString PackageName = UPackageTools::SanitizePackageName(SavePackagePath / AssetName);
		UTexture2D* ST = RT->ConstructTexture2D(CreatePackage(NULL, *PackageName), AssetName, Flags, CTF_Default, NULL);
		if (ST)
		{
			// package needs saving
			ST->MarkPackageDirty();

			// Notify the asset registry
			FAssetRegistryModule::AssetCreated(ST);

			Results.Add(ST);
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("Fail to save %s"), *Name);
			continue;
		}
		auto RP = GetTexture(ReplaceTexture);
		if (RP)
		{
			TArray<UObject*> ToReplace;
			ToReplace.Add(RP);
			ObjectTools::ConsolidateObjects(ST, ToReplace);
		}
		++I;
	}
	ContentBrowserModule.Get().SyncBrowserToAssets(Results);
	GWarn->EndSlowTask();
	
	return;
}

FString GetCommonPrefix(const FString& A, const FString& B)
{
	FString Result;
	int32 N = FMath::Min(A.Len(), B.Len());
	for (int32 i = 0; i < N && A[i] == B[i]; ++i)
		Result.AppendChar(A[i]);
	return Result;
}

FString GetCommonSuffix(const FString& A, const FString& B)
{
	FString Result;
	int32 N = FMath::Min(A.Len(), B.Len());
	for (int32 i = 0; i < N && A[A.Len() - i - 1] == B[B.Len() - i - 1]; ++i)
		Result.AppendChar(A[A.Len() - i - 1]);
	return Result.Reverse();
}

void UTextureMergeSettings::AutoKeyword()
{
	TArray<FString> Strings;
	if (R.Texture && R.Optional)
		Strings.Push(R.Texture->GetName());
	if (G.Texture && G.Optional)
		Strings.Push(G.Texture->GetName());
	if (B.Texture && B.Optional)
		Strings.Push(B.Texture->GetName());
	if (A.Texture && A.Optional)
		Strings.Push(A.Texture->GetName());
	if (ReplaceTexture.Texture && ReplaceTexture.Optional)
		Strings.Push(ReplaceTexture.Texture->GetName());
	if (Strings.Num() < 2)
	{
		return;
	}
	FString Prefix = Strings[0];
	for (int32 i = 1; i < Strings.Num(); ++i)
		Prefix = GetCommonPrefix(Prefix, Strings[i]);

	FString Suffix = Strings[0];
	for (int32 i = 1; i < Strings.Num(); ++i)
		Suffix = GetCommonSuffix(Suffix, Strings[i]);
	if (Prefix.IsEmpty() && Suffix.IsEmpty())
	{
		FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("NoKeyword", "Can't analysis keyword!"));
		return ;
	}
	if (R.Texture && R.Optional)
	{
		FString Name = R.Texture->GetName();
		R.Keyword = Name.Mid(Prefix.Len(), Name.Len() - Prefix.Len() - Suffix.Len());
	}
	if (G.Texture && G.Optional)
	{
		FString Name = G.Texture->GetName();
		G.Keyword = Name.Mid(Prefix.Len(), Name.Len() - Prefix.Len() - Suffix.Len());
	}
	if (B.Texture && B.Optional)
	{
		FString Name = B.Texture->GetName();
		B.Keyword = Name.Mid(Prefix.Len(), Name.Len() - Prefix.Len() - Suffix.Len());
	}
	if (A.Texture && A.Optional)
	{
		FString Name = A.Texture->GetName();
		A.Keyword = Name.Mid(Prefix.Len(), Name.Len() - Prefix.Len() - Suffix.Len());
	}
	if (ReplaceTexture.Texture && ReplaceTexture.Optional)
	{
		FString Name = ReplaceTexture.Texture->GetName();
		ReplaceTexture.Keyword = Name.Mid(Prefix.Len(), Name.Len() - Prefix.Len() - Suffix.Len());
	}
	return;
}