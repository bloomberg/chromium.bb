// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_PLUGINS_PPAPI_FILE_PATH_H_
#define WEBKIT_PLUGINS_PPAPI_FILE_PATH_H_

#include <string>

#include "base/file_path.h"

namespace webkit {
namespace ppapi {

class PluginModule;

// TODO(vtl): Once we put |::FilePath| into the |base| namespace, get rid of the
// |Pepper| (or |PEPPER_|) prefixes. Right now, it's just too
// confusing/dangerous!

class PepperFilePath {
 public:
  enum Domain {
    DOMAIN_INVALID = 0,
    DOMAIN_ABSOLUTE,
    DOMAIN_MODULE_LOCAL,

    // Used for validity-checking.
    DOMAIN_MAX_VALID = DOMAIN_MODULE_LOCAL
  };

  PepperFilePath();
  PepperFilePath(Domain d, FilePath p);

  static PepperFilePath MakeAbsolute(const FilePath& path);
  static PepperFilePath MakeModuleLocal(PluginModule* module,
                                        const char* utf8_path);

  Domain domain() const { return domain_; }
  const FilePath& path() const { return path_; }

 private:
  Domain domain_;
  FilePath path_;
};

}  // namespace ppapi
}  // namespace webkit

#endif  // WEBKIT_PLUGINS_PPAPI_FILE_PATH_H_
