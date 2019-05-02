// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_DARK_MODE_SETTINGS_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_DARK_MODE_SETTINGS_H_

namespace blink {

enum class DarkMode {
  // Default, drawing is unfiltered.
  kOff,
  // For testing only, does a simple 8-bit invert of every RGB pixel component.
  kSimpleInvertForTesting,
  kInvertBrightness,
  kInvertLightness,
  kInvertLightnessLAB,
};

enum class DarkModeImagePolicy {
  // Apply dark-mode filter to all images.
  kFilterAll,
  // Never apply dark-mode filter to any images.
  kFilterNone,
  // Apply dark-mode based on image content.
  kFilterSmart,
};

// For images that should have a filter applied, which filter should be used?
enum class DarkModeImageStyle {
  // Invert images the same way as other elements
  kDefault,
  // Apply grayscale to images as well as inverting them
  kGrayscale
};

enum class DarkModePagePolicy {
  // Apply dark-mode filter to all frames, regardless of content.
  kFilterAll,
  // Apply dark-mode filter to frames based on background color.
  kFilterByBackground,
};

struct DarkModeSettings {
  DarkMode mode = DarkMode::kOff;
  bool grayscale = false;
  float contrast = 0.0;  // Valid range from -1.0 to 1.0
  DarkModeImagePolicy image_policy = DarkModeImagePolicy::kFilterAll;
  DarkModeImageStyle image_style = DarkModeImageStyle::kDefault;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_DARK_MODE_SETTINGS_H_
