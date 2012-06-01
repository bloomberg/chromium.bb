// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_PLUGINS_PPAPI_DIR_CONTENTS_H_
#define WEBKIT_PLUGINS_PPAPI_DIR_CONTENTS_H_

#include <vector>

#include "base/file_path.h"

namespace webkit {
namespace ppapi {

struct DirEntry {
  FilePath name;
  bool is_dir;
};

typedef std::vector<DirEntry> DirContents;

}  // namespace ppapi
}  // namespace webkit

#endif  // WEBKIT_PLUGINS_PPAPI_DIR_CONTENTS_H_
