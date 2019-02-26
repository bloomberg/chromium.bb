// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_CLEANER_PUP_DATA_PUP_DISK_UTIL_H_
#define CHROME_CHROME_CLEANER_PUP_DATA_PUP_DISK_UTIL_H_

#include <vector>

#include "chrome/chrome_cleaner/pup_data/pup_data.h"

namespace base {
class FilePath;
}

namespace chrome_cleaner {

// Collect files and folders under |file_path| and add them to |pup|.
void CollectPathsRecursively(const base::FilePath& file_path,
                             PUPData::PUP* pup);

// Collect files and folders under |file_path| and add them to |pup|.
// |max_files| limits the number of files to be added to |pup|. Returns false if
// a file is bigger than |max_filesize| a file has a greater size than
// |max_filesize| or if a folder is found when |allow_folders| is false. |pup|
// is left unchanged when this function returns false.
bool CollectPathsRecursivelyWithLimits(const base::FilePath& file_path,
                                       size_t max_files,
                                       size_t max_filesize,
                                       bool allow_folders,
                                       PUPData::PUP* pup);

}  // namespace chrome_cleaner

#endif  // CHROME_CHROME_CLEANER_PUP_DATA_PUP_DISK_UTIL_H_
