// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/default_app_order.h"

#include <utility>

#include "ash/constants/ash_paths.h"
#include "ash/public/cpp/app_list/internal_app_id_constants.h"
#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/cxx17_backports.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_file_value_serializer.h"
#include "base/path_service.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "chrome/browser/ash/file_manager/app_id.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/app_list/arc/arc_app_utils.h"
#include "chrome/browser/ui/app_list/page_break_constants.h"
#include "chrome/browser/web_applications/web_app_id_constants.h"
#include "chrome/common/extensions/extension_constants.h"
#include "extensions/common/constants.h"

namespace chromeos {
namespace default_app_order {
namespace {

// The single ExternalLoader instance.
ExternalLoader* loader_instance = NULL;

// Names used in JSON file.
const char kOemAppsFolderAttr[] = "oem_apps_folder";
const char kLocalizedContentAttr[] = "localized_content";
const char kDefaultAttr[] = "default";
const char kNameAttr[] = "name";
const char kImportDefaultOrderAttr[] = "import_default_order";

// Canonical ordering specified in: go/default-apps
const char* const kDefaultAppOrder[] = {
    extension_misc::kChromeAppId,
    arc::kPlayStoreAppId,

    extension_misc::kFilesManagerAppId,
    file_manager::kFileManagerSwaAppId,

    arc::kGmailAppId,
    extension_misc::kGmailAppId,
    web_app::kGmailAppId,

    web_app::kGoogleMeetAppId,

    web_app::kGoogleChatAppId,

    extension_misc::kGoogleDocsAppId,
    web_app::kGoogleDocsAppId,

    extension_misc::kGoogleSlidesAppId,
    web_app::kGoogleSlidesAppId,

    extension_misc::kGoogleSheetsAppId,
    web_app::kGoogleSheetsAppId,

    extension_misc::kGoogleDriveAppId,
    web_app::kGoogleDriveAppId,

    extension_misc::kGoogleKeepAppId,
    web_app::kGoogleKeepAppId,

    arc::kGoogleCalendarAppId,
    extension_misc::kCalendarAppId,
    web_app::kGoogleCalendarAppId,

    arc::kYoutubeAppId,
    extension_misc::kYoutubeAppId,
    web_app::kYoutubeAppId,

    arc::kYoutubeMusicAppId,
    web_app::kYoutubeMusicAppId,
    arc::kYoutubeMusicWebApkAppId,

    arc::kPlayMoviesAppId,
    extension_misc::kGooglePlayMoviesAppId,

    arc::kPlayMusicAppId,
    extension_misc::kGooglePlayMusicAppId,

    arc::kPlayBooksAppId,
    extension_misc::kGooglePlayBooksAppId,
    web_app::kPlayBooksAppId,

    web_app::kCameraAppId,

    arc::kGooglePhotosAppId,
    extension_misc::kGooglePhotosAppId,

    arc::kGoogleDuoAppId,
    web_app::kStadiaAppId,

    // First default page break
    app_list::kDefaultPageBreak1,

    arc::kGoogleMapsAppId,
    extension_misc::kGoogleMapsAppId,  // TODO(crbug.com/976578): Remove.
    web_app::kGoogleMapsAppId,

    ash::kInternalAppIdSettings,
    web_app::kSettingsAppId,
    web_app::kOsSettingsAppId,

    web_app::kHelpAppId,

    web_app::kCalculatorAppId,
    extension_misc::kCalculatorAppId,

    web_app::kCursiveAppId,
    web_app::kCanvasAppId,
    extension_misc::kTextEditorAppId,
    web_app::kYoutubeTVAppId,
    web_app::kGoogleNewsAppId,
    extensions::kWebStoreAppId,
    arc::kLightRoomAppId,
    arc::kInfinitePainterAppId,
    web_app::kShowtimeAppId,
    extension_misc::kGooglePlusAppId,
};

// Reads external ordinal json file and returned the parsed value. Returns NULL
// if the file does not exist or could not be parsed properly. Caller takes
// ownership of the returned value.
std::unique_ptr<base::ListValue> ReadExternalOrdinalFile(
    const base::FilePath& path) {
  if (!base::PathExists(path))
    return NULL;

  JSONFileValueDeserializer deserializer(path);
  std::string error_msg;
  std::unique_ptr<base::Value> value =
      deserializer.Deserialize(NULL, &error_msg);
  if (!value) {
    LOG(WARNING) << "Unable to deserialize default app ordinals json data:"
        << error_msg << ", file=" << path.value();
    return NULL;
  }

  std::unique_ptr<base::ListValue> ordinal_list_value =
      base::ListValue::From(std::move(value));
  if (!ordinal_list_value)
    LOG(WARNING) << "Expect a JSON list in file " << path.value();

  return ordinal_list_value;
}

std::string GetLocaleSpecificStringImpl(
    const base::DictionaryValue* root,
    const std::string& locale,
    const std::string& dictionary_name,
    const std::string& entry_name) {
  const base::DictionaryValue* dictionary_content = NULL;
  if (!root || !root->GetDictionary(dictionary_name, &dictionary_content))
    return std::string();

  const base::DictionaryValue* locale_dictionary = NULL;
  if (dictionary_content->GetDictionary(locale, &locale_dictionary)) {
    std::string result;
    if (locale_dictionary->GetString(entry_name, &result))
      return result;
  }

  const base::DictionaryValue* default_dictionary = NULL;
  if (dictionary_content->GetDictionary(kDefaultAttr, &default_dictionary)) {
    std::string result;
    if (default_dictionary->GetString(entry_name, &result))
      return result;
  }

  return std::string();
}

// Gets built-in default app order.
void GetDefault(std::vector<std::string>* app_ids) {
  for (size_t i = 0; i < base::size(kDefaultAppOrder); ++i)
    app_ids->push_back(std::string(kDefaultAppOrder[i]));
}

}  // namespace

const size_t kDefaultAppOrderCount = base::size(kDefaultAppOrder);

ExternalLoader::ExternalLoader(bool async)
    : loaded_(base::WaitableEvent::ResetPolicy::MANUAL,
              base::WaitableEvent::InitialState::NOT_SIGNALED) {
  DCHECK(!loader_instance);
  loader_instance = this;

  if (async) {
    base::ThreadPool::PostTask(
        FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
        base::BindOnce(&ExternalLoader::Load, base::Unretained(this)));
  } else {
    Load();
  }
}

ExternalLoader::~ExternalLoader() {
  DCHECK(loaded_.IsSignaled());
  DCHECK_EQ(loader_instance, this);
  loader_instance = NULL;
}

const std::vector<std::string>& ExternalLoader::GetAppIds() {
  if (!loaded_.IsSignaled())
    LOG(ERROR) << "GetAppIds() called before loaded.";
  return app_ids_;
}

const std::string& ExternalLoader::GetOemAppsFolderName() {
  if (!loaded_.IsSignaled())
    LOG(ERROR) << "GetOemAppsFolderName() called before loaded.";
  return oem_apps_folder_name_;
}

void ExternalLoader::Load() {
  base::FilePath ordinals_file;
  CHECK(
      base::PathService::Get(chromeos::FILE_DEFAULT_APP_ORDER, &ordinals_file));

  std::unique_ptr<base::ListValue> ordinals_value =
      ReadExternalOrdinalFile(ordinals_file);
  if (ordinals_value) {
    std::string locale = g_browser_process->GetApplicationLocale();
    for (const base::Value& i : ordinals_value->GetList()) {
      const base::DictionaryValue* dict = nullptr;
      if (i.is_string()) {
        std::string app_id = i.GetString();
        app_ids_.push_back(app_id);
      } else if (i.GetAsDictionary(&dict)) {
        if (dict->FindBoolPath(kOemAppsFolderAttr).value_or(false)) {
          oem_apps_folder_name_ = GetLocaleSpecificStringImpl(
              dict, locale, kLocalizedContentAttr, kNameAttr);
        } else if (dict->FindBoolPath(kImportDefaultOrderAttr)
                       .value_or(false)) {
          GetDefault(&app_ids_);
        } else {
          LOG(ERROR) << "Invalid syntax in default_app_order.json";
        }
      } else {
        LOG(ERROR) << "Invalid entry in default_app_order.json";
      }
    }
  } else {
    GetDefault(&app_ids_);
  }

  loaded_.Signal();
}

void Get(std::vector<std::string>* app_ids) {
  // |loader_instance| could be NULL for test.
  if (!loader_instance) {
    GetDefault(app_ids);
    return;
  }

  *app_ids = loader_instance->GetAppIds();
}

std::string GetOemAppsFolderName() {
  // |loader_instance| could be NULL for test.
  if (!loader_instance)
    return std::string();
  else
    return loader_instance->GetOemAppsFolderName();
}

}  // namespace default_app_order
}  // namespace chromeos
