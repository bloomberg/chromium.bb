// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/scoped_make_current.h"

#include "base/logging.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_surface.h"

namespace ui {

ScopedMakeCurrent::ScopedMakeCurrent(gl::GLContext* context,
                                     gl::GLSurface* surface)
    : previous_context_(gl::GLContext::GetCurrent()),
      previous_surface_(gl::GLSurface::GetCurrent()),
      context_(context),
      surface_(surface),
      succeeded_(false) {
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

bool ScopedMakeCurrent::Succeeded() const {
  return succeeded_;
}

ScopedReleaseCurrent::ScopedReleaseCurrent(gl::GLSurface* this_surface) {
  gl::GLContext* current_context = gl::GLContext::GetCurrent();
  bool was_current =
      current_context && current_context->IsCurrent(this_surface);
  if (was_current) {
    make_current_.emplace(current_context, this_surface);
    current_context->ReleaseCurrent(this_surface);
  }
}

ScopedReleaseCurrent::~ScopedReleaseCurrent() {}

}  // namespace ui
