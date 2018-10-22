// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_CLEANER_OS_WHITELISTED_DIRECTORY_H_
#define CHROME_CHROME_CLEANER_OS_WHITELISTED_DIRECTORY_H_

#include "base/files/file_path.h"
#include "base/memory/singleton.h"
#include "chrome/chrome_cleaner/os/file_path_set.h"

namespace chrome_cleaner {

class WhitelistedDirectory {
 public:
  static WhitelistedDirectory* GetInstance();

  ~WhitelistedDirectory();

  // Disables caching, used in unit tests. Tests sometimes override whitelisted
  // directories and caching them causes problems.
  void DisableCache();

  // Ensures path is not in a whitelist that should not be removed.
  bool IsWhitelistedDirectory(const base::FilePath& path);

 protected:
  WhitelistedDirectory();

 private:
  friend struct base::DefaultSingletonTraits<WhitelistedDirectory>;

  void GenerateDirectoryWhitelist();

  bool cache_disabled_ = false;
  UnorderedFilePathSet whitelisted_directories_;
};

}  // namespace chrome_cleaner

#endif  // CHROME_CHROME_CLEANER_OS_WHITELISTED_DIRECTORY_H_
