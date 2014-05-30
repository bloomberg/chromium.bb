// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/color_profile.h"

#include <map>
#include <windows.h>

#include "base/file_util.h"
#include "base/lazy_instance.h"
#include "base/synchronization/lock.h"

namespace gfx {

class ColorProfileCache {
 public:
  // A thread-safe cache of color profiles keyed by windows device name.
  ColorProfileCache() {}

  bool Find(const std::wstring& device, std::vector<char>* profile) {
    base::AutoLock lock(lock_);
    DeviceColorProfile::const_iterator it = cache_.find(device);
    if (it == cache_.end())
      return false;
    *profile = it->second;
    return true;
  }

  void Insert(const std::wstring& device, const std::vector<char>& profile) {
    base::AutoLock lock(lock_);
    cache_[device] = profile;
  }

  bool Erase(const std::wstring& device) {
    base::AutoLock lock(lock_);
    DeviceColorProfile::iterator it = cache_.find(device);
    if (it == cache_.end())
      return false;
    cache_.erase(device);
    return true;
  }

  void Clear() {
    base::AutoLock lock(lock_);
    cache_.clear();
  }

 private:
  typedef std::map<std::wstring, std::vector<char> > DeviceColorProfile;

  DeviceColorProfile cache_;
  base::Lock lock_;

  DISALLOW_COPY_AND_ASSIGN(ColorProfileCache);
};

bool GetDisplayColorProfile(const gfx::Rect& bounds,
                            std::vector<char>* profile) {
  if (bounds.IsEmpty())
    return false;
  // TODO(noel): implement.
  return false;
}

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
