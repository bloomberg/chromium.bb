// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_DARK_MODE_COLOR_CLASSIFIER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_DARK_MODE_COLOR_CLASSIFIER_H_

#include "third_party/blink/renderer/platform/graphics/color.h"
#include "third_party/blink/renderer/platform/platform_export.h"

namespace blink {

bool PLATFORM_EXPORT IsLight(const Color& color, float opacity);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_DARK_MODE_COLOR_CLASSIFIER_H_
