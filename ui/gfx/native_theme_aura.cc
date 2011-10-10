// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/native_theme_aura.h"

namespace gfx {

// static
const NativeTheme* NativeTheme::instance() {
  return NativeThemeAura::instance();
}

// static
const NativeThemeAura* NativeThemeAura::instance() {
  static const NativeThemeAura s_native_theme;
  return &s_native_theme;
}

NativeThemeAura::NativeThemeAura() {
}

NativeThemeAura::~NativeThemeAura() {
}

}  // namespace gfx
