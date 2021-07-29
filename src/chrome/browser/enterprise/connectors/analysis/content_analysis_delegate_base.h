// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_ANALYSIS_CONTENT_ANALYSIS_DELEGATE_BASE_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_ANALYSIS_CONTENT_ANALYSIS_DELEGATE_BASE_H_

#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace enterprise_connectors {

class ContentAnalysisDelegateBase {
 public:
  // Enum to identify which message to show once scanning is complete. Ordered
  // by precedence for when multiple files have conflicting results.
  enum class FinalResult {
    // Show that an issue was found and that the upload is blocked.
    FAILURE = 0,

    // Show that files were not uploaded since they were too large.
    LARGE_FILES = 1,

    // Show that files were not uploaded since they were encrypted.
    ENCRYPTED_FILES = 2,

    // Show that DLP checks failed, but that the user can proceed if they want.
    WARNING = 3,

    // Show that no issue was found and that the user may proceed.
    SUCCESS = 4,
  };

  virtual ~ContentAnalysisDelegateBase() = default;

  // Called when the user decides to bypass the verdict they obtained from DLP.
  virtual void BypassWarnings() = 0;

  // Called when the user hits "cancel" on the dialog, typically cancelling a
  // pending file transfer.
  virtual void Cancel(bool warning) = 0;

  // Returns the custom message specified by the admin to display in the dialog,
  // or absl::nullopt if there isn't any.
  virtual absl::optional<std::u16string> GetCustomMessage() const = 0;

  // Returns the custom "learn more" URL specified by the admin to display in
  // the dialog, or absl::nullopt if there isn't any.
  virtual absl::optional<GURL> GetCustomLearnMoreUrl() const = 0;

  // Returns the text to display on the "cancel" button in the dialog, or
  // absl::nullopt if no text is specified by this delegate. Takes precedence
  // over any other text that would be chosen by the dialog.
  virtual absl::optional<std::u16string> OverrideCancelButtonText() const = 0;
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_ANALYSIS_CONTENT_ANALYSIS_DELEGATE_BASE_H_
