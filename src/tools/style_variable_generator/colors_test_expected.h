// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is generated from:
//   colors_test_palette.json5
//   colors_test.json5

#ifndef TOOLS_STYLE_VARIABLE_GENERATOR_COLORS_TEST_EXPECTED_H_
#define TOOLS_STYLE_VARIABLE_GENERATOR_COLORS_TEST_EXPECTED_H_

namespace colors_test_expected {

enum class ColorName {
  kGoogleGrey900,
  kTextColorPrimary,
  kToggleColor,
};

enum class OpacityName {
  kDisabledOpacity,
  kReferenceOpacity,
};

constexpr SkAlpha GetOpacity(OpacityName opacity_name, bool is_dark_mode) {
  switch (opacity_name) {
    case OpacityName::kDisabledOpacity:
      return 0x60;
    case OpacityName::kReferenceOpacity:
      if (is_dark_mode) {
        return 0xFF;
      } else {
        return GetOpacity(OpacityName::kDisabledOpacity, is_dark_mode);
      }
  }
}

constexpr SkColor ResolveColor(ColorName color_name, bool is_dark_mode) {
  switch (color_name) {
    case ColorName::kGoogleGrey900:
      return SkColorSetRGB(0x20, 0x21, 0x24);
    case ColorName::kTextColorPrimary:
      if (is_dark_mode) {
        return SkColorSetRGB(0xFF, 0xFF, 0xFF);
      } else {
        return ResolveColor(ColorName::kGoogleGrey900, is_dark_mode);
      }
    case ColorName::kToggleColor:
      if (is_dark_mode) {
        return SkColorSetA(ResolveColor(ColorName::kTextColorPrimary, is_dark_mode), GetOpacity(OpacityName::kDisabledOpacity, is_dark_mode));
      } else {
        return SkColorSetA(ResolveColor(ColorName::kTextColorPrimary, is_dark_mode), 0x19);
      }
  }
}

}  // namespace colors_test_expected
#endif  // TOOLS_STYLE_VARIABLE_GENERATOR_COLORS_TEST_EXPECTED_H_
