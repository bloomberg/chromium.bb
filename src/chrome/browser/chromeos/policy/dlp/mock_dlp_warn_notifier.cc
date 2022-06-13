// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/dlp/mock_dlp_warn_notifier.h"

#include "chrome/browser/chromeos/policy/dlp/dlp_warn_dialog.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_warn_notifier.h"
#include "testing/gmock/include/gmock/gmock.h"

using ::testing::Mock;

namespace policy {

MockDlpWarnNotifier::MockDlpWarnNotifier(bool should_proceed)
    : should_proceed_(should_proceed) {
  // Simulate proceed or cancel.
  ON_CALL(*this, ShowDlpWarningDialog)
      .WillByDefault([this](OnDlpRestrictionCheckedCallback callback,
                            DlpWarnDialog::DlpWarnDialogOptions options) {
        std::move(callback).Run(should_proceed_);
      });
}

MockDlpWarnNotifier::~MockDlpWarnNotifier() = default;

}  // namespace policy
