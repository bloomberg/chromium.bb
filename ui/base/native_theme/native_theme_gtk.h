// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_NATIVE_THEME_NATIVE_THEME_GTK_H_
#define UI_BASE_NATIVE_THEME_NATIVE_THEME_GTK_H_
#pragma once

#include "ui/base/native_theme/native_theme_base.h"

namespace ui {

// GTK implementation of native theme support.
class NativeThemeGtk : public NativeThemeBase {
 public:
  static const NativeThemeGtk* instance();

  virtual SkColor GetSystemColor(ColorId color_id) const OVERRIDE;

 private:
  NativeThemeGtk();
  virtual ~NativeThemeGtk();
};

}  // namespace ui

#endif  // UI_BASE_NATIVE_THEME_NATIVE_THEME_GTK_H_
