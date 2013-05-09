// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_SCOPED_BINDERS_H_
#define UI_GL_SCOPED_BINDERS_H_

#include "ui/gl/gl_export.h"

namespace ui {

class GL_EXPORT ScopedFrameBufferBinder {
 public:
  explicit ScopedFrameBufferBinder(unsigned int fbo);
  ~ScopedFrameBufferBinder();

 private:
  int old_fbo_;
};

class GL_EXPORT ScopedTextureBinder {
 public:
  ScopedTextureBinder(unsigned int target, unsigned int id);
  ~ScopedTextureBinder();

 private:
  int target_;
  int old_id_;
};

}  // namespace ui

#endif  // UI_GL_SCOPED_BINDERS_H_
