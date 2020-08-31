// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_DRAGDROP_FILE_INFO_FILE_INFO_H_
#define UI_BASE_DRAGDROP_FILE_INFO_FILE_INFO_H_

#include "base/component_export.h"
#include "base/files/file_path.h"

namespace ui {

// struct that bundles a file's path with an optional display name.
struct COMPONENT_EXPORT(UI_BASE_DRAGDROP_FILE_INFO) FileInfo {
  FileInfo();
  FileInfo(const base::FilePath& path, const base::FilePath& display_name);
  ~FileInfo();
  bool operator==(const FileInfo& other) const;

  base::FilePath path;
  base::FilePath display_name;  // Optional.
};

}  // namespace ui

#endif  // UI_BASE_DRAGDROP_FILE_INFO_FILE_INFO_H_
