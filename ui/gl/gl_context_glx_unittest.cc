// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/gl_context_glx.h"

#include "base/memory/scoped_refptr.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/x/x11_error_tracker.h"
#include "ui/gfx/x/x11_types.h"
#include "ui/gl/gl_surface_glx_x11.h"
#include "ui/gl/init/gl_factory.h"
#include "ui/gl/test/gl_image_test_support.h"

#include <X11/Xlib.h>

namespace gl {

TEST(GLContextGLXTest, DoNotDesrtroyOnFailedMakeCurrent) {
  auto* xdisplay = gfx::GetXDisplay();
  ASSERT_TRUE(xdisplay);

  gfx::X11ErrorTracker error_tracker;

  // Turn on sync behaviour, and create the window.
  XSynchronize(xdisplay, True);
  XSetWindowAttributes swa;
  memset(&swa, 0, sizeof(swa));
  swa.background_pixmap = 0;
  swa.override_redirect = True;
  auto xwindow = XCreateWindow(xdisplay, DefaultRootWindow(xdisplay), 0, 0, 10,
                               10,              // x, y, width, height
                               0,               // border width
                               CopyFromParent,  // depth
                               InputOutput,
                               CopyFromParent,  // visual
                               CWBackPixmap | CWOverrideRedirect, &swa);
  XMapWindow(xdisplay, xwindow);

  GLImageTestSupport::InitializeGL();
  auto surface =
      gl::InitializeGLSurface(base::MakeRefCounted<GLSurfaceGLXX11>(xwindow));
  scoped_refptr<GLContext> context =
      gl::init::CreateGLContext(nullptr, surface.get(), GLContextAttribs());
  // Verify that MakeCurrent() is successful.
  ASSERT_TRUE(context->GetHandle());
  ASSERT_TRUE(context->MakeCurrent(surface.get()));
  EXPECT_TRUE(context->GetHandle());

  // Destroy the window, and turn off sync behaviour. We should get no x11
  // errors so far.
  context->ReleaseCurrent(surface.get());
  XDestroyWindow(xdisplay, xwindow);
  XSynchronize(xdisplay, False);
  ASSERT_FALSE(error_tracker.FoundNewError());

  // Now that the window is gone, MakeCurrent() should fail. But the context
  // shouldn't be destroyed, and there should be some x11 errors captured.
  EXPECT_FALSE(context->MakeCurrent(surface.get()));
  ASSERT_TRUE(context->GetHandle());
  EXPECT_TRUE(error_tracker.FoundNewError());
}

}  // namespace gl
