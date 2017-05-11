// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FakeGLES2Interface_h
#define FakeGLES2Interface_h

#include "gpu/command_buffer/client/gles2_interface_stub.h"

class FakeGLES2Interface : public gpu::gles2::GLES2InterfaceStub {
 public:
  // GLES2Interface implementation.
  GLenum GetGraphicsResetStatusKHR() override {
    return context_lost_ ? GL_INVALID_OPERATION : GL_NO_ERROR;
  }

  // Methods for tests.
  void SetIsContextLost(bool lost) { context_lost_ = lost; }

 private:
  bool context_lost_ = false;
};

#endif  // FakeGLES2Interface_h
