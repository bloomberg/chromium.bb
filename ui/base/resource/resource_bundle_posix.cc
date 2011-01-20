// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/resource/resource_bundle.h"

#include "app/l10n_util.h"
#include "base/lock.h"
#include "base/logging.h"
#include "base/stl_util-inl.h"
#include "base/string16.h"
#include "base/string_piece.h"
#include "gfx/font.h"
#include "ui/base/resource/data_pack.h"

namespace ui {

namespace {

DataPack* LoadResourcesDataPak(FilePath resources_pak_path) {
  DataPack* resources_pak = new DataPack;
  bool success = resources_pak->Load(resources_pak_path);
  if (!success) {
    delete resources_pak;
    resources_pak = NULL;
  }
  return resources_pak;
}

}  // namespace

ResourceBundle::~ResourceBundle() {
  FreeImages();
#if defined(OS_POSIX) && !defined(OS_MACOSX)
  FreeGdkPixBufs();
#endif
  UnloadLocaleResources();
  STLDeleteContainerPointers(data_packs_.begin(),
                             data_packs_.end());
  delete resources_data_;
  resources_data_ = NULL;
}

void ResourceBundle::UnloadLocaleResources() {
  delete locale_resources_data_;
  locale_resources_data_ = NULL;
}

// static
RefCountedStaticMemory* ResourceBundle::LoadResourceBytes(
    DataHandle module, int resource_id) {
  DCHECK(module);
  return module->GetStaticMemory(resource_id);
}

base::StringPiece ResourceBundle::GetRawDataResource(int resource_id) const {
  DCHECK(resources_data_);
  base::StringPiece data;
  if (!resources_data_->GetStringPiece(resource_id, &data)) {
    if (!locale_resources_data_->GetStringPiece(resource_id, &data)) {
      for (size_t i = 0; i < data_packs_.size(); ++i) {
        if (data_packs_[i]->GetStringPiece(resource_id, &data))
          return data;
      }

      return base::StringPiece();
    }
  }
  return data;
}

string16 ResourceBundle::GetLocalizedString(int message_id) {
  // If for some reason we were unable to load a resource pak, return an empty
  // string (better than crashing).
  if (!locale_resources_data_) {
    LOG(WARNING) << "locale resources are not loaded";
    return string16();
  }

  base::StringPiece data;
  if (!locale_resources_data_->GetStringPiece(message_id, &data)) {
    // Fall back on the main data pack (shouldn't be any strings here except in
    // unittests).
    data = GetRawDataResource(message_id);
    if (data.empty()) {
      NOTREACHED() << "unable to find resource: " << message_id;
      return string16();
    }
  }

  // Data pack encodes strings as UTF16.
  DCHECK_EQ(data.length() % 2, 0U);
  string16 msg(reinterpret_cast<const char16*>(data.data()),
               data.length() / 2);
  return msg;
}

void ResourceBundle::LoadCommonResources() {
  DCHECK(!resources_data_) << "chrome.pak already loaded";
  FilePath resources_file_path = GetResourcesFilePath();
  CHECK(!resources_file_path.empty()) << "chrome.pak not found";
  resources_data_ = LoadResourcesDataPak(resources_file_path);
  CHECK(resources_data_) << "failed to load chrome.pak";
}

std::string ResourceBundle::LoadLocaleResources(
    const std::string& pref_locale) {
  DCHECK(!locale_resources_data_) << "locale.pak already loaded";
  std::string app_locale = l10n_util::GetApplicationLocale(pref_locale);
  FilePath locale_file_path = GetLocaleFilePath(app_locale);
  if (locale_file_path.empty()) {
    // It's possible that there is no locale.pak.
    NOTREACHED();
    return std::string();
  }
  locale_resources_data_ = LoadResourcesDataPak(locale_file_path);
  CHECK(locale_resources_data_) << "failed to load locale.pak";
  return app_locale;
}

}  // namespace ui
