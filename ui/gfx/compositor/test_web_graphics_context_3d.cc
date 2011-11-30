// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/compositor/test_web_graphics_context_3d.h"

#include "ui/gfx/gl/gl_context_stub.h"
#include "ui/gfx/gl/gl_surface_stub.h"

namespace ui {

bool TestWebGraphicsContext3D::initialize(Attributes attributes,
                                          WebKit::WebView* view,
                                          bool render_directly_to_web_view) {
  gl_surface_ = new gfx::GLSurfaceStub;
  gl_context_ = new gfx::GLContextStub;
  gl_context_->MakeCurrent(gl_surface_.get());
  return true;
}

}  // namespace ui
