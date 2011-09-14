// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/resource/resource_bundle.h"

#include "base/file_path.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "base/string16.h"
#include "base/string_piece.h"
#include "base/synchronization/lock.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/data_pack.h"
#include "ui/gfx/font.h"

namespace ui {

ResourceBundle::~ResourceBundle() {
  FreeImages();
  UnloadLocaleResources();
  STLDeleteContainerPointers(data_packs_.begin(),
                             data_packs_.end());
  delete resources_data_;
  resources_data_ = NULL;
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

void ResourceBundle::LoadCommonResources() {
  DCHECK(!resources_data_) << "chrome.pak already loaded";
  FilePath resources_file_path = GetResourcesFilePath();
  CHECK(!resources_file_path.empty()) << "chrome.pak not found";
  resources_data_ = LoadResourcesDataPak(resources_file_path);
  CHECK(resources_data_) << "failed to load chrome.pak";

  FilePath large_icon_resources_file_path = GetLargeIconResourcesFilePath();
  if (!large_icon_resources_file_path.empty()) {
    large_icon_resources_data_ =
        LoadResourcesDataPak(large_icon_resources_file_path);
    CHECK(large_icon_resources_data_) <<
        "failed to load theme_resources_large.pak";
  }
}

void ResourceBundle::LoadTestResources(const FilePath& path) {
  DCHECK(!resources_data_) << "resource already loaded";

  // Use the given resource pak for both common and localized resources.
  resources_data_ = LoadResourcesDataPak(path);
  locale_resources_data_.reset(LoadResourcesDataPak(path));
}

}  // namespace ui
