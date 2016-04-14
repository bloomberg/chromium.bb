// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_COLOR_PALETTE_H_
#define UI_GFX_COLOR_PALETTE_H_

#include "third_party/skia/include/core/SkColor.h"

namespace gfx {

// A placeholder value for unset colors. This should never be visible and is red
// as a visual flag for misbehaving code.
constexpr SkColor kPlaceholderColor = SK_ColorRED;

const SkColor kChromeIconGrey = SkColorSetRGB(0x5A, 0x5A, 0x5A);

// The number refers to the shade of darkness. Each color in the MD
// palette ranges from 100-900.
const SkColor kGoogleBlue300 = SkColorSetRGB(0x7B, 0xAA, 0xF7);
const SkColor kGoogleBlue500 = SkColorSetRGB(0x42, 0x85, 0xF4);
const SkColor kGoogleBlue700 = SkColorSetRGB(0x33, 0x67, 0xD6);
const SkColor kGoogleRed300 = SkColorSetRGB(0xE6, 0x7C, 0x73);
const SkColor kGoogleRed700 = SkColorSetRGB(0xC5, 0x39, 0x29);
const SkColor kGoogleGreen300 = SkColorSetRGB(0x57, 0xBB, 0x8A);
const SkColor kGoogleGreen700 = SkColorSetRGB(0x0B, 0x80, 0x43);
const SkColor kGoogleYellow300 = SkColorSetRGB(0xF7, 0xCB, 0x4D);
const SkColor kGoogleYellow700 = SkColorSetRGB(0xF0, 0x93, 0x00);

// Material Design canonical colors, from
// https://www.google.com/design/spec/style/color.html#color-color-palette
const SkColor kMaterialBlue300 = SkColorSetRGB(0x64, 0xB5, 0xF6);
const SkColor kMaterialBlue500 = SkColorSetRGB(0x21, 0x96, 0xF3);
const SkColor kMaterialBlue700 = SkColorSetRGB(0x19, 0x76, 0xD2);

const SkColor kMaterialGrey300 = SkColorSetRGB(0xE0, 0xE0, 0xE0);
const SkColor kMaterialGrey500 = SkColorSetRGB(0x9E, 0x9E, 0x9E);

}  // namespace gfx

#endif  // UI_GFX_COLOR_PALETTE_H_
