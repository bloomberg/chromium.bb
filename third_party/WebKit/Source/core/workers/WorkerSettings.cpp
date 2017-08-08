// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/workers/WorkerSettings.h"

namespace blink {

WorkerSettings::WorkerSettings(Settings* settings) {
  if (settings)
    this->CopyFlagValuesFromSettings(settings);
}

void WorkerSettings::CopyFlagValuesFromSettings(Settings* settings) {
  disable_reading_from_canvas_ = settings->GetDisableReadingFromCanvas();
  strict_mixed_content_checking_ = settings->GetStrictMixedContentChecking();
  allow_running_of_insecure_content_ =
      settings->GetAllowRunningOfInsecureContent();
  strictly_block_blockable_mixed_content_ =
      settings->GetStrictlyBlockBlockableMixedContent();
  settings->GetGenericFontFamilySettings().IsolatedCopyTo(
      generic_font_family_settings_);
}

}  // namespace blink
