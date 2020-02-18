// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_GL_FENCE_APPLE_H_
#define UI_GL_GL_FENCE_APPLE_H_

#include "base/macros.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_export.h"
#include "ui/gl/gl_fence.h"

namespace gl {

class GL_EXPORT GLFenceAPPLE : public GLFence {
 public:
  GLFenceAPPLE();
  ~GLFenceAPPLE() override;

  // GLFence implementation:
  bool HasCompleted() override;
  void ClientWait() override;
  void ServerWait() override;
  void Invalidate() override;

 private:
  GLuint fence_ = 0;

  DISALLOW_COPY_AND_ASSIGN(GLFenceAPPLE);
};

}  // namespace gl

#endif  // UI_GL_GL_FENCE_APPLE_H_
