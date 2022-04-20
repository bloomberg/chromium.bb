// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/personalization_app/personalization_app_ui.h"

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/ambient/ambient_client.h"
#include "ash/webui/grit/ash_personalization_app_resources.h"
#include "ash/webui/grit/ash_personalization_app_resources_map.h"
#include "ash/webui/personalization_app/personalization_app_ambient_provider.h"
#include "ash/webui/personalization_app/personalization_app_theme_provider.h"
#include "ash/webui/personalization_app/personalization_app_url_constants.h"
#include "ash/webui/personalization_app/personalization_app_user_provider.h"
#include "ash/webui/personalization_app/personalization_app_wallpaper_provider.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/common/url_constants.h"
#include "services/network/public/mojom/content_security_policy.mojom-shared.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/resources/grit/webui_generated_resources.h"
#include "ui/webui/mojo_web_ui_controller.h"

namespace ash {
namespace personalization_app {

namespace {

inline constexpr char kGooglePhotosURL[] = "https://photos.google.com";

GURL GetGooglePhotosURL() {
  return GURL(kGooglePhotosURL);
}

bool IsAmbientModeAllowed() {
  return ash::AmbientClient::Get() &&
         ash::AmbientClient::Get()->IsAmbientModeAllowed();
}

void AddResources(content::WebUIDataSource* source) {
  source->AddResourcePath("", IDR_ASH_PERSONALIZATION_APP_TRUSTED_INDEX_HTML);
  source->AddResourcePaths(base::make_span(
      kAshPersonalizationAppResources, kAshPersonalizationAppResourcesSize));
  source->AddResourcePath("test_loader.html", IDR_WEBUI_HTML_TEST_LOADER_HTML);
  source->AddResourcePath("test_loader.js", IDR_WEBUI_JS_TEST_LOADER_JS);
  source->AddResourcePath("test_loader_util.js",
                          IDR_WEBUI_JS_TEST_LOADER_UTIL_JS);

#if !DCHECK_IS_ON()
  // Add a default path to avoid crash when not debugging.
  source->SetDefaultResource(IDR_ASH_PERSONALIZATION_APP_TRUSTED_INDEX_HTML);
#endif  // !DCHECK_IS_ON()
}

void AddStrings(content::WebUIDataSource* source) {
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"personalizationTitle",
       IDS_PERSONALIZATION_APP_PERSONALIZATION_HUB_TITLE},
      {"wallpaperLabel", IDS_PERSONALIZATION_APP_WALLPAPER_LABEL},
      {"back", IDS_PERSONALIZATION_APP_BACK_BUTTON},
      {"currentlySet", IDS_PERSONALIZATION_APP_CURRENTLY_SET},
      {"myImagesLabel", IDS_PERSONALIZATION_APP_MY_IMAGES},
      {"wallpaperCollections", IDS_PERSONALIZATION_APP_WALLPAPER_COLLECTIONS},
      {"center", IDS_PERSONALIZATION_APP_CENTER},
      {"fill", IDS_PERSONALIZATION_APP_FILL},
      {"changeDaily", IDS_PERSONALIZATION_APP_CHANGE_DAILY},
      {"ariaLabelChangeDaily", IDS_PERSONALIZATION_APP_ARIA_LABEL_CHANGE_DAILY},
      {"refresh", IDS_PERSONALIZATION_APP_REFRESH},
      {"ariaLabelRefresh", IDS_PERSONALIZATION_APP_ARIA_LABEL_REFRESH},
      {"dailyRefresh", IDS_PERSONALIZATION_APP_DAILY_REFRESH},
      {"unknownImageAttribution",
       IDS_PERSONALIZATION_APP_UNKNOWN_IMAGE_ATTRIBUTION},
      {"networkError", IDS_PERSONALIZATION_APP_NETWORK_ERROR},
      {"ariaLabelLoading", IDS_PERSONALIZATION_APP_ARIA_LABEL_LOADING},
      // Using old wallpaper app error string pending final revision.
      // TODO(b/195609442)
      {"setWallpaperError", IDS_PERSONALIZATION_APP_SET_WALLPAPER_ERROR},
      {"loadWallpaperError", IDS_PERSONALIZATION_APP_LOAD_WALLPAPER_ERROR},
      {"dismiss", IDS_PERSONALIZATION_APP_DISMISS},
      {"ariaLabelViewFullScreen",
       IDS_PERSONALIZATION_APP_ARIA_LABEL_VIEW_FULL_SCREEN},
      {"exitFullscreen", IDS_PERSONALIZATION_APP_EXIT_FULL_SCREEN},
      {"ariaLabelExitFullscreen",
       IDS_PERSONALIZATION_APP_ARIA_LABEL_EXIT_FULL_SCREEN},
      {"setAsWallpaper", IDS_PERSONALIZATION_APP_SET_AS_WALLPAPER},
      {"zeroImages", IDS_PERSONALIZATION_APP_NO_IMAGES},
      {"oneImage", IDS_PERSONALIZATION_APP_ONE_IMAGE},
      {"multipleImages", IDS_PERSONALIZATION_APP_MULTIPLE_IMAGES},
      {"managedSetting", IDS_PERSONALIZATION_APP_MANAGED_SETTING},
      {"ariaLabelChangeWallpaper",
       IDS_PERSONALIZATION_APP_ARIA_LABEL_CHANGE_WALLPAPER},
      {"ariaLabelHome", IDS_PERSONALIZATION_APP_ARIA_LABEL_HOME},

      // Theme related strings.
      {"themeLabel", IDS_PERSONALIZATION_APP_THEME_LABEL},
      {"darkColorMode", IDS_PERSONALIZATION_APP_THEME_DARK_COLOR_MODE},
      {"lightColorMode", IDS_PERSONALIZATION_APP_THEME_LIGHT_COLOR_MODE},
      {"autoColorMode", IDS_PERSONALIZATION_APP_THEME_AUTO_COLOR_MODE},
      {"ariaLabelEnableDarkColorMode",
       IDS_PERSONALIZATION_APP_ARIA_LABEL_ENABLE_DARK_COLOR_MODE},
      {"ariaLabelEnableLightColorMode",
       IDS_PERSONALIZATION_APP_ARIA_LABEL_ENABLE_LIGHT_COLOR_MODE},
      {"ariaLabelEnableAutoColorMode",
       IDS_PERSONALIZATION_APP_ARIA_LABEL_ENABLE_AUTO_COLOR_MODE},

      // User/avatar related strings.
      {"avatarLabel", IDS_PERSONALIZATION_APP_AVATAR_LABEL},
      {"takeWebcamPhoto", IDS_PERSONALIZATION_APP_AVATAR_TAKE_PHOTO},
      {"takeWebcamVideo", IDS_PERSONALIZATION_APP_AVATAR_TAKE_VIDEO},
      {"confirmWebcamPhoto", IDS_PERSONALIZATION_APP_AVATAR_CONFIRM_PHOTO},
      {"confirmWebcamVideo", IDS_PERSONALIZATION_APP_AVATAR_CONFIRM_VIDEO},
      {"rejectWebcamPhoto", IDS_PERSONALIZATION_APP_AVATAR_REJECT_PHOTO},
      {"ariaLabelChangeAvatar",
       IDS_PERSONALIZATION_APP_ARIA_LABEL_CHANGE_AVATAR},
      {"ariaLabelGoToAccountSettings",
       IDS_PERSONALIZATION_APP_ARIA_LABEL_GO_TO_ACCOUNT_SETTINGS},
      {"googleProfilePhoto",
       IDS_PERSONALIZATION_APP_AVATAR_GOOGLE_PROFILE_PHOTO},
      {"chooseAFile", IDS_PERSONALIZATION_APP_AVATAR_CHOOSE_A_FILE},
      {"lastExternalImageTitle",
       IDS_PERSONALIZATION_APP_AVATAR_LAST_EXTERNAL_IMAGE},

      // Ambient mode related string.
      {"screensaverLabel", IDS_PERSONALIZATION_APP_SCREENSAVER_LABEL},
      {"ambientModePageDescription",
       IDS_PERSONALIZATION_APP_AMBIENT_MODE_PAGE_DESCRIPTION},
      {"ambientModeOn", IDS_PERSONALIZATION_APP_AMBIENT_MODE_ON},
      {"ambientModeOff", IDS_PERSONALIZATION_APP_AMBIENT_MODE_OFF},
      {"ambientModeTopicSourceTitle",
       IDS_PERSONALIZATION_APP_AMBIENT_MODE_TOPIC_SOURCE_TITLE},
      {"ambientModeTopicSourceGooglePhotos",
       IDS_PERSONALIZATION_APP_AMBIENT_MODE_TOPIC_SOURCE_GOOGLE_PHOTOS},
      {"ambientModeTopicSourceGooglePhotosDescription",
       IDS_PERSONALIZATION_APP_AMBIENT_MODE_TOPIC_SOURCE_GOOGLE_PHOTOS_DESC},
      {"ambientModeTopicSourceGooglePhotosDescriptionNoAlbum",
       IDS_PERSONALIZATION_APP_AMBIENT_MODE_TOPIC_SOURCE_GOOGLE_PHOTOS_DESC_NO_ALBUM},
      {"ambientModeTopicSourceArtGallery",
       IDS_PERSONALIZATION_APP_AMBIENT_MODE_TOPIC_SOURCE_ART_GALLERY},
      {"ambientModeTopicSourceArtGalleryDescription",
       IDS_PERSONALIZATION_APP_AMBIENT_MODE_TOPIC_SOURCE_ART_GALLERY_DESCRIPTION},
      {"ambientModeTopicSourceSelectedRow",
       IDS_PERSONALIZATION_APP_AMBIENT_MODE_TOPIC_SOURCE_SELECTED_ROW},
      {"ambientModeTopicSourceUnselectedRow",
       IDS_PERSONALIZATION_APP_AMBIENT_MODE_TOPIC_SOURCE_UNSELECTED_ROW},
      {"ambientModeWeatherTitle",
       IDS_PERSONALIZATION_APP_AMBIENT_MODE_WEATHER_TITLE},
      {"ambientModeWeatherUnitFahrenheit",
       IDS_PERSONALIZATION_APP_AMBIENT_MODE_WEATHER_UNIT_FAHRENHEIT},
      {"ambientModeWeatherUnitCelsius",
       IDS_PERSONALIZATION_APP_AMBIENT_MODE_WEATHER_UNIT_CELSIUS},
      {"ambientModeAlbumsSubpageRecentHighlightsDesc",
       IDS_PERSONALIZATION_APP_AMBIENT_MODE_ALBUMS_SUBPAGE_RECENT_DESC},
      {"ambientModeAlbumsSubpagePhotosNumPluralDesc",
       IDS_PERSONALIZATION_APP_AMBIENT_MODE_ALBUMS_SUBPAGE_PHOTOS_NUM_PLURAL},
      {"ambientModeAlbumsSubpagePhotosNumSingularDesc",
       IDS_PERSONALIZATION_APP_AMBIENT_MODE_ALBUMS_SUBPAGE_PHOTOS_NUM_SINGULAR},
      {"ambientModeAlbumsSubpageAlbumSelected",
       IDS_PERSONALIZATION_APP_AMBIENT_MODE_ALBUMS_SUBPAGE_ALBUM_SELECTED},
      {"ambientModeAlbumsSubpageAlbumUnselected",
       IDS_PERSONALIZATION_APP_AMBIENT_MODE_ALBUMS_SUBPAGE_ALBUM_UNSELECTED},
      {"ambientModeLastArtAlbumMessage",
       IDS_PERONSONALIZATION_APP_AMBIENT_MODE_LAST_ART_ALBUM_MESSAGE},
      {"ambientModeArtAlbumDialogCloseButtonLabel",
       IDS_PERONSONALIZATION_APP_AMBIENT_MODE_ART_ALBUM_DIALOG_CLOSE_BUTTON_LABEL},
      {"ambientModeAnimationTitle",
       IDS_PERSONALIZATION_APP_AMBIENT_MODE_ANIMATION_TITLE},
      {"ambientModeAnimationSlideshowLabel",
       IDS_PERSONALIZATION_APP_AMBIENT_MODE_ANIMATION_SLIDESHOW_LABEL},
      {"ambientModeAnimationFeelTheBreezeLabel",
       IDS_PERSONALIZATION_APP_AMBIENT_MODE_ANIMATION_FEEL_THE_BREEZE_LABEL},
      {"ambientModeAnimationFloatOnByLabel",
       IDS_PERSONALIZATION_APP_AMBIENT_MODE_ANIMATION_FLOAT_ON_BY_LABEL},
      {"ambientModeZeroStateMessage",
       IDS_PERSONALIZATION_APP_AMBIENT_MODE_ZERO_STATE_MESSAGE},
      {"ambientModeMultipleAlbumsDesc",
       IDS_PERSONALIZATION_APP_AMBIENT_MODE_MULTIPLE_ALBUMS_DESC},
      {"ambientModeMainPageZeroStateMessage",
       IDS_PERSONALIZATION_APP_AMBIENT_MODE_MAIN_PAGE_ZERO_STATE_MESSAGE},
      {"ambientModeTurnOnLabel",
       IDS_PERSONALIZATION_APP_AMBIENT_MODE_TURN_ON_LABEL},
      {"ariaLabelChangeScreensaver",
       IDS_PERSONALIZATION_APP_ARIA_LABEL_CHANGE_SCREENSAVER},

      // Google Photos strings
      {"googlePhotosLabel", IDS_PERSONALIZATION_APP_GOOGLE_PHOTOS},
      {"googlePhotosAlbumsTabLabel",
       IDS_PERSONALIZATION_APP_GOOGLE_PHOTOS_ALBUMS_TAB},
      {"googlePhotosPhotosTabLabel",
       IDS_PERSONALIZATION_APP_GOOGLE_PHOTOS_PHOTOS_TAB},
      {"googlePhotosZeroStateMessage",
       IDS_PERSONALIZATION_APP_GOOGLE_PHOTOS_ZERO_STATE_MESSAGE}};

  source->AddLocalizedStrings(kLocalizedStrings);

  source->AddString(
      "ambientModeAlbumsSubpageGooglePhotosTitle",
      l10n_util::GetStringFUTF16(
          IDS_PERSONALIZATION_APP_AMBIENT_MODE_ALBUMS_SUBPAGE_GOOGLE_PHOTOS_TITLE,
          base::UTF8ToUTF16(GetGooglePhotosURL().spec())));
  source->AddString(
      "ambientModeAlbumsSubpageGooglePhotosNoAlbum",
      l10n_util::GetStringFUTF16(
          IDS_PERSONALIZATION_APP_AMBIENT_MODE_ALBUMS_SUBPAGE_GOOGLE_PHOTOS_NO_ALBUM,
          base::UTF8ToUTF16(GetGooglePhotosURL().spec())));

  source->UseStringsJs();
  source->EnableReplaceI18nInJS();
}

void AddBooleans(content::WebUIDataSource* source) {
  source->AddBoolean("fullScreenPreviewEnabled",
                     features::IsWallpaperFullScreenPreviewEnabled());

  source->AddBoolean("isGooglePhotosIntegrationEnabled",
                     features::IsWallpaperGooglePhotosIntegrationEnabled());

  source->AddBoolean("isPersonalizationHubEnabled",
                     features::IsPersonalizationHubEnabled());

  source->AddBoolean("isDarkLightModeEnabled",
                     features::IsDarkLightModeEnabled());

  source->AddBoolean("isAmbientModeAllowed", IsAmbientModeAllowed());
}

}  // namespace

PersonalizationAppUI::PersonalizationAppUI(
    content::WebUI* web_ui,
    std::unique_ptr<PersonalizationAppAmbientProvider> ambient_provider,
    std::unique_ptr<PersonalizationAppThemeProvider> theme_provider,
    std::unique_ptr<PersonalizationAppUserProvider> user_provider,
    std::unique_ptr<PersonalizationAppWallpaperProvider> wallpaper_provider)
    : ui::MojoWebUIController(web_ui),
      ambient_provider_(std::move(ambient_provider)),
      theme_provider_(std::move(theme_provider)),
      user_provider_(std::move(user_provider)),
      wallpaper_provider_(std::move(wallpaper_provider)) {
  DCHECK(wallpaper_provider_);

  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      web_ui->GetWebContents()->GetBrowserContext(),
      kChromeUIPersonalizationAppHost);

  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::ScriptSrc,
      "script-src chrome://resources chrome://test chrome://webui-test "
      "'self';");

  // TODO(crbug.com/1098690): Trusted Type Polymer
  source->DisableTrustedTypesCSP();

  AddResources(source);
  AddStrings(source);
  AddBooleans(source);
}

PersonalizationAppUI::~PersonalizationAppUI() = default;

void PersonalizationAppUI::BindInterface(
    mojo::PendingReceiver<personalization_app::mojom::AmbientProvider>
        receiver) {
  ambient_provider_->BindInterface(std::move(receiver));
}

void PersonalizationAppUI::BindInterface(
    mojo::PendingReceiver<personalization_app::mojom::ThemeProvider> receiver) {
  theme_provider_->BindInterface(std::move(receiver));
}

void PersonalizationAppUI::BindInterface(
    mojo::PendingReceiver<personalization_app::mojom::WallpaperProvider>
        receiver) {
  wallpaper_provider_->BindInterface(std::move(receiver));
}

void PersonalizationAppUI::BindInterface(
    mojo::PendingReceiver<personalization_app::mojom::UserProvider> receiver) {
  user_provider_->BindInterface(std::move(receiver));
}

WEB_UI_CONTROLLER_TYPE_IMPL(PersonalizationAppUI)

}  // namespace personalization_app
}  // namespace ash
