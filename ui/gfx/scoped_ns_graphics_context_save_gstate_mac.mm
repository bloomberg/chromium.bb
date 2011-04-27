// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/scoped_ns_graphics_context_save_gstate_mac.h"

#include <AppKit/AppKit.h>

namespace gfx {

ScopedNSGraphicsContextSaveGState::ScopedNSGraphicsContextSaveGState(
    NSGraphicsContext* context) : context_([context retain]) {
  if (!context_)
    context_.reset([[NSGraphicsContext currentContext] retain]);
  [context_ saveGraphicsState];
}

ScopedNSGraphicsContextSaveGState::~ScopedNSGraphicsContextSaveGState() {
  [context_ restoreGraphicsState];
}

}  // namespace gfx
