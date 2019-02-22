// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_CLEANER_OS_FILE_REMOVER_H_
#define CHROME_CHROME_CLEANER_OS_FILE_REMOVER_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/strings/string16.h"
#include "chrome/chrome_cleaner/os/digest_verifier.h"
#include "chrome/chrome_cleaner/os/file_path_set.h"
#include "chrome/chrome_cleaner/os/file_remover_api.h"
#include "chrome/chrome_cleaner/os/layered_service_provider_api.h"

namespace chrome_cleaner {

// This class implements the |FileRemoverAPI| for production code.
class FileRemover : public FileRemoverAPI {
 public:
  // Checks whether deletion of the file at |path| is allowed. Files at paths in
  // |fordbid_deletion| are never allowed to be deleted. Files at paths in
  // |allow_deletion| are allowed to be deleted even if they do not appear to be
  // executable.
  static DeletionValidationStatus IsFileRemovalAllowed(
      const base::FilePath& path,
      const FilePathSet& allow_deletion,
      const FilePathSet& forbid_deletion);

  // |digest_verifier| can be either nullptr or an instance of DigestVerifier.
  // If it is an instance of DigestVerifier, any files known to the
  // DigestVerifier will not be removed.
  FileRemover(std::shared_ptr<DigestVerifier> digest_verifier,
              const LayeredServiceProviderAPI& lsp,
              const FilePathSet& deletion_allowed_paths,
              base::RepeatingClosure reboot_needed_callback);

  ~FileRemover() override;

  // FileRemoverAPI implementation.
  bool RemoveNow(const base::FilePath& path) const override;
  bool RegisterPostRebootRemoval(
      const base::FilePath& file_path) const override;

  // Checks if the file is active and can be deleted.
  DeletionValidationStatus CanRemove(const base::FilePath& file) const override;

 private:
  std::shared_ptr<DigestVerifier> digest_verifier_;
  FilePathSet deletion_forbidden_paths_;
  FilePathSet deletion_allowed_paths_;
  base::RepeatingClosure reboot_needed_callback_;
};

}  // namespace chrome_cleaner

#endif  // CHROME_CHROME_CLEANER_OS_FILE_REMOVER_H_
