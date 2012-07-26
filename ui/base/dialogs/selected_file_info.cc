// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/dialogs/selected_file_info.h"

namespace ui {

SelectedFileInfo::SelectedFileInfo() {}

SelectedFileInfo::SelectedFileInfo(const FilePath& in_file_path,
                                   const FilePath& in_local_path)
    : file_path(in_file_path),
      local_path(in_local_path) {
  if (local_path.empty())
    local_path = file_path;
  display_name = in_file_path.BaseName().value();
}

SelectedFileInfo::~SelectedFileInfo() {}

}  // namespace ui
