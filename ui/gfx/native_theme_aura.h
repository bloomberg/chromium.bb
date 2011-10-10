// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_NATIVE_THEME_AURA_H_
#define UI_GFX_NATIVE_THEME_AURA_H_
#pragma once

#include "ui/gfx/native_theme_base.h"

namespace gfx {

// Aura implementation of native theme support.
class NativeThemeAura : public NativeThemeBase {
 public:
  static const NativeThemeAura* instance();

 private:
  NativeThemeAura();
  virtual ~NativeThemeAura();
};

}  // namespace gfx

#endif  // UI_GFX_NATIVE_THEME_AURA_H_
