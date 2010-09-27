// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_PLUGINS_PEPPER_FILE_SYSTEM_H_
#define WEBKIT_GLUE_PLUGINS_PEPPER_FILE_SYSTEM_H_

#include "base/basictypes.h"

struct PPB_FileSystem_Dev;

namespace pepper {

class FileSystem {
 public:
  // Returns a pointer to the interface implementing PPB_FileSystem that is
  // exposed to the plugin.
  static const PPB_FileSystem_Dev* GetInterface();

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(FileSystem);
};

}  // namespace pepper

#endif  // WEBKIT_GLUE_PLUGINS_PEPPER_FILE_SYSTEM_H_
