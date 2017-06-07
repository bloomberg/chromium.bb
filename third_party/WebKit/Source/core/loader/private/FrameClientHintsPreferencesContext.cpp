// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/loader/private/FrameClientHintsPreferencesContext.h"

#include "core/frame/UseCounter.h"

namespace blink {

FrameClientHintsPreferencesContext::FrameClientHintsPreferencesContext(
    LocalFrame* frame)
    : frame_(frame) {}

void FrameClientHintsPreferencesContext::CountClientHintsDeviceRAM() {
  UseCounter::Count(frame_, WebFeature::kClientHintsDeviceRAM);
}

void FrameClientHintsPreferencesContext::CountClientHintsDPR() {
  UseCounter::Count(frame_, WebFeature::kClientHintsDPR);
}

void FrameClientHintsPreferencesContext::CountClientHintsResourceWidth() {
  UseCounter::Count(frame_, WebFeature::kClientHintsResourceWidth);
}

void FrameClientHintsPreferencesContext::CountClientHintsViewportWidth() {
  UseCounter::Count(frame_, WebFeature::kClientHintsViewportWidth);
}

}  // namespace blink
