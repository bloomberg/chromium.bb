// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_SCREEN_COMPATIBLE_DC_WIN_H_
#define UI_GFX_SCREEN_COMPATIBLE_DC_WIN_H_
#pragma once

#include <windows.h>

#include "base/basictypes.h"

namespace gfx {

// Temporary device context that is compatible with the screen. Can be used for
// short-lived operations on the UI threads.
class ScopedTemporaryScreenCompatibleDC {
 public:
  ScopedTemporaryScreenCompatibleDC();
  ~ScopedTemporaryScreenCompatibleDC();

  HDC get() const { return hdc_; }

 private:
  HDC hdc_;

  DISALLOW_COPY_AND_ASSIGN(ScopedTemporaryScreenCompatibleDC);
};

}  // namespace gfx

#endif  // UI_GFX_SCREEN_COMPATIBLE_DC_WIN_H_
