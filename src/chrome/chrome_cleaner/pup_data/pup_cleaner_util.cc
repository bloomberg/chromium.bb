// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/pup_data/pup_cleaner_util.h"

#include "base/files/file_util.h"
#include "base/logging.h"
#include "chrome/chrome_cleaner/os/file_remover.h"
#include "chrome/chrome_cleaner/pup_data/pup_data.h"

namespace chrome_cleaner {

bool CollectRemovablePupFiles(Engine::Name engine,
                              const std::vector<UwSId>& pup_ids,
                              FilePathSet* pup_files) {
  bool valid_removal = true;

  FilePathSet files_detected_in_services =
      PUPData::GetFilesDetectedInServices(pup_ids);

  for (const auto& pup_id : pup_ids) {
    size_t added_pup_files_size = 0;
    const auto* pup = PUPData::GetPUP(pup_id);

    for (const auto& file_path : pup->expanded_disk_footprints.file_paths()) {
      // Verify that files can be deleted.
      if (FileRemover::IsFileRemovalAllowed(file_path,
                                            files_detected_in_services, {}) ==
          FileRemoverAPI::DeletionValidationStatus::ALLOWED) {
        pup_files->Insert(file_path);
        ++added_pup_files_size;
      }
    }

    // Check the number of files collected to prevent bugs or malware tricks to
    // get too many files to be removed.
    // TODO(veranika): maybe limit number of deleted files for other engines
    // too.
    if (added_pup_files_size > pup->signature().max_files_to_remove &&
        engine == Engine::URZA) {
      LOG(ERROR) << "Too many files to remove for UwS " << pup->signature().id
                 << " (" << added_pup_files_size << " files).";
      valid_removal = false;
    }
  }

  return valid_removal;
}

}  // namespace chrome_cleaner
