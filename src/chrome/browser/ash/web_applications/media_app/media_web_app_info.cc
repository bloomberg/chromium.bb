// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/web_applications/media_app/media_web_app_info.h"
#include "base/containers/span.h"

#include <memory>
#include <string>

#include "ash/constants/ash_features.h"
#include "ash/grit/ash_media_app_resources.h"
#include "ash/webui/media_app_ui/url_constants.h"
#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ash/web_applications/system_web_app_install_utils.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_application_info.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

using FileHandlerConfig = std::pair<const char*, const char*>;

// FileHandler configuration.
// See https://github.com/WICG/file-handling/blob/main/explainer.md.
constexpr FileHandlerConfig kFileHandlers[] = {
    {"image/*", ""},
    {"video/*", ""},

    // Raw images. Note the MIME type doesn't really matter here. MIME sniffing
    // logic in the files app tends to detect image/tiff, but sniffing is only
    // done for "local" files, so the extension list is needed in addition to
    // the "image/*" wildcard above. The MIME type is never sent to the web app.
    {"image/tiff", ".arw,.cr2,.dng,.nef,.nrw,.orf,.raf,.rw2"},

    // More video formats; building on the video/* wildcard which doesn't
    // actually catch very much due to hard-coded maps in Chromium. Again, the
    // MIME type doesn't really matter. "video/mpeg" is used a catchall for
    // unknown mime types.
    {"video/ogg", ".ogv,.ogx,.ogm"},
    {"video/webm", ".webm"},
    {"video/mpeg", ".3gp,.avi,.m4v,.mkv,.mov,.mp4,.mpeg,.mpeg4,.mpg,.mpg4"},

    // More image formats. These are needed because MIME types are not always
    // available, even for known extensions. E.g. filesystem providers may
    // override Chrome's builtin mapping with an empty string. All supported
    // image/* `k{Primary,Secondary}Mappings` in net/base/mime_util.cc should be
    // listed here to ensure consistency with the image/* handler.
    {"image/bmp", ".bmp"},
    {"image/gif", ".gif"},
    {"image/vnd.microsoft.icon", ".ico"},
    {"image/jpeg", ".jpeg,.jpg,.jpe,.jfif,.jif,.jfi,.pjpeg,.pjp"},
    {"image/png", ".png"},
    {"image/webp", ".webp"},
    {"image/svg+xml", ".svg,.svgz"},
    {"image/avif", ".avif"},

    // PDF.
    {"application/pdf", ".pdf"},

    // When updating this list, `FOO_EXTENSIONS` in go/bl-launch should be
    // updated as well.
};

constexpr FileHandlerConfig kAudioFileHandlers[] = {
    {"audio/*", ""},

    // More audio formats.
    {"audio/flac", ".flac"},
    {"audio/m4a", ".m4a"},
    {"audio/mpeg", ".mp3"},
    {"audio/ogg", ".oga,.ogg,.opus"},
    {"audio/wav", ".wav"},
    {"audio/webm", "weba"},

    // Note: some extensions appear twice. See mime_util.cc.
    {"audio/mp3", "mp3"},
    {"audio/x-m4a", "m4a"},

    // When updating this list, `AUDIO_EXTENSIONS` in go/bl-launch should be
    // updated as well.
};

// Converts a FileHandlerConfig constexpr into the type needed to populate the
// WebApplicationInfo's `accept` property.
std::vector<apps::FileHandler::AcceptEntry> MakeFileHandlerAccept(
    base::span<const FileHandlerConfig> config) {
  std::vector<apps::FileHandler::AcceptEntry> result;
  result.reserve(config.size());

  const std::string separator = ",";
  for (const auto& handler : config) {
    result.emplace_back();
    result.back().mime_type = handler.first;
    auto file_extensions = base::SplitString(
        handler.second, separator, base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
    result.back().file_extensions.insert(file_extensions.begin(),
                                         file_extensions.end());
  }
  return result;
}

}  // namespace

MediaSystemAppDelegate::MediaSystemAppDelegate(Profile* profile)
    : web_app::SystemWebAppDelegate(
          web_app::SystemAppType::MEDIA,
          "Media",
          GURL("chrome://media-app/pwa.html"),
          profile,
          web_app::OriginTrialsMap(
              {{web_app::GetOrigin("chrome://media-app"), {"FileHandling"}}})) {
}

std::unique_ptr<WebApplicationInfo> CreateWebAppInfoForMediaWebApp() {
  std::unique_ptr<WebApplicationInfo> info =
      std::make_unique<WebApplicationInfo>();
  info->start_url = GURL(ash::kChromeUIMediaAppURL);
  info->scope = GURL(ash::kChromeUIMediaAppURL);

  info->title = l10n_util::GetStringUTF16(IDS_MEDIA_APP_APP_NAME);
  web_app::CreateIconInfoForSystemWebApp(
      info->start_url,
      {
          {"app_icon_16.png", 16, IDR_MEDIA_APP_GALLERY_ICON_16_PNG},
          {"app_icon_32.png", 32, IDR_MEDIA_APP_GALLERY_ICON_32_PNG},
          {"app_icon_48.png", 48, IDR_MEDIA_APP_GALLERY_ICON_48_PNG},
          {"app_icon_64.png", 64, IDR_MEDIA_APP_GALLERY_ICON_64_PNG},
          {"app_icon_96.png", 96, IDR_MEDIA_APP_GALLERY_ICON_96_PNG},
          {"app_icon_128.png", 128, IDR_MEDIA_APP_GALLERY_ICON_128_PNG},
          {"app_icon_192.png", 192, IDR_MEDIA_APP_GALLERY_ICON_192_PNG},
          {"app_icon_256.png", 256, IDR_MEDIA_APP_GALLERY_ICON_256_PNG},
      },
      *info);
  info->theme_color = 0xff202124;
  info->background_color = 0xff3c4043;
  info->display_mode = blink::mojom::DisplayMode::kStandalone;
  info->user_display_mode = blink::mojom::DisplayMode::kStandalone;

  // Add handlers for image+video and audio. We keep them separate since their
  // UX are sufficiently different (we don't want audio files to have a carousel
  // since this would be a second layer of navigation in conjunction with the
  // play queue). Order matters here; the Files app will prefer earlier
  // handlers.
  apps::FileHandler image_video_handler;
  image_video_handler.action = GURL(ash::kChromeUIMediaAppURL);
  image_video_handler.accept = MakeFileHandlerAccept(kFileHandlers);
  info->file_handlers.push_back(std::move(image_video_handler));
  apps::FileHandler audio_handler;
  audio_handler.action = GURL(ash::kChromeUIMediaAppURL);
  audio_handler.accept = MakeFileHandlerAccept(kAudioFileHandlers);
  info->file_handlers.push_back(std::move(audio_handler));
  return info;
}

std::unique_ptr<WebApplicationInfo> MediaSystemAppDelegate::GetWebAppInfo()
    const {
  return CreateWebAppInfoForMediaWebApp();
}

bool MediaSystemAppDelegate::ShouldIncludeLaunchDirectory() const {
  return true;
}

bool MediaSystemAppDelegate::ShouldShowInLauncher() const {
  return false;
}

bool MediaSystemAppDelegate::ShouldCaptureNavigations() const {
  return true;
}

bool MediaSystemAppDelegate::ShouldShowInSearch() const {
  return false;
}

bool MediaSystemAppDelegate::ShouldShowNewWindowMenuOption() const {
  return base::FeatureList::IsEnabled(
      chromeos::features::kMediaAppHandlesAudio);
}

bool MediaSystemAppDelegate::ShouldBeSingleWindow() const {
  return !ShouldShowNewWindowMenuOption();
}
