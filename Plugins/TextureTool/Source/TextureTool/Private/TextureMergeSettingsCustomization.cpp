#include "TextureMergeSettingsCustomization.h"
#include "PropertyHandle.h"
#include "DetailLayoutBuilder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"
#include "IDetailPropertyRow.h"
#include "DetailCategoryBuilder.h"
#include "DetailWidgetRow.h"
#include "DetailLayoutBuilder.h"
#include "IDetailChildrenBuilder.h"
#include "IDetailPropertyRow.h"
#include "SettingObjects.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "IDetailCustomization.h"
#include "IPropertyTypeCustomization.h"
#include "IDetailRootObjectCustomization.h"
#include "Widgets/Layout/SBox.h"
#include "Editor/PropertyEditor/Public/PropertyCustomizationHelpers.h"
#include "Engine/Texture2D.h"
#define LOCTEXT_NAMESPACE "TextureToolUI"

TSharedRef<IPropertyTypeCustomization> FTextureChannelSourceCustomization::MakeInstance()
{
	return MakeShareable(new FTextureChannelSourceCustomization);
}

void FTextureChannelSourceCustomization::CustomizeHeader(TSharedRef<IPropertyHandle> PropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	TSharedRef<IPropertyHandle> Texture = PropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FTextureChannelSrc, Texture)).ToSharedRef();
	TSharedRef<IPropertyHandle> Channel = PropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FTextureChannelSrc, Channel)).ToSharedRef();
	TSharedRef<IPropertyHandle> Keyword = PropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FTextureChannelSrc, Keyword)).ToSharedRef();
	Optional = PropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FTextureChannelSrc, Optional));
	PropertyHandle->MarkResetToDefaultCustomized(true);
	//Channel->MarkHiddenByCustomization();
	{
		HeaderRow.NameContent().MinDesiredWidth(30).MaxDesiredWidth(100)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth()
			[
				Optional->CreatePropertyValueWidget()
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(10.f,0.f,0.f,0.f)
			[
				PropertyHandle->CreatePropertyNameWidget()
			]
		];
		HeaderRow.ValueContent().MinDesiredWidth(200).MaxDesiredWidth(600)
		[
			SNew(SObjectPropertyEntryBox)
			.IsEnabled(this, &FTextureChannelSourceCustomization::IsEnabled)
			.PropertyHandle(Texture)
			.AllowClear(true)
			.DisplayBrowse(true)
			.DisplayCompactSize(false)
			.DisplayThumbnail(true)
			.DisplayUseSelected(true)
			.ThumbnailPool(CustomizationUtils.GetThumbnailPool())
			.AllowedClass(UTexture2D::StaticClass())
			.ThumbnailSizeOverride(FIntPoint(72, 72))
			.CustomContentSlot()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth()
				[
					Channel->CreatePropertyValueWidget(false)
				]
				+ SHorizontalBox::Slot().AutoWidth().Padding(10.f, 0.f).VAlign(VAlign_Bottom)
				[
					SNew(STextBlock).Text(LOCTEXT("Keyword", "Keyword"))
				]
				+ SHorizontalBox::Slot().FillWidth(1.f)
				[
					Keyword->CreatePropertyValueWidget(false)
				]
			]
		];
	}
}

bool FTextureChannelSourceCustomization::IsEnabled() const
{
	bool Value;
	Optional->GetValue(Value);
	return Value;
}

TSharedRef<IDetailCustomization> FTextureMergeSettingDetail::MakeInstance()
{
	return MakeShareable(new FTextureMergeSettingDetail);
}

void FTextureMergeSettingDetail::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	auto& Category = DetailBuilder.EditCategory("Merge");

	Category.AddProperty(GET_MEMBER_NAME_CHECKED(UTextureMergeSettings, R));
	Category.AddProperty(GET_MEMBER_NAME_CHECKED(UTextureMergeSettings, G));
	Category.AddProperty(GET_MEMBER_NAME_CHECKED(UTextureMergeSettings, B));
	Category.AddProperty(GET_MEMBER_NAME_CHECKED(UTextureMergeSettings, A));
	Category.AddCustomRow(FText::GetEmpty())
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.Padding(15, 12, 0, 12)
		[
			SNew(STextBlock)
			.Font(DetailBuilder.GetDetailFont())
			.Text(LOCTEXT("BatchReplace_Tip", "Tips: Remember to uncheck ReplaceTexture if nothing to replace"))
		]
	];
	Category.AddProperty(GET_MEMBER_NAME_CHECKED(UTextureMergeSettings, ReplaceTexture));
	Category.AddProperty(GET_MEMBER_NAME_CHECKED(UTextureMergeSettings, InputDirectory));
	Category.AddProperty(GET_MEMBER_NAME_CHECKED(UTextureMergeSettings, bRecursive));
}