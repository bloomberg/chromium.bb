// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_SAFE_BROWSING_FILE_TYPE_POLICIES_TEST_UTIL_H_
#define CHROME_COMMON_SAFE_BROWSING_FILE_TYPE_POLICIES_TEST_UTIL_H_

#include "chrome/common/safe_browsing/file_type_policies.h"

namespace safe_browsing {

// This is a test fixture for modifying the proto with FileTypePolicies.
// While an object of this class is in scope, it will cause callers
// of FileTypePolicies::GetInstance() to see the modified list.
// When it goes out of scope, future callers will get the original list.
//
// Example:
//   FileTypePoliciesTestOverlay overlay_;
//   std::unique_ptr<DownloadFileTypesConfig> cfg =
//       overlay_.DuplicateConfig();
//   cfg.set_sampled_ping_probability(1.0);
//   overlay_.SwapConfig(cfg);
//   ...
class FileTypePoliciesTestOverlay {
 public:
  FileTypePoliciesTestOverlay();
  ~FileTypePoliciesTestOverlay();

  // Swaps the contents bewtween the existing config and |new_config|.
  void SwapConfig(std::unique_ptr<DownloadFileTypeConfig>& new_config) const;

  // Return a new copy of the original config.
  std::unique_ptr<DownloadFileTypeConfig> DuplicateConfig() const;

 private:
  std::unique_ptr<DownloadFileTypeConfig> orig_config_;
};

}  // namespace safe_browsing

#endif  // CHROME_COMMON_SAFE_BROWSING_FILE_TYPE_POLICIES_TEST_UTIL_H_
