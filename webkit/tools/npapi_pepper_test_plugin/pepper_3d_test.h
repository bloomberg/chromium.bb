// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_TOOLS_NPAPI_PEPPER_TEST_PLUGIN_PEPPER_3D_TEST_H
#define WEBKIT_TOOLS_NPAPI_PEPPER_TEST_PLUGIN_PEPPER_3D_TEST_H

#include "gpu/pgl/pgl.h"
#include "third_party/gles2_book/Chapter_11/Stencil_Test/Stencil_Test.h"
#include "webkit/glue/plugins/test/plugin_test.h"

namespace NPAPIClient {

// This class contains a list of windowed plugin tests. Please add additional
// tests to this class.
class Pepper3DTest : public PluginTest {
 public:
  Pepper3DTest(NPP id, NPNetscapeFuncs *host_functions);
  ~Pepper3DTest();

  // Pepper tests run in windowless plugin mode.
  virtual bool IsWindowless() const { return true; }

  // NPAPI functions.
  virtual NPError New(uint16 mode, int16 argc, const char* argn[],
                      const char* argv[], NPSavedData* saved);
  virtual NPError Destroy();
  virtual NPError SetWindow(NPWindow* window);

 private:
  static void RepaintCallback(NPP, NPDeviceContext3D*);

  void CreateContext();
  void DestroyContext();
  void MakeContextCurrent();
  void Paint();
  void SwapBuffers();

  void InitGL();
  void ReleaseGL();
  void DrawGL();
  void TestGL();
  void TestPixel(int x, int y, const GLubyte expected_color[3]);

  NPExtensions* pepper_extensions_;
  NPDevice* device_3d_;
#if defined(ENABLE_NEW_NPDEVICE_API)
  NPDeviceContext3D* context_3d_;
#else
  NPDeviceContext3D context_3d_;
#endif
  PGLContext pgl_context_;

  ESContext es_context_;
  STUserData es_data_;
};

} // namespace NPAPIClient

#endif  // WEBKIT_TOOLS_NPAPI_PEPPER_TEST_PLUGIN_PEPPER_3D_TEST_H
