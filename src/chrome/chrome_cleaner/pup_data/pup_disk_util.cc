// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/pup_data/pup_disk_util.h"

#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "chrome/chrome_cleaner/os/disk_util.h"

namespace chrome_cleaner {

void CollectPathsRecursively(const base::FilePath& file_path,
                             PUPData::PUP* pup) {
  DCHECK(pup);

  // Avoid enumerating the folder content if the folder is already in the pup's
  // disk footprints.
  if (!pup->AddDiskFootprint(file_path))
    return;

  base::FileEnumerator file_enum(
      file_path,
      true,
      base::FileEnumerator::FILES | base::FileEnumerator::DIRECTORIES);
  for (base::FilePath file = file_enum.Next(); !file.empty();
       file = file_enum.Next()) {
    pup->AddDiskFootprint(file);
  }
}

bool CollectPathsRecursivelyWithLimits(const base::FilePath& file_path,
                                       size_t max_files,
                                       size_t max_filesize,
                                       bool allow_folders,
                                       PUPData::PUP* pup) {
  return CollectPathsRecursivelyWithLimits(file_path,
                                           max_files,
                                           max_filesize,
                                           allow_folders,
                                           &pup->expanded_disk_footprints);
}

}  // namespace chrome_cleaner
