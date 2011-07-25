// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/render_text_linux.h"

namespace gfx {

RenderTextLinux::RenderTextLinux()
    : RenderText() {
}

RenderTextLinux::~RenderTextLinux() {
}

RenderText* RenderText::CreateRenderText() {
  return new RenderTextLinux;
}

}  // namespace gfx
