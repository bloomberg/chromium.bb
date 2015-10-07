// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_TEST_GL_SURFACE_TEST_SUPPORT_H_
#define UI_GL_TEST_GL_SURFACE_TEST_SUPPORT_H_

namespace gfx {

class GLContext;

class GLSurfaceTestSupport {
 public:
  static void InitializeOneOff();
  static void InitializeOneOffWithMockBindings();
  static void InitializeDynamicMockBindings(GLContext* context);
};

}  // namespace gfx

#endif  // UI_GL_TEST_GL_SURFACE_TEST_SUPPORT_H_
