// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/gl_surface_egl.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/init/gl_factory.h"
#include "ui/gl/test/gl_surface_test_support.h"

namespace gl {

class GLSurfaceEGLTest : public testing::Test {};

TEST(GLSurfaceEGLTest, SurfaceFormatTest) {
  GLSurfaceTestSupport::InitializeOneOffImplementation(
      GLImplementation::kGLImplementationEGLGLES2, true);

  GLSurfaceFormat surface_format = GLSurfaceFormat();
  surface_format.SetDepthBits(24);
  surface_format.SetStencilBits(8);
  surface_format.SetSamples(0);
  scoped_refptr<GLSurface> surface =
      init::CreateOffscreenGLSurfaceWithFormat(gfx::Size(1, 1), surface_format);
  EGLConfig config = surface->GetConfig();
  EXPECT_TRUE(config);

  EGLint attrib;
  eglGetConfigAttrib(surface->GetDisplay(), config, EGL_DEPTH_SIZE, &attrib);
  EXPECT_LE(24, attrib);

  eglGetConfigAttrib(surface->GetDisplay(), config, EGL_STENCIL_SIZE, &attrib);
  EXPECT_LE(8, attrib);

  eglGetConfigAttrib(surface->GetDisplay(), config, EGL_SAMPLES, &attrib);
  EXPECT_EQ(0, attrib);
}

}  // namespace gl
