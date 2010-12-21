// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/tools/npapi_pepper_test_plugin/pepper_3d_test.h"

namespace {
const int32 kCommandBufferSize = 1024 * 1024;
}  // namespace

namespace NPAPIClient {

Pepper3DTest::Pepper3DTest(NPP id, NPNetscapeFuncs *host_functions)
    : PluginTest(id, host_functions),
      pepper_extensions_(NULL),
      device_3d_(NULL),
#if defined(ENABLE_NEW_NPDEVICE_API)
      context_3d_(NULL),
#endif
      pgl_context_(PGL_NO_CONTEXT) {
#if !defined(ENABLE_NEW_NPDEVICE_API)
  memset(&context_3d_, 0, sizeof(context_3d_));
#endif

  esInitContext(&es_context_);
  memset(&es_data_, 0, sizeof(es_data_));
  es_context_.userData = &es_data_;
}

Pepper3DTest::~Pepper3DTest() {
}

NPError Pepper3DTest::New(uint16 mode, int16 argc, const char* argn[],
                          const char* argv[], NPSavedData* saved) {
  return PluginTest::New(mode, argc, argn, argv, saved);
}

NPError Pepper3DTest::Destroy() {
  DestroyContext();
  pglTerminate();
  return NPERR_NO_ERROR;
}

NPError Pepper3DTest::SetWindow(NPWindow* window) {
  // Create context if needed.
  CreateContext();

  es_context_.width = window->width;
  es_context_.height = window->height;

  return NPERR_NO_ERROR;
}

void Pepper3DTest::RepaintCallback(NPP npp, NPDeviceContext3D* /* context */) {
  Pepper3DTest* plugin = static_cast<Pepper3DTest*>(npp->pdata);
  plugin->Paint();
}

void Pepper3DTest::CreateContext() {
  if (pgl_context_ != PGL_NO_CONTEXT)
    return;

  HostFunctions()->getvalue(id(), NPNVPepperExtensions, &pepper_extensions_);
  if (pepper_extensions_ == NULL) {
    SetError("Could not acquire pepper extensions");
    SignalTestCompleted();
    return;
  }

  device_3d_ = pepper_extensions_->acquireDevice(id(), NPPepper3DDevice);
  if (device_3d_ == NULL) {
    SetError("Could not acquire 3D device");
    SignalTestCompleted();
    return;
  }

  // Initialize a 3D context.
#if defined(ENABLE_NEW_NPDEVICE_API)
  int32 attrib_list[] = {
    NP3DAttrib_CommandBufferSize, kCommandBufferSize,
    NPAttrib_End
  };
  if (device_3d_->createContext(id(), 0, attrib_list,
      reinterpret_cast<NPDeviceContext**>(&context_3d_)) != NPERR_NO_ERROR) {
    SetError("Could not initialize 3D context");
    SignalTestCompleted();
    return;
  }

  device_3d_->registerCallback(
      id(),
      context_3d_,
      NP3DCallback_Repaint,
      reinterpret_cast<NPDeviceGenericCallbackPtr>(RepaintCallback),
      NULL);
#else
  NPDeviceContext3DConfig config;
  config.commandBufferSize = kCommandBufferSize;
  if (device_3d_->initializeContext(id(), &config, &context_3d_)
      != NPERR_NO_ERROR) {
    SetError("Could not initialize 3D context");
    SignalTestCompleted();
    return;
  }
  context_3d_.repaintCallback = RepaintCallback;
#endif  // ENABLE_NEW_NPDEVICE_API

  // Initialize PGL and create a PGL context.
  if (!pglInitialize()) {
    SetError("Could not initialize PGL");
    SignalTestCompleted();
    return;
  }
#if defined(ENABLE_NEW_NPDEVICE_API)
  pgl_context_ = pglCreateContext(id(), device_3d_, context_3d_);
#else
  pgl_context_ = pglCreateContext(id(), device_3d_, &context_3d_);
#endif
  if (pgl_context_ == PGL_NO_CONTEXT) {
    SetError("Could not initialize PGL context");
    SignalTestCompleted();
    return;
  }

  // Initialize OpenGL.
  MakeContextCurrent();
  InitGL();
  pglMakeCurrent(PGL_NO_CONTEXT);
}

void Pepper3DTest::DestroyContext() {
  if (pgl_context_ == PGL_NO_CONTEXT)
    return;

  MakeContextCurrent();
  ReleaseGL();
  if (!pglDestroyContext(pgl_context_)) {
    SetError("Could not destroy PGL context");
  }
  pgl_context_ = PGL_NO_CONTEXT;

#if defined(ENABLE_NEW_NPDEVICE_API)
  if (device_3d_->destroyContext(id(), context_3d_) != NPERR_NO_ERROR) {
    SetError("Could not destroy 3D context");
  }
#else
  if (device_3d_->destroyContext(id(), &context_3d_) != NPERR_NO_ERROR) {
    SetError("Could not destroy 3D context");
  }
#endif
}

void Pepper3DTest::MakeContextCurrent() {
  DCHECK(pgl_context_ != PGL_NO_CONTEXT);

  if (!pglMakeCurrent(pgl_context_)) {
    SetError("Could not make PGL context current");
  }
}

void Pepper3DTest::Paint() {
  MakeContextCurrent();
  DrawGL();
  TestGL();
  SwapBuffers();
  pglMakeCurrent(PGL_NO_CONTEXT);

  // Painting once is enough to check correctness.
  SignalTestCompleted();
}

void Pepper3DTest::SwapBuffers() {
  if (!pglSwapBuffers()) {
    SetError("Could not swap buffers");
  }
}

void Pepper3DTest::InitGL() {
  if (!stInit(&es_context_)) {
    SetError("Could not initialize OpenGL resources");
  }
}

void Pepper3DTest::ReleaseGL() {
  stShutDown(&es_context_);
}

void Pepper3DTest::DrawGL() {
  stDraw(&es_context_);
}

void Pepper3DTest::TestGL() {
  // NW quadrant is red.
  GLint x = es_context_.width / 4;
  GLint y = (3 * es_context_.height) / 4;
  GLubyte red_color[3] = {255, 0, 0};
  TestPixel(x, y, red_color);

  // NE quadrant is green.
  x = (3 * es_context_.width) / 4;
  y = (3 * es_context_.height) / 4;
  GLubyte green_color[3] = {0, 255, 0};
  TestPixel(x, y, green_color);

  // SW quadrant is blue.
  x = es_context_.width / 4;
  y = es_context_.height / 4;
  GLubyte blue_color[3] = {0, 0, 255};
  TestPixel(x, y, blue_color);

  // SE quadrant is yellow.
  x = (3 * es_context_.width) / 4;
  y = es_context_.height / 4;
  GLubyte yellow_color[3] = {255, 255, 0};
  TestPixel(x, y, yellow_color);

  // Mid-point is black.
  x = es_context_.width / 2;
  y = es_context_.height / 2;
  GLubyte black_color[3] = {0, 0, 0};
  TestPixel(x, y, black_color);
}

void Pepper3DTest::TestPixel(int x, int y, const GLubyte expected_color[3]) {
  GLubyte pixel_color[4];
  glReadPixels(x, y, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel_color);

  ExpectIntegerEqual(pixel_color[0], expected_color[0]);
  ExpectIntegerEqual(pixel_color[1], expected_color[1]);
  ExpectIntegerEqual(pixel_color[2], expected_color[2]);
}

}  // namespace NPAPIClient
