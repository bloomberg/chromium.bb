// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/dragdrop/file_info.h"

namespace ui {

FileInfo::FileInfo() {}

FileInfo::FileInfo(const base::FilePath& path,
                   const base::FilePath& display_name)
    : path(path), display_name(display_name) {}

FileInfo::~FileInfo() {}

}  // namespace ui
