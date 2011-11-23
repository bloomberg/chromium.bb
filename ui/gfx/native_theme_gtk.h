// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_NATIVE_THEME_GTK_H_
#define UI_GFX_NATIVE_THEME_GTK_H_
#pragma once

#include "ui/gfx/native_theme_base.h"

namespace gfx {

// GTK implementation of native theme support.
class NativeThemeGtk : public NativeThemeBase {
 public:
  static const NativeThemeGtk* instance();

  virtual SkColor GetSystemColor(ColorId color_id) const OVERRIDE;

 private:
  NativeThemeGtk();
  virtual ~NativeThemeGtk();
};

}  // namespace gfx

#endif  // UI_GFX_NATIVE_THEME_GTK_H_
