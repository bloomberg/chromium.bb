// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/native_theme_gtk.h"

#include <gtk/gtk.h>

#include "base/basictypes.h"
#include "base/logging.h"
#include "ui/gfx/skia_utils_gtk.h"

namespace {

const SkColor kInvalidColorIdColor = SkColorSetRGB(255, 0, 128);

}  // namespace

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

SkColor NativeThemeGtk::GetSystemColor(ColorId color_id) const {
  switch (color_id) {
    case kColorId_DialogBackground:
      // TODO(benrg): This code used to call gtk_widget_get_style() on the
      // widget being styled. After refactoring, that widget is not available
      // and we have to call gtk_widget_get_default_style(). Unfortunately,
      // it turns out that this breaks everything (chromium bug 105609,
      // chromium-os bug 23461). Need to figure out the right thing and do it.
      return gfx::GdkColorToSkColor(
                     gtk_widget_get_default_style()->bg[GTK_STATE_NORMAL]);
    default:
      NOTREACHED() << "Invalid color_id: " << color_id;
      break;
  }
  return kInvalidColorIdColor;
}

NativeThemeGtk::NativeThemeGtk() {
}

NativeThemeGtk::~NativeThemeGtk() {
}

}  // namespace gfx
