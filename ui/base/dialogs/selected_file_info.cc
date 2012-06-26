// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/dialogs/selected_file_info.h"

namespace ui {

SelectedFileInfo::SelectedFileInfo() {}

SelectedFileInfo::SelectedFileInfo(const FilePath& in_path,
                                   const FilePath::StringType& in_display_name)
    : path(in_path),
      display_name(in_display_name) {
}

SelectedFileInfo::~SelectedFileInfo() {}

}  // namespace ui
