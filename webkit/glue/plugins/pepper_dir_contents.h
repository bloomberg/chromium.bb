// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_PLUGINS_PEPPER_DIR_CONTENTS_H_
#define WEBKIT_GLUE_PLUGINS_PEPPER_DIR_CONTENTS_H_

#include <vector>
#include "base/file_path.h"

struct PepperDirEntry {
  FilePath name;
  bool is_dir;
};

typedef std::vector<PepperDirEntry> PepperDirContents;

#endif  // WEBKIT_GLUE_PLUGINS_PEPPER_DIR_CONTENTS_H_
