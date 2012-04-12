// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/resource/resource_bundle.h"

#include "base/file_path.h"
#include "base/logging.h"
#include "base/string16.h"
#include "base/string_piece.h"
#include "base/synchronization/lock.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/data_pack.h"
#include "ui/gfx/font.h"

namespace ui {

void ResourceBundle::LoadTestResources(const FilePath& path) {
  // Use the given resource pak for both common and localized resources.
  scoped_ptr<DataPack> data_pack(new DataPack());
  if (data_pack->Load(path))
    data_packs_.push_back(data_pack.release());

  data_pack.reset(new DataPack());
  if (data_pack->Load(path))
    locale_resources_data_.reset(data_pack.release());
}

}  // namespace ui
