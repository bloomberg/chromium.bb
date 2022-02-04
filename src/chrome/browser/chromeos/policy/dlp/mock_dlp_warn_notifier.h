// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_DLP_MOCK_DLP_WARN_NOTIFIER_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_DLP_MOCK_DLP_WARN_NOTIFIER_H_

#include "base/callback_forward.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_confidential_contents.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_warn_dialog.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_warn_notifier.h"
#include "testing/gmock/include/gmock/gmock.h"

using ::testing::Mock;

namespace policy {

// Allows tests to simulate the user's response to the warning dialog.
class MockDlpWarnNotifier : public DlpWarnNotifier {
 public:
  MockDlpWarnNotifier() = delete;
  explicit MockDlpWarnNotifier(bool should_proceed = true);
  MockDlpWarnNotifier(const MockDlpWarnNotifier& other) = delete;
  MockDlpWarnNotifier& operator=(const MockDlpWarnNotifier& other) = delete;
  ~MockDlpWarnNotifier() override;

  MOCK_METHOD(void,
              ShowDlpWarningDialog,
              (OnDlpRestrictionCheckedCallback callback,
               DlpWarnDialog::DlpWarnDialogOptions options),
              (override));

 private:
  const bool should_proceed_;
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_DLP_MOCK_DLP_WARN_NOTIFIER_H_
