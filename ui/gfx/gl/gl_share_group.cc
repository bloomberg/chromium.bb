// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/gl/gl_share_group.h"

#include "ui/gfx/gl/gl_context.h"

namespace gfx {

GLShareGroup::GLShareGroup() {
}

void GLShareGroup::AddContext(GLContext* context) {
  contexts_.insert(context);
}

void GLShareGroup::RemoveContext(GLContext* context) {
  contexts_.erase(context);
}

void* GLShareGroup::GetHandle() {
  for (ContextSet::iterator it = contexts_.begin();
       it != contexts_.end();
       ++it) {
     if ((*it)->GetHandle())
       return (*it)->GetHandle();
  }

  return NULL;
}

GLShareGroup::~GLShareGroup() {
}

}  // namespace gfx
