// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/test_theme_provider.h"

#include "ui/gfx/color_palette.h"
#include "ui/gfx/color_utils.h"

TestThemeProvider::TestThemeProvider() = default;

TestThemeProvider::~TestThemeProvider() = default;

gfx::ImageSkia* TestThemeProvider::GetImageSkiaNamed(int id) const {
  return nullptr;
}

SkColor TestThemeProvider::GetColor(int id) const {
  auto it = colors_.find(id);
  return it != colors_.end() ? it->second : gfx::kPlaceholderColor;
}

color_utils::HSL TestThemeProvider::GetTint(int id) const {
  return color_utils::HSL();
}

int TestThemeProvider::GetDisplayProperty(int id) const {
  return -1;
}

bool TestThemeProvider::ShouldUseNativeFrame() const {
  return false;
}

bool TestThemeProvider::HasCustomImage(int id) const {
  return false;
}

bool TestThemeProvider::HasCustomColor(int id) const {
  return colors_.find(id) != colors_.end();
}

base::RefCountedMemory* TestThemeProvider::GetRawData(
    int id,
    ui::ResourceScaleFactor scale_factor) const {
  return nullptr;
}

void TestThemeProvider::SetColor(int id, SkColor color) {
  colors_[id] = color;
}
