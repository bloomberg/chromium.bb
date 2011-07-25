// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_RENDER_TEXT_WIN_H_
#define UI_GFX_RENDER_TEXT_WIN_H_
#pragma once

#include "ui/gfx/render_text.h"

namespace gfx {

// RenderTextWin is the Windows implementation of RenderText using Uniscribe.
class RenderTextWin : public RenderText {
 public:
  RenderTextWin();
  virtual ~RenderTextWin();

 private:
  DISALLOW_COPY_AND_ASSIGN(RenderTextWin);
};

}  // namespace gfx;

#endif  // UI_GFX_RENDER_TEXT_WIN_H_
