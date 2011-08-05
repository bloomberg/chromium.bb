// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/scoped_ns_graphics_context_save_gstate_mac.h"

#import <AppKit/AppKit.h>

#include "base/logging.h"
#include "base/mac/mac_util.h"

namespace gfx {

ScopedNSGraphicsContextSaveGState::ScopedNSGraphicsContextSaveGState()
    : context_([NSGraphicsContext currentContext]) {
  [NSGraphicsContext saveGraphicsState];
}

ScopedNSGraphicsContextSaveGState::~ScopedNSGraphicsContextSaveGState() {
  [NSGraphicsContext restoreGraphicsState];
  if (!context_ && base::mac::IsOSLeopardOrEarlier()) {
    // On 10.5 and earlier, there is a bug. If the current graphics context was
    // nil when +[NSGraphicsContext saveGraphicsState] was called, then calling
    // +[NSGraphicsContext restoreGraphicsState] will not restore a nil current
    // context, but will leave the current context in place. Because allowing
    // that stale context (which will be deallocated soon) to remain current
    // will only lead to heartache and pain, the current context must be
    // manually nilled out.
    [NSGraphicsContext setCurrentContext:nil];
  }
  DCHECK_EQ(context_, [NSGraphicsContext currentContext]);
}

}  // namespace gfx
