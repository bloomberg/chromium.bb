// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/convert_web_app.h"

#include <stddef.h>
#include <stdint.h>

#include <limits>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/json/json_file_value_serializer.h"
#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/extensions/api/url_handlers/url_handlers_parser.h"
#include "chrome/common/extensions/manifest_handlers/app_theme_color_info.h"
#include "chrome/common/web_application_info.h"
#include "crypto/sha2.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/file_util.h"
#include "extensions/common/image_util.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/manifest_handlers/file_handler_info.h"
#include "net/base/url_util.h"
#include "third_party/blink/public/common/manifest/manifest_util.h"
#include "third_party/blink/public/mojom/manifest/display_mode.mojom.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/geometry/safe_integer_conversions.h"
#include "url/gurl.h"

namespace extensions {

namespace keys = manifest_keys;

namespace {
const char kIconsDirName[] = "icons";
const char kScopeUrlHandlerId[] = "scope";

std::unique_ptr<base::DictionaryValue> CreateFileHandlersForBookmarkApp(
    const std::vector<blink::Manifest::FileHandler>& manifest_file_handlers) {
  base::Value file_handlers(base::Value::Type::DICTIONARY);

  for (const auto& entry : manifest_file_handlers) {
    base::Value file_handler(base::Value::Type::DICTIONARY);
    file_handler.SetKey(keys::kFileHandlerIncludeDirectories,
                        base::Value(false));
    file_handler.SetKey(keys::kFileHandlerVerb,
                        base::Value(apps::file_handler_verbs::kOpenWith));

    base::Value mime_types(base::Value::Type::LIST);
    base::Value file_extensions(base::Value::Type::LIST);

    for (const auto& it : entry.accept) {
      std::string type = base::UTF16ToUTF8(it.first);
      if (type.empty())
        continue;

      mime_types.Append(base::Value(type));
      for (const auto& extensionUTF16 : it.second) {
        std::string extension = base::UTF16ToUTF8(extensionUTF16);
        if (extension.empty())
          continue;

        if (extension[0] != '.')
          continue;

        // Remove the '.' before appending.
        file_extensions.Append(base::Value(extension.substr(1)));
      }
    }

    file_handler.SetKey(keys::kFileHandlerTypes, std::move(mime_types));
    file_handler.SetKey(keys::kFileHandlerExtensions,
                        std::move(file_extensions));

    file_handlers.SetKey(entry.action.spec(), std::move(file_handler));
  }

  return base::DictionaryValue::From(
      base::Value::ToUniquePtrValue(std::move(file_handlers)));
}

}  // namespace

std::unique_ptr<base::DictionaryValue> CreateURLHandlersForBookmarkApp(
    const GURL& scope_url,
    const base::string16& title) {
  auto matches = std::make_unique<base::ListValue>();
  matches->AppendString(scope_url.GetOrigin().Resolve(scope_url.path()).spec() +
                        "*");

  auto scope_handler = std::make_unique<base::DictionaryValue>();
  scope_handler->SetList(keys::kMatches, std::move(matches));
  // The URL handler title is not used anywhere but we set it to the
  // web app's title just in case.
  scope_handler->SetString(keys::kUrlHandlerTitle, base::UTF16ToUTF8(title));

  auto url_handlers = std::make_unique<base::DictionaryValue>();
  // Use "scope" as the url handler's identifier.
  url_handlers->SetDictionary(kScopeUrlHandlerId, std::move(scope_handler));
  return url_handlers;
}

GURL GetScopeURLFromBookmarkApp(const Extension* extension) {
  DCHECK(extension->from_bookmark());
  const std::vector<UrlHandlerInfo>* url_handlers =
      UrlHandlers::GetUrlHandlers(extension);
  if (!url_handlers)
    return GURL();

  // A Bookmark app created by us should only have a url_handler with id
  // kScopeUrlHandlerId. This URL handler should have a single pattern which
  // corresponds to the web manifest's scope. The URL handler's pattern should
  // be the Web Manifest's scope's origin + path with a wildcard, '*', appended
  // to it.
  auto handler_it = std::find_if(
      url_handlers->begin(), url_handlers->end(),
      [](const UrlHandlerInfo& info) { return info.id == kScopeUrlHandlerId; });
  if (handler_it == url_handlers->end()) {
    return GURL();
  }

  const auto& patterns = handler_it->patterns;
  DCHECK(patterns.size() == 1);
  const auto& pattern_iter = patterns.begin();
  // Remove the '*' character at the end (which was added when creating the URL
  // handler, see CreateURLHandlersForBookmarkApp()).
  const std::string& pattern_str = pattern_iter->GetAsString();
  DCHECK_EQ(pattern_str.back(), '*');
  return GURL(pattern_str.substr(0, pattern_str.size() - 1));
}

// Generates a version for the converted app using the current date. This isn't
// really needed, but it seems like useful information.
std::string ConvertTimeToExtensionVersion(const base::Time& create_time) {
  base::Time::Exploded create_time_exploded;
  create_time.UTCExplode(&create_time_exploded);

  double micros = static_cast<double>(
      (create_time_exploded.millisecond *
       base::Time::kMicrosecondsPerMillisecond) +
      (create_time_exploded.second * base::Time::kMicrosecondsPerSecond) +
      (create_time_exploded.minute * base::Time::kMicrosecondsPerMinute) +
      (create_time_exploded.hour * base::Time::kMicrosecondsPerHour));
  double day_fraction = micros / base::Time::kMicrosecondsPerDay;
  int stamp =
      gfx::ToRoundedInt(day_fraction * std::numeric_limits<uint16_t>::max());

  return base::StringPrintf("%i.%i.%i.%i", create_time_exploded.year,
                            create_time_exploded.month,
                            create_time_exploded.day_of_month, stamp);
}

scoped_refptr<Extension> ConvertWebAppToExtension(
    const WebApplicationInfo& web_app,
    const base::Time& create_time,
    const base::FilePath& extensions_dir,
    int extra_creation_flags,
    Manifest::Location install_source) {
  base::FilePath install_temp_dir =
      file_util::GetInstallTempDir(extensions_dir);
  if (install_temp_dir.empty()) {
    LOG(ERROR) << "Could not get path to profile temporary directory.";
    return nullptr;
  }

  base::ScopedTempDir temp_dir;
  if (!temp_dir.CreateUniqueTempDirUnderPath(install_temp_dir)) {
    LOG(ERROR) << "Could not create temporary directory.";
    return nullptr;
  }

  // Create the manifest
  std::unique_ptr<base::DictionaryValue> root(new base::DictionaryValue);
  root->SetString(keys::kPublicKey,
                  web_app::GenerateAppKeyFromURL(web_app.app_url));
  root->SetString(keys::kName, base::UTF16ToUTF8(web_app.title));
  root->SetString(keys::kVersion, ConvertTimeToExtensionVersion(create_time));
  root->SetString(keys::kDescription, base::UTF16ToUTF8(web_app.description));
  root->SetString(keys::kLaunchWebURL, web_app.app_url.spec());
  if (web_app.generated_icon_color != SK_ColorTRANSPARENT) {
    root->SetString(keys::kAppIconColor, image_util::GenerateHexColorString(
                                             web_app.generated_icon_color));
  }

  if (web_app.theme_color) {
    root->SetString(keys::kAppThemeColor, color_utils::SkColorToRgbaString(
                                              web_app.theme_color.value()));
  }

  if (!web_app.scope.is_empty()) {
    root->SetDictionary(keys::kUrlHandlers, CreateURLHandlersForBookmarkApp(
                                                web_app.scope, web_app.title));
  }

  DCHECK_NE(blink::mojom::DisplayMode::kUndefined, web_app.display_mode);
  root->SetString(keys::kAppDisplayMode,
                  blink::DisplayModeToString(web_app.display_mode));

  if (web_app.file_handlers.size() != 0) {
    root->SetDictionary(keys::kFileHandlers, CreateFileHandlersForBookmarkApp(
                                                 web_app.file_handlers));
  }

  // Add the icons and linked icon information.
  auto linked_icons = std::make_unique<base::ListValue>();
  for (const WebApplicationIconInfo& icon_info : web_app.icon_infos) {
    DCHECK(icon_info.url.is_valid());
    std::unique_ptr<base::DictionaryValue> linked_icon(
        new base::DictionaryValue());
    linked_icon->SetString(keys::kLinkedAppIconURL, icon_info.url.spec());
    linked_icon->SetInteger(keys::kLinkedAppIconSize, icon_info.square_size_px);
    linked_icons->Append(std::move(linked_icon));
  }
  auto icons = std::make_unique<base::DictionaryValue>();
  for (const std::pair<SquareSizePx, SkBitmap>& icon : web_app.icon_bitmaps) {
    std::string size = base::StringPrintf("%i", icon.first);
    std::string icon_path = base::StringPrintf("%s/%s.png", kIconsDirName,
                                               size.c_str());
    icons->SetString(size, icon_path);
  }
  root->Set(keys::kIcons, std::move(icons));
  root->Set(keys::kLinkedAppIcons, std::move(linked_icons));

  // Write the manifest.
  base::FilePath manifest_path = temp_dir.GetPath().Append(kManifestFilename);
  JSONFileValueSerializer serializer(manifest_path);
  if (!serializer.Serialize(*root)) {
    LOG(ERROR) << "Could not serialize manifest.";
    return nullptr;
  }

  // Write the icon files.
  base::FilePath icons_dir = temp_dir.GetPath().AppendASCII(kIconsDirName);
  if (!base::CreateDirectory(icons_dir)) {
    LOG(ERROR) << "Could not create icons directory.";
    return nullptr;
  }
  for (const std::pair<SquareSizePx, SkBitmap>& icon : web_app.icon_bitmaps) {
    DCHECK_NE(icon.second.colorType(), kUnknown_SkColorType);

    base::FilePath icon_file =
        icons_dir.AppendASCII(base::StringPrintf("%i.png", icon.first));
    std::vector<unsigned char> image_data;
    if (!gfx::PNGCodec::EncodeBGRASkBitmap(icon.second, false, &image_data)) {
      LOG(ERROR) << "Could not create icon file.";
      return nullptr;
    }

    const char* image_data_ptr = reinterpret_cast<const char*>(&image_data[0]);
    int size = base::checked_cast<int>(image_data.size());
    if (base::WriteFile(icon_file, image_data_ptr, size) != size) {
      LOG(ERROR) << "Could not write icon file.";
      return nullptr;
    }
  }

  // Finally, create the extension object to represent the unpacked directory.
  std::string error;
  scoped_refptr<Extension> extension = Extension::Create(
      temp_dir.GetPath(), install_source, *root,
      Extension::FROM_BOOKMARK | extra_creation_flags, &error);
  if (!extension.get()) {
    LOG(ERROR) << error;
    return nullptr;
  }

  temp_dir.Take();  // The caller takes ownership of the directory.
  return extension;
}

}  // namespace extensions
