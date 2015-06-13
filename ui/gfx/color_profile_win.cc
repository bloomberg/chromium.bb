// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/color_profile.h"

#include <windows.h>
#include <map>

#include "base/files/file_util.h"
#include "base/lazy_instance.h"
#include "base/synchronization/lock.h"

namespace gfx {

void ReadColorProfile(std::vector<char>* profile) {
  // TODO: support multiple monitors.
  HDC screen_dc = GetDC(NULL);
  DWORD path_len = MAX_PATH;
  WCHAR path[MAX_PATH + 1];

  BOOL result = GetICMProfile(screen_dc, &path_len, path);
  ReleaseDC(NULL, screen_dc);
  if (!result)
    return;
  std::string profileData;
  if (!base::ReadFileToString(base::FilePath(path), &profileData))
    return;
  size_t length = profileData.size();
  if (gfx::InvalidColorProfileLength(length))
    return;
  profile->assign(profileData.data(), profileData.data() + length);
}

}  // namespace gfx
