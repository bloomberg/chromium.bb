// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/page_info/page_info_view_factory.h"

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/page_info/chrome_page_info_ui_delegate.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/page_info/page_info_about_this_site_content_view.h"
#include "chrome/browser/ui/views/page_info/page_info_main_view.h"
#include "chrome/browser/ui/views/page_info/page_info_navigation_handler.h"
#include "chrome/browser/ui/views/page_info/page_info_permission_content_view.h"
#include "chrome/browser/ui/views/page_info/page_info_security_content_view.h"
#include "components/page_info/page_info.h"
#include "components/page_info/proto/about_this_site_metadata.pb.h"
#include "components/permissions/permission_util.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/color/color_id.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/separator.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/vector_icons.h"

constexpr int PageInfoViewFactory::kMinBubbleWidth;
constexpr int PageInfoViewFactory::kMaxBubbleWidth;

namespace {

class PageInfoSubpageView : public views::View {
 public:
  PageInfoSubpageView(std::unique_ptr<views::View> header,
                      std::unique_ptr<views::View> content) {
    SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kVertical));
    AddChildView(std::move(header));
    content_ = AddChildView(std::move(content));
  }

  gfx::Size CalculatePreferredSize() const override {
    // Only the with of |content_| is taken into account, because the header
    // view contains site origin in the subtitle which can be very long.
    const int width = content_->GetPreferredSize().width();
    return gfx::Size(width, GetHeightForWidth(width));
  }

  void ChildPreferredSizeChanged(views::View* child) override {
    PreferredSizeChanged();
  }

  views::View* content_ = nullptr;
};

int GetIconSize() {
  return GetLayoutConstant(PAGE_INFO_ICON_SIZE);
}

}  // namespace

// static
std::unique_ptr<views::View> PageInfoViewFactory::CreateSeparator() {
  // Distance for multi content list is used, but split in half, since there is
  // a separator in the middle of it.
  const int separator_spacing = ChromeLayoutProvider::Get()->GetDistanceMetric(
                                    DISTANCE_CONTENT_LIST_VERTICAL_MULTI) /
                                2;
  auto separator = std::make_unique<views::Separator>();
  separator->SetProperty(views::kMarginsKey, gfx::Insets(separator_spacing, 0));
  return separator;
}

// static
std::unique_ptr<views::View> PageInfoViewFactory::CreateLabelWrapper() {
  const int icon_label_spacing = ChromeLayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_RELATED_LABEL_HORIZONTAL);
  auto label_wrapper = std::make_unique<views::View>();
  label_wrapper->SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kVertical);
  label_wrapper->SetProperty(views::kMarginsKey,
                             gfx::Insets(0, icon_label_spacing));
  label_wrapper->SetProperty(views::kCrossAxisAlignmentKey,
                             views::LayoutAlignment::kStretch);
  label_wrapper->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToZero,
                               views::MaximumFlexSizeRule::kUnbounded,
                               /*adjust_height_for_width =*/true)
          .WithWeight(1));
  return label_wrapper;
}

PageInfoViewFactory::PageInfoViewFactory(
    PageInfo* presenter,
    ChromePageInfoUiDelegate* ui_delegate,
    PageInfoNavigationHandler* navigation_handler)
    : presenter_(presenter),
      ui_delegate_(ui_delegate),
      navigation_handler_(navigation_handler) {}

std::unique_ptr<views::View> PageInfoViewFactory::CreateMainPageView(
    base::OnceClosure initialized_callback) {
  return std::make_unique<PageInfoMainView>(presenter_, ui_delegate_,
                                            navigation_handler_,
                                            std::move(initialized_callback));
}

std::unique_ptr<views::View> PageInfoViewFactory::CreateSecurityPageView() {
  return std::make_unique<PageInfoSubpageView>(
      CreateSubpageHeader(
          l10n_util::GetStringUTF16(IDS_PAGE_INFO_SECURITY_SUBPAGE_HEADER)),
      std::make_unique<PageInfoSecurityContentView>(
          presenter_, /*is_standalone_page=*/true));
}

std::unique_ptr<views::View> PageInfoViewFactory::CreatePermissionPageView(
    ContentSettingsType type) {
  return std::make_unique<PageInfoSubpageView>(
      CreateSubpageHeader(PageInfoUI::PermissionTypeToUIString(type)),
      std::make_unique<PageInfoPermissionContentView>(presenter_, ui_delegate_,
                                                      type));
}

std::unique_ptr<views::View> PageInfoViewFactory::CreateAboutThisSitePageView(
    const page_info::proto::SiteInfo& info) {
  return std::make_unique<PageInfoSubpageView>(
      CreateSubpageHeader(
          l10n_util::GetStringUTF16(IDS_PAGE_INFO_ABOUT_THIS_SITE_HEADER)),
      std::make_unique<PageInfoAboutThisSiteContentView>(presenter_,
                                                         ui_delegate_, info));
}

std::unique_ptr<views::View> PageInfoViewFactory::CreateSubpageHeader(
    std::u16string title) {
  ChromeLayoutProvider* layout_provider = ChromeLayoutProvider::Get();
  views::FlexSpecification stretch_specification =
      views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToZero,
                               views::MaximumFlexSizeRule::kUnbounded,
                               /*adjust_height_for_width =*/true)
          .WithWeight(1);
  auto wrapper = std::make_unique<views::View>();
  wrapper->SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kVertical);

  const int side_margin =
      layout_provider->GetInsetsMetric(views::INSETS_DIALOG).left();
  const int bottom_margin =
      layout_provider->GetDistanceMetric(DISTANCE_CONTENT_LIST_VERTICAL_MULTI);

  auto* header = wrapper->AddChildView(std::make_unique<views::View>());
  header->SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetCrossAxisAlignment(views::LayoutAlignment::kStart)
      .SetInteriorMargin(
          gfx::Insets(0, side_margin, bottom_margin, side_margin));
  header->SetProperty(views::kFlexBehaviorKey, stretch_specification);
  wrapper->AddChildView(CreateSeparator());

  auto back_button = views::CreateVectorImageButtonWithNativeTheme(
      base::BindRepeating(&PageInfoNavigationHandler::OpenMainPage,
                          base::Unretained(navigation_handler_),
                          base::DoNothing()),
      vector_icons::kArrowBackIcon);
  views::InstallCircleHighlightPathGenerator(back_button.get());
  back_button->SetID(VIEW_ID_BACK_BUTTON);
  back_button->SetTooltipText(l10n_util::GetStringUTF16(IDS_ACCNAME_BACK));
  back_button->SetProperty(views::kInternalPaddingKey,
                           back_button->GetInsets());
  header->AddChildView(std::move(back_button));

  auto* label_wrapper = header->AddChildView(CreateLabelWrapper());
  auto* title_label = label_wrapper->AddChildView(
      std::make_unique<views::Label>(title, views::style::CONTEXT_DIALOG_TITLE,
                                     views::style::STYLE_SECONDARY));
  title_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  auto* subtitle_label =
      label_wrapper->AddChildView(std::make_unique<views::Label>(
          presenter_->GetSimpleSiteName(), views::style::CONTEXT_LABEL,
          views::style::STYLE_SECONDARY,
          gfx::DirectionalityMode::DIRECTIONALITY_AS_URL));
  subtitle_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  subtitle_label->SetAllowCharacterBreak(true);
  subtitle_label->SetMultiLine(true);
  subtitle_label->SetProperty(views::kFlexBehaviorKey, stretch_specification);

  auto close_button = views::BubbleFrameView::CreateCloseButton(
      base::BindRepeating(&PageInfoNavigationHandler::CloseBubble,
                          base::Unretained(navigation_handler_)));
  close_button->SetID(VIEW_ID_PAGE_INFO_CLOSE_BUTTON);
  close_button->SetVisible(true);
  close_button->SetProperty(views::kInternalPaddingKey,
                            close_button->GetInsets());
  header->AddChildView(std::move(close_button));

  return wrapper;
}

// static
const ui::ImageModel PageInfoViewFactory::GetPermissionIcon(
    const PageInfo::PermissionInfo& info) {
  const gfx::VectorIcon* icon = &gfx::kNoneIcon;
  switch (info.type) {
    case ContentSettingsType::COOKIES:
      icon = &vector_icons::kCookieIcon;
      break;
    case ContentSettingsType::IMAGES:
      icon = &vector_icons::kPhotoIcon;
      break;
    case ContentSettingsType::JAVASCRIPT:
      icon = &vector_icons::kCodeIcon;
      break;
    case ContentSettingsType::POPUPS:
      icon = &vector_icons::kLaunchIcon;
      break;
    case ContentSettingsType::GEOLOCATION:
      icon = &vector_icons::kLocationOnIcon;
      break;
    case ContentSettingsType::NOTIFICATIONS:
      icon = &vector_icons::kNotificationsIcon;
      break;
    case ContentSettingsType::MEDIASTREAM_MIC:
      icon = &vector_icons::kMicIcon;
      break;
    case ContentSettingsType::MEDIASTREAM_CAMERA:
    case ContentSettingsType::CAMERA_PAN_TILT_ZOOM:
      icon = &vector_icons::kVideocamIcon;
      break;
    case ContentSettingsType::AUTOMATIC_DOWNLOADS:
      icon = &vector_icons::kFileDownloadIcon;
      break;
#if BUILDFLAG(IS_CHROMEOS_ASH) || defined(OS_WIN)
    case ContentSettingsType::PROTECTED_MEDIA_IDENTIFIER:
      icon = &vector_icons::kProtectedContentIcon;
      break;
#endif
    case ContentSettingsType::MIDI_SYSEX:
      icon = &vector_icons::kMidiIcon;
      break;
    case ContentSettingsType::BACKGROUND_SYNC:
      icon = &vector_icons::kSyncIcon;
      break;
    case ContentSettingsType::ADS:
      icon = &vector_icons::kAdsIcon;
      break;
    case ContentSettingsType::SOUND:
      icon = &vector_icons::kVolumeUpIcon;
      break;
    case ContentSettingsType::CLIPBOARD_READ_WRITE:
      icon = &vector_icons::kPageInfoContentPasteIcon;
      break;
    case ContentSettingsType::SENSORS:
      icon = &vector_icons::kSensorsIcon;
      break;
    case ContentSettingsType::USB_GUARD:
      icon = &vector_icons::kUsbIcon;
      break;
    case ContentSettingsType::SERIAL_GUARD:
      icon = &vector_icons::kSerialPortIcon;
      break;
    case ContentSettingsType::BLUETOOTH_GUARD:
      icon = &vector_icons::kBluetoothIcon;
      break;
    case ContentSettingsType::BLUETOOTH_SCANNING:
      icon = &vector_icons::kBluetoothScanningIcon;
      break;
    case ContentSettingsType::FILE_SYSTEM_WRITE_GUARD:
      icon = &vector_icons::kSaveOriginalFileIcon;
      break;
    case ContentSettingsType::VR:
    case ContentSettingsType::AR:
      icon = &vector_icons::kVrHeadsetIcon;
      break;
    case ContentSettingsType::WINDOW_PLACEMENT:
      icon = &vector_icons::kSelectWindowIcon;
      break;
    case ContentSettingsType::FONT_ACCESS:
      icon = &vector_icons::kFontDownloadIcon;
      break;
    case ContentSettingsType::HID_GUARD:
      icon = &vector_icons::kVideogameAssetIcon;
      break;
    case ContentSettingsType::IDLE_DETECTION:
      icon = &vector_icons::kDevicesIcon;
      break;
    case ContentSettingsType::FILE_HANDLING:
      icon = &vector_icons::kDescriptionIcon;
      break;
    default:
      // All other |ContentSettingsType|s do not have icons on desktop or are
      // not shown in the Page Info bubble.
      NOTREACHED();
      break;
  }

  ContentSetting setting = info.setting == CONTENT_SETTING_DEFAULT
                               ? info.default_setting
                               : info.setting;
  const bool show_blocked_badge =
      !permissions::PermissionUtil::IsGuardContentSetting(info.type)
          ? setting == CONTENT_SETTING_BLOCK || setting == CONTENT_SETTING_ASK
          : setting == CONTENT_SETTING_BLOCK;
  return ui::ImageModel::FromVectorIcon(
      *icon, ui::kColorIcon, GetIconSize(),
      show_blocked_badge ? &vector_icons::kBlockedBadgeIcon : nullptr);
}

// static
const ui::ImageModel PageInfoViewFactory::GetChosenObjectIcon(
    const PageInfoUI::ChosenObjectInfo& object,
    bool deleted) {
  // The permissions data for device APIs will always appear even if the device
  // is not currently conncted to the system.
  // TODO(https://crbug.com/1048860): Check the connected status of devices and
  // change the icon to one that reflects that status.
  const gfx::VectorIcon* icon = &gfx::kNoneIcon;
  switch (object.ui_info.content_settings_type) {
    case ContentSettingsType::USB_CHOOSER_DATA:
      icon = &vector_icons::kUsbIcon;
      break;
    case ContentSettingsType::SERIAL_CHOOSER_DATA:
      icon = &vector_icons::kSerialPortIcon;
      break;
    case ContentSettingsType::BLUETOOTH_CHOOSER_DATA:
      icon = &vector_icons::kBluetoothIcon;
      break;
    case ContentSettingsType::HID_CHOOSER_DATA:
      icon = &vector_icons::kVideogameAssetIcon;
      break;
    default:
      // All other content settings types do not represent chosen object
      // permissions.
      NOTREACHED();
      break;
  }

  return ui::ImageModel::FromVectorIcon(
      *icon, ui::kColorIcon, GetIconSize(),
      deleted ? &vector_icons::kBlockedBadgeIcon : nullptr);
}

// static
const ui::ImageModel PageInfoViewFactory::GetValidCertificateIcon() {
  return ui::ImageModel::FromVectorIcon(vector_icons::kCertificateIcon,
                                        ui::kColorIcon, GetIconSize());
}

// static
const ui::ImageModel PageInfoViewFactory::GetInvalidCertificateIcon() {
  return ui::ImageModel::FromVectorIcon(vector_icons::kCertificateIcon,
                                        ui::kColorIcon, GetIconSize(),
                                        &vector_icons::kBlockedBadgeIcon);
}

// static
const ui::ImageModel PageInfoViewFactory::GetSiteSettingsIcon() {
  return ui::ImageModel::FromVectorIcon(vector_icons::kSettingsIcon,
                                        ui::kColorIcon);
}

// static
const ui::ImageModel PageInfoViewFactory::GetVrSettingsIcon() {
  return ui::ImageModel::FromVectorIcon(vector_icons::kVrHeadsetIcon,
                                        ui::kColorIcon);
}

// static
const ui::ImageModel PageInfoViewFactory::GetLaunchIcon() {
  return ui::ImageModel::FromVectorIcon(vector_icons::kLaunchIcon,
                                        ui::kColorIconSecondary, GetIconSize());
}

// static
const ui::ImageModel PageInfoViewFactory::GetConnectionNotSecureIcon() {
  return ui::ImageModel::FromVectorIcon(vector_icons::kNotSecureWarningIcon,
                                        ui::kColorAlertHighSeverity);
}

// static
const ui::ImageModel PageInfoViewFactory::GetConnectionSecureIcon() {
  return ui::ImageModel::FromVectorIcon(vector_icons::kHttpsValidIcon,
                                        ui::kColorIcon);
}

// static
const ui::ImageModel PageInfoViewFactory::GetOpenSubpageIcon() {
  return ui::ImageModel::FromVectorIcon(vector_icons::kSubmenuArrowIcon,
                                        ui::kColorIcon);
}

// static
const ui::ImageModel PageInfoViewFactory::GetAboutThisSiteIcon() {
  return ui::ImageModel::FromVectorIcon(views::kInfoIcon, ui::kColorIcon,
                                        GetIconSize());
}

// static
const ui::ImageModel PageInfoViewFactory::GetManagedPermissionIcon(
    const PageInfo::PermissionInfo& info) {
  const gfx::VectorIcon& managed_vector_icon =
      info.source == content_settings::SETTING_SOURCE_EXTENSION
          ? vector_icons::kExtensionIcon
          : vector_icons::kBusinessIcon;
  return ui::ImageModel::FromVectorIcon(managed_vector_icon, ui::kColorIcon,
                                        GetIconSize());
}
