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
      surface_(surface) {
  DCHECK(context);
  DCHECK(surface);
  succeeded_ = context->MakeCurrent(surface);
}

ScopedMakeCurrent::~ScopedMakeCurrent() {
  if (!restored_)
    CHECK(Restore()) << "ScopedMakeCurrent: Restore failed";
}

bool ScopedMakeCurrent::Succeeded() const {
  return succeeded_;
}

bool ScopedMakeCurrent::Restore() {
  DCHECK(!restored_);
  restored_ = true;

  if (!succeeded_)
    return true;

  if (previous_context_)
    return previous_context_->MakeCurrent(previous_surface_.get());

  context_->ReleaseCurrent(surface_.get());
  return true;
}

ScopedReleaseCurrent::ScopedReleaseCurrent()
    : previous_context_(gl::GLContext::GetCurrent()),
      previous_surface_(gl::GLSurface::GetCurrent()) {
  if (previous_context_)
    previous_context_->ReleaseCurrent(previous_surface_.get());
}

ScopedReleaseCurrent::~ScopedReleaseCurrent() {
  if (!restored_)
    CHECK(Restore()) << "ScopedReleaseCurrent: Restore failed";
}

bool ScopedReleaseCurrent::Restore() {
  DCHECK(!restored_);
  restored_ = true;

  if (previous_context_)
    return previous_context_->MakeCurrent(previous_surface_.get());

  return true;
}

}  // namespace ui
