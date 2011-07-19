// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_TOOLS_PEPPER_TEST_PLUGIN_DEMO_3D_H_
#define WEBKIT_TOOLS_PEPPER_TEST_PLUGIN_DEMO_3D_H_

#if !defined(INDEPENDENT_PLUGIN)
#include "base/basictypes.h"
#include "third_party/gles2_book/Chapter_2/Hello_Triangle/Hello_Triangle.h"

namespace pepper {
class Demo3D {
 public:
  Demo3D();
  virtual ~Demo3D();

  // Sets the size of the window on which this demo object will render.
  void SetWindowSize(int width, int height);

  // Initializes the OpenGL state required by this demo.
  // When this function is called, it is assumed that a rendering context has
  // already been created and made current.
  bool InitGL();

  // Release the OpenGL resources acquired by this demo.
  // When this function is called, it is assumed that the rendering context
  // used to initialize the demo is current.
  void ReleaseGL();

  // Performs OpenGL rendering.
  // When this function is called, it is assumed that the rendering context
  // has been made current.
  void Draw();

 private:
  ESContext context_;
  HTUserData user_data_;

  DISALLOW_COPY_AND_ASSIGN(Demo3D);
};
}  // namespace pepper
#endif  // INDEPENDENT_PLUGIN
#endif  // WEBKIT_TOOLS_PEPPER_TEST_PLUGIN_DEMO_3D_H_
