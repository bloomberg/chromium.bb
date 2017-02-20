// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/loader/private/FrameClientHintsPreferencesContext.h"

#include "core/frame/UseCounter.h"

namespace blink {

FrameClientHintsPreferencesContext::FrameClientHintsPreferencesContext(
    Frame* frame)
    : m_frame(frame) {}

void FrameClientHintsPreferencesContext::countClientHintsDPR() {
  UseCounter::count(m_frame, UseCounter::ClientHintsDPR);
}

void FrameClientHintsPreferencesContext::countClientHintsResourceWidth() {
  UseCounter::count(m_frame, UseCounter::ClientHintsResourceWidth);
}

void FrameClientHintsPreferencesContext::countClientHintsViewportWidth() {
  UseCounter::count(m_frame, UseCounter::ClientHintsViewportWidth);
}

}  // namespace blink
