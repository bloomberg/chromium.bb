// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/color_profile.h"

#include <windows.h>
#include <stddef.h>
#include <map>

#include "base/files/file_util.h"
#include "base/lazy_instance.h"
#include "base/macros.h"
#include "base/synchronization/lock.h"

namespace gfx {

namespace {

void ReadBestMonitorColorProfile(std::vector<char>* profile) {
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
  if (!ColorProfile::IsValidProfileLength(length))
    return;
  profile->assign(profileData.data(), profileData.data() + length);
}

base::LazyInstance<base::Lock> g_best_color_profile_lock =
    LAZY_INSTANCE_INITIALIZER;
base::LazyInstance<gfx::ColorProfile> g_best_color_profile =
    LAZY_INSTANCE_INITIALIZER;
bool g_has_initialized_best_color_profile = false;

}  // namespace

// static
ColorProfile ColorProfile::GetFromBestMonitor() {
  base::AutoLock lock(g_best_color_profile_lock.Get());
  return g_best_color_profile.Get();
}

// static
bool ColorProfile::CachedProfilesNeedUpdate() {
  base::AutoLock lock(g_best_color_profile_lock.Get());
  return !g_has_initialized_best_color_profile;
}

// static
void ColorProfile::UpdateCachedProfilesOnBackgroundThread() {
  std::vector<char> profile;
  ReadBestMonitorColorProfile(&profile);

  base::AutoLock lock(g_best_color_profile_lock.Get());
  g_best_color_profile.Get().profile_ = profile;
  g_has_initialized_best_color_profile = true;
}

}  // namespace gfx
