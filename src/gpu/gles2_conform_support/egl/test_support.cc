// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "test_support.h"
#include "gpu/gles2_conform_support/egl/display.h"
#include "gpu/gles2_conform_support/egl/thread_state.h"

#if defined(COMPONENT_BUILD) && defined(COMMAND_BUFFER_GLES_LIB_SUPPORT_ONLY)
bool g_command_buffer_gles_has_atexit_manager;
#endif

extern "C" {
EGLAPI void EGLAPIENTRY
CommandBufferGLESSetNextCreateWindowSurfaceCreatesPBuffer(EGLDisplay dpy,
                                                          EGLint width,
                                                          EGLint height) {
  egl::ThreadState* ts = egl::ThreadState::Get();
  egl::Display* display = ts->GetDisplay(dpy);
  if (!display)
    return;
  display->SetNextCreateWindowSurfaceCreatesPBuffer(width, height);
}
}
