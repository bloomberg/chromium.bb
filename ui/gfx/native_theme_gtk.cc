// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/native_theme_gtk.h"

#include "base/basictypes.h"

namespace gfx {

// static
const NativeTheme* NativeTheme::instance() {
  return NativeThemeGtk::instance();
}

// static
const NativeThemeGtk* NativeThemeGtk::instance() {
  CR_DEFINE_STATIC_LOCAL(NativeThemeGtk, s_native_theme, ());
  return &s_native_theme;
}

NativeThemeGtk::NativeThemeGtk() {
}

NativeThemeGtk::~NativeThemeGtk() {
}

}  // namespace gfx
