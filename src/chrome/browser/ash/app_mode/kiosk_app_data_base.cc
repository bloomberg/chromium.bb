// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_mode/kiosk_app_data_base.h"

#include <utility>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_blocking_call.h"
#include "chrome/browser/browser_process.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "content/public/browser/browser_thread.h"
#include "ui/gfx/codec/png_codec.h"

using content::BrowserThread;

namespace ash {

namespace {

// Keys for local state data.
constexpr char kKeyName[] = "name";
constexpr char kKeyIcon[] = "icon";

// Icon file extension.
constexpr char kIconFileExtension[] = ".png";

// Save |raw_icon| for given |app_id|.
void SaveIconToLocalOnBlockingPool(const base::FilePath& icon_path,
                                   std::vector<unsigned char> image_data) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  const base::FilePath dir = icon_path.DirName();
  if (!base::PathExists(dir) && !base::CreateDirectory(dir)) {
    LOG(ERROR) << "Failed to create directory to store kiosk icons";
    return;
  }

  const int wrote = base::WriteFile(
      icon_path, reinterpret_cast<char*>(image_data.data()), image_data.size());
  if (wrote != static_cast<int>(image_data.size())) {
    LOG(ERROR) << "Failed to write kiosk icon file";
  }
}

void RemoveDictionaryPath(base::Value* dict, base::StringPiece path) {
  DCHECK(dict->is_dict());
  base::StringPiece current_path(path);
  base::Value* current_dictionary = dict;
  size_t delimiter_position = current_path.rfind('.');
  if (delimiter_position != base::StringPiece::npos) {
    current_dictionary =
        dict->FindPath(current_path.substr(0, delimiter_position));
    if (!current_dictionary)
      return;
    current_path = current_path.substr(delimiter_position + 1);
  }
  current_dictionary->RemoveKey(current_path);
}

}  // namespace

// static
const char KioskAppDataBase::kKeyApps[] = "apps";

KioskAppDataBase::KioskAppDataBase(const std::string& dictionary_name,
                                   const std::string& app_id,
                                   const AccountId& account_id)
    : dictionary_name_(dictionary_name),
      app_id_(app_id),
      account_id_(account_id) {}

KioskAppDataBase::~KioskAppDataBase() = default;

void KioskAppDataBase::SaveToDictionary(DictionaryPrefUpdate& dict_update) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  const std::string app_key = std::string(kKeyApps) + '.' + app_id_;
  const std::string name_key = app_key + '.' + kKeyName;
  const std::string icon_path_key = app_key + '.' + kKeyIcon;

  dict_update->SetStringPath(name_key, name_);
  dict_update->SetStringPath(icon_path_key, icon_path_.value());
}

void KioskAppDataBase::SaveIconToDictionary(DictionaryPrefUpdate& dict_update) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  const std::string app_key = std::string(kKeyApps) + '.' + app_id_;
  const std::string icon_path_key = app_key + '.' + kKeyIcon;

  dict_update->SetStringPath(icon_path_key, icon_path_.value());
}

bool KioskAppDataBase::LoadFromDictionary(const base::Value& dict,
                                          bool lazy_icon_load) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  const std::string app_key =
      std::string(KioskAppDataBase::kKeyApps) + '.' + app_id_;
  const std::string name_key = app_key + '.' + kKeyName;
  const std::string icon_path_key = app_key + '.' + kKeyIcon;

  // If there is no title stored, do not stop, sometimes only icon is cached.
  const std::string* maybe_name = dict.FindStringPath(name_key);
  if (maybe_name)
    name_ = *maybe_name;

  const std::string* icon_path_string = dict.FindStringPath(icon_path_key);
  if (!icon_path_string) {
    return false;
  }

  icon_path_ = base::FilePath(*icon_path_string);

  if (!lazy_icon_load) {
    DecodeIcon();
  }

  return true;
}

void KioskAppDataBase::DecodeIcon() {
  DCHECK(!icon_path_.empty());
  kiosk_app_icon_loader_ = std::make_unique<KioskAppIconLoader>(this);
  kiosk_app_icon_loader_->Start(icon_path_);
}

void KioskAppDataBase::SaveIcon(const SkBitmap& icon,
                                const base::FilePath& cache_dir) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  std::vector<unsigned char> image_data;
  if (!gfx::PNGCodec::EncodeBGRASkBitmap(icon, false, &image_data)) {
    LOG(ERROR) << "Failed to encode kiosk icon";
    return;
  }

  const base::FilePath icon_path =
      cache_dir.AppendASCII(app_id_).AddExtension(kIconFileExtension);
  base::ThreadPool::PostTask(FROM_HERE, {base::MayBlock()},
                             base::BindOnce(&SaveIconToLocalOnBlockingPool,
                                            icon_path, std::move(image_data)));

  icon_path_ = icon_path;
}

void KioskAppDataBase::ClearCache() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  PrefService* local_state = g_browser_process->local_state();

  DictionaryPrefUpdate dict_update(local_state, dictionary_name());

  const std::string app_key =
      std::string(KioskAppDataBase::kKeyApps) + '.' + app_id_;
  RemoveDictionaryPath(dict_update.Get(), app_key);

  if (!icon_path_.empty()) {
    base::ThreadPool::PostTask(
        FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
        base::BindOnce(base::GetDeleteFileCallback(), icon_path_));
  }
}

}  // namespace ash
