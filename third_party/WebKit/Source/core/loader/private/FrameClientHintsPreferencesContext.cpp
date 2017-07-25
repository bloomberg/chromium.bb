// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/loader/private/FrameClientHintsPreferencesContext.h"

#include "core/frame/UseCounter.h"

namespace blink {

namespace {

// Mapping from WebClientHintsType to WebFeature. The ordering should match the
// ordering of enums in WebClientHintsType.
static constexpr WebFeature kWebFeatureMapping[] = {
    WebFeature::kClientHintsDeviceMemory, WebFeature::kClientHintsDPR,
    WebFeature::kClientHintsResourceWidth,
    WebFeature::kClientHintsViewportWidth,
};

static_assert(kWebClientHintsTypeLast + 1 == arraysize(kWebFeatureMapping),
              "unhandled client hint type");

}  // namespace

FrameClientHintsPreferencesContext::FrameClientHintsPreferencesContext(
    LocalFrame* frame)
    : frame_(frame) {}

void FrameClientHintsPreferencesContext::CountClientHints(
    WebClientHintsType type) {
  UseCounter::Count(frame_, kWebFeatureMapping[static_cast<int32_t>(type)]);
}

void FrameClientHintsPreferencesContext::CountPersistentClientHintHeaders() {
  UseCounter::Count(frame_, WebFeature::kPersistentClientHintHeader);
}

}  // namespace blink
