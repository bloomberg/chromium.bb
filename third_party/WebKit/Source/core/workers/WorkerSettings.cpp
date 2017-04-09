// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/workers/WorkerSettings.h"

namespace blink {

WorkerSettings::WorkerSettings(Settings* settings) {
  if (settings)
    this->CopyFlagValuesFromSettings(settings);
  else
    this->SetDefaultValues();
}

void WorkerSettings::CopyFlagValuesFromSettings(Settings* settings) {
  disable_reading_from_canvas_ = settings->GetDisableReadingFromCanvas();
}

void WorkerSettings::SetDefaultValues() {
  disable_reading_from_canvas_ = false;
}

}  // namespace blink
