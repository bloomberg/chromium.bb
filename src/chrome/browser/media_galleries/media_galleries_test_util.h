// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_GALLERIES_MEDIA_GALLERIES_TEST_UTIL_H_
#define CHROME_BROWSER_MEDIA_GALLERIES_MEDIA_GALLERIES_TEST_UTIL_H_

#include <memory>
#include <string>
#include <vector>

#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/test/scoped_path_override.h"
#include "build/build_config.h"

#if defined(OS_WIN)
#include "base/test/test_reg_util_win.h"
#endif

namespace extensions {
class Extension;
}

namespace registry_util {
class RegistryOverrideManager;
}

class Profile;

#if defined(OS_MACOSX)
class MockPreferences;
#endif

scoped_refptr<extensions::Extension> AddMediaGalleriesApp(
    const std::string& name,
    const std::vector<std::string>& media_galleries_permissions,
    Profile* profile);

class EnsureMediaDirectoriesExists {
 public:
  EnsureMediaDirectoriesExists();
  ~EnsureMediaDirectoriesExists();

  int num_galleries() const { return num_galleries_; }

  base::FilePath GetFakeAppDataPath() const;

  // Changes the directories for the media paths (music, pictures, videos)
  // overrides to new, different directories that are generated.
  void ChangeMediaPathOverrides();
#if defined(OS_WIN)
  base::FilePath GetFakeLocalAppDataPath() const;
#endif

 private:
  void Init();

  base::ScopedTempDir fake_dir_;

  int num_galleries_;

  int times_overrides_changed_;

  std::unique_ptr<base::ScopedPathOverride> app_data_override_;
  std::unique_ptr<base::ScopedPathOverride> music_override_;
  std::unique_ptr<base::ScopedPathOverride> pictures_override_;
  std::unique_ptr<base::ScopedPathOverride> video_override_;
#if defined(OS_WIN)
  std::unique_ptr<base::ScopedPathOverride> local_app_data_override_;

  registry_util::RegistryOverrideManager registry_override_;
#endif
#if defined(OS_MACOSX)
  std::unique_ptr<MockPreferences> mac_preferences_;
#endif

  DISALLOW_COPY_AND_ASSIGN(EnsureMediaDirectoriesExists);
};

extern base::FilePath MakeMediaGalleriesTestingPath(const std::string& dir);

#endif  // CHROME_BROWSER_MEDIA_GALLERIES_MEDIA_GALLERIES_TEST_UTIL_H_
