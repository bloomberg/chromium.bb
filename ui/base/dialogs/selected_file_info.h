// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_DIALOGS_SELECTED_FILE_INFO_H_
#define UI_BASE_DIALOGS_SELECTED_FILE_INFO_H_
#pragma once

#include <vector>

#include "base/file_path.h"
#include "base/string16.h"
#include "ui/base/ui_export.h"

namespace ui {

// Struct used for passing selected file info to WebKit.
struct UI_EXPORT SelectedFileInfo {
  // The real path to the selected file. This can be a snapshot file with a
  // human unreadable name like /blah/.d41d8cd98f00b204e9800998ecf8427e.
  FilePath path;

  // This field is optional. The display name contains only the base name
  // portion of a file name (ex. no path separators), and used for displaying
  // selected file names. If this field is empty, the base name portion of
  // |path| is used for displaying.
  FilePath::StringType display_name;

  SelectedFileInfo() {}
  SelectedFileInfo(const FilePath& in_path,
                   const FilePath::StringType& in_display_name)
      : path(in_path),
        display_name(in_display_name) {
  }
};

}  // namespace ui

#endif  // UI_BASE_DIALOGS_SELECTED_FILE_INFO_H_
