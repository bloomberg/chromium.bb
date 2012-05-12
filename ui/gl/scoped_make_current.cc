// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/scoped_make_current.h"

#include "base/logging.h"

namespace gfx {

ScopedMakeCurrent::ScopedMakeCurrent(GLContext* context, GLSurface* surface)
    : previous_context_(GLContext::GetCurrent())
    , previous_surface_(GLSurface::GetCurrent())
    , context_(context)
    , surface_(surface)
    , succeeded_(false) {
  DCHECK(context);
  DCHECK(surface);
  succeeded_ = context->MakeCurrent(surface);
}

ScopedMakeCurrent::~ScopedMakeCurrent() {
  if (previous_context_.get()) {
    DCHECK(previous_surface_.get());
    previous_context_->MakeCurrent(previous_surface_.get());
  } else {
    context_->ReleaseCurrent(surface_.get());
  }
}

bool ScopedMakeCurrent::Succeeded() {
  return succeeded_;
}

}  // namespace gfx

