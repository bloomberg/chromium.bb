// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_RENDER_TEXT_LINUX_H_
#define UI_GFX_RENDER_TEXT_LINUX_H_
#pragma once

#include "ui/gfx/render_text.h"

namespace gfx {

// RenderTextLinux is the Linux implementation of RenderText using Pango.
class RenderTextLinux : public RenderText {
 public:
  RenderTextLinux();
  virtual ~RenderTextLinux();

private:
  DISALLOW_COPY_AND_ASSIGN(RenderTextLinux);
};

}  // namespace gfx;

#endif  // UI_GFX_RENDER_TEXT_LINUX_H_
