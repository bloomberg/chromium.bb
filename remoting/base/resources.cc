// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/resources.h"

#include "base/file_path.h"
#include "base/path_service.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_paths.h"

namespace remoting {

// Loads chromoting resources.
bool LoadResources(const std::string& locale) {
  FilePath path;
  if (!PathService::Get(base::DIR_MODULE, &path))
    return false;
  path = path.Append(FILE_PATH_LITERAL("remoting_locales"));
  PathService::Override(ui::DIR_LOCALES, path);

  ui::ResourceBundle::InitSharedInstanceLocaleOnly(locale, NULL);

  return true;
}

}  // namespace remoting
