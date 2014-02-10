// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "ui/gl/scoped_cgl.h"

namespace gfx {

ScopedCGLSetCurrentContext::ScopedCGLSetCurrentContext(CGLContextObj context)
    : previous_context_(CGLGetCurrentContext(), base::scoped_policy::RETAIN) {
  CGLError error = CGLSetCurrentContext(context);
  DCHECK_EQ(error, kCGLNoError) << "CGLSetCurrentContext should never fail";
}

ScopedCGLSetCurrentContext::~ScopedCGLSetCurrentContext() {
  CGLError error = CGLSetCurrentContext(previous_context_);
  DCHECK_EQ(error, kCGLNoError) << "CGLSetCurrentContext should never fail";
}

}  // namespace gfx
