// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HighContrastSettings_h
#define HighContrastSettings_h

namespace blink {

enum class HighContrastMode {
  // Default, drawing is unfiltered.
  kOff,
  // For testing only, does a simple 8-bit invert of every RGB pixel component.
  kSimpleInvertForTesting,
  kInvertBrightness,
  kInvertLightness,
};

enum class HighContrastImagePolicy {
  // Apply high-contrast filter to all images.
  kFilterAll,
  // Never apply high-contrast filter to any images.
  kFilterNone,
};

struct HighContrastSettings {
  HighContrastMode mode = HighContrastMode::kOff;
  bool grayscale = false;
  float contrast = 0.0;  // Valid range from -1.0 to 1.0
  HighContrastImagePolicy image_policy = HighContrastImagePolicy::kFilterAll;
};

}  // namespace blink

#endif  // HighContrastSettings_h
