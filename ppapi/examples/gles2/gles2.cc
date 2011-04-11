// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is an example Pepper plugin that demonstrates use of the
// browser-provided OpenGLES2 interface.

#include <string.h>

#include "ppapi/c/dev/ppb_opengles_dev.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/cpp/dev/context_3d_dev.h"
#include "ppapi/cpp/dev/graphics_3d_client_dev.h"
#include "ppapi/cpp/dev/graphics_3d_dev.h"
#include "ppapi/cpp/dev/surface_3d_dev.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/rect.h"
#include "ppapi/lib/gl/include/GLES2/gl2.h"

namespace {

class GLES2DemoInstance : public pp::Instance, public pp::Graphics3DClient_Dev {
 public:
  GLES2DemoInstance(PP_Instance instance, pp::Module* module)
      : pp::Instance(instance), pp::Graphics3DClient_Dev(this),
        context_(NULL), surface_(NULL),
        callback_factory_(this), repaint_count_(0) {
    gles2_if_ = static_cast<const struct PPB_OpenGLES2_Dev*>(
        module->GetBrowserInterface(PPB_OPENGLES2_DEV_INTERFACE));
    assert(gles2_if_);
  }

  virtual ~GLES2DemoInstance() {
    delete context_;
    delete surface_;
  }

  // pp::Instance implementation (see PPP_Instance).
  virtual void DidChangeView(const pp::Rect& position_ignored,
                             const pp::Rect& clip) {
    InitGL(clip.width(), clip.height());
  }

  // Graphics3DClient_Dev implementation.
  virtual void Graphics3DContextLost() {
    assert(!"Unexpectedly lost graphics context");
  }

 private:
  // Initialize GL context & surface, and trigger first repaint.
  bool InitGL(int width, int height) {
    assert(width && height);

    if (context_)
      delete(context_);
    context_ = new pp::Context3D_Dev(*this, 0, pp::Context3D_Dev(), NULL);
    assert(!context_->is_null());

    int32_t surface_attributes[] = {
      PP_GRAPHICS3DATTRIB_WIDTH, width,
      PP_GRAPHICS3DATTRIB_HEIGHT, height,
      PP_GRAPHICS3DATTRIB_NONE
    };
    if (surface_)
      delete(surface_);
    surface_ = new pp::Surface3D_Dev(*this, 0, surface_attributes);
    assert(!surface_->is_null());

    int32_t bind_error = context_->BindSurfaces(*surface_, *surface_);
    assert(!bind_error);
    assertNoGLError();

    bool success = BindGraphics(*surface_);
    assert(success);

    FlickerAndPaint(0, true);
    return true;
  }

  // Assert |context_| isn't holding any GL Errors.
  void assertNoGLError() {
    assert(!gles2_if_->GetError(context_->pp_resource()));
  }

  // Fills |*context_| with (mostly-opaque) pixels: blue if |paint_blue|, else
  // red.  Triggers a repaint of |surface_| to the browser, and a flipped call
  // to self on completion.
  void FlickerAndPaint(int32_t result, bool paint_blue) {
    assert(!result);
    ++repaint_count_;
    float r = paint_blue ? 0 : 1;
    float g = 0;
    float b = paint_blue ? 1 : 0;
    float a = 0.75;
    gles2_if_->ClearColor(context_->pp_resource(), r, g, b, a);
    gles2_if_->Clear(context_->pp_resource(), GL_COLOR_BUFFER_BIT);
    assertNoGLError();

    pp::CompletionCallback cb = callback_factory_.NewCallback(
        &GLES2DemoInstance::FlickerAndPaint, !paint_blue);
    int32_t error = surface_->SwapBuffers(cb);
    assert(error == PP_ERROR_WOULDBLOCK);
    assertNoGLError();
  }

  // Unowned pointers.
  const struct PPB_OpenGLES2_Dev* gles2_if_;

  // Owned data.
  pp::Context3D_Dev* context_;
  pp::Surface3D_Dev* surface_;
  pp::CompletionCallbackFactory<GLES2DemoInstance> callback_factory_;
  int repaint_count_;
};

// This object is the global object representing this plugin library as long
// as it is loaded.
class GLES2DemoModule : public pp::Module {
 public:
  GLES2DemoModule() : pp::Module() {}
  virtual ~GLES2DemoModule() {}

  virtual pp::Instance* CreateInstance(PP_Instance instance) {
    return new GLES2DemoInstance(instance, this);
  }
};
}  // anonymous namespace

namespace pp {
// Factory function for your specialization of the Module object.
Module* CreateModule() {
  return new GLES2DemoModule();
}
}  // namespace pp
