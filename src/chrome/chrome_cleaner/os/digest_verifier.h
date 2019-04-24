// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_CLEANER_OS_DIGEST_VERIFIER_H_
#define CHROME_CHROME_CLEANER_OS_DIGEST_VERIFIER_H_

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "base/files/file_path.h"

namespace chrome_cleaner {

// This class verifies whether a file on disk is known, and if it is, whether
// the file's digest matches the expected digest for that file.
class DigestVerifier {
 public:
  // Returns an instance of DigestVerifier, or nullptr if initialization
  // failed. This will load a list of digests from the "TEXT" resource with ID
  // |resource_id|, which is expected to be a FileDigests proto (see
  // file_digest.proto).
  static std::shared_ptr<DigestVerifier> CreateFromResource(int resource_id);

  ~DigestVerifier();

  // Returns true if |file| has a name that matches the name of one of the known
  // files, and if that file's digest also matches the expected digest as
  // specified in the proto used when creating this instance.
  bool IsKnownFile(const base::FilePath& file) const;

  std::vector<base::FilePath::StringType> GetKnownFileNames() const;

 private:
  // Maps a file's basename to the expected digest for that file.
  std::unordered_map<base::FilePath::StringType, std::string> digests_;

  DigestVerifier();

  bool Initialize(int resource_id);
};

}  // namespace chrome_cleaner

#endif  // CHROME_CHROME_CLEANER_OS_DIGEST_VERIFIER_H_
