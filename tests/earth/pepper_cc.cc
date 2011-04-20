/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

// NaCl Earth demo
// Pepper code in C++

#include "native_client/tests/earth/earth.h"
#include "native_client/common/worker.h"
// Pepper includes
#include "native_client/src/shared/ppapi_proxy/utility.h"
#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/cpp/completion_callback.h"
#include <ppapi/cpp/graphics_2d.h>
#include <ppapi/cpp/image_data.h>
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/module.h"
#include <ppapi/cpp/rect.h>
#include <ppapi/cpp/size.h>


void RepaintCallback(void* data, int32_t result);

class GlobeInstance : public pp::Instance {
 public:
  explicit GlobeInstance(PP_Instance instance) : pp::Instance(instance),
                                                 ready_(false),
                                                 window_width_(0),
                                                 window_height_(0) {
    DebugPrintf("GlobeInstance::GlobeInstance()\n");
  }

  virtual bool Init(uint32_t argc, const char* argn[], const char* argv[]) {
    Earth_Init(argc, argn, argv);

    return true;
  }

  virtual void DidChangeView(const pp::Rect& position, const pp::Rect& clip) {
    size_ = position.size();
    if (false == ready_) {
      // Note: This example does not use translucent pixels.
      // PPAPI uses pre-multiplied alpha.
      pixel_buffer_ =  new pp::ImageData(this,
                                         PP_IMAGEDATAFORMAT_BGRA_PREMUL,
                                         size_, PP_TRUE);
      if (!pixel_buffer_) {
        DebugPrintf("couldn't allocate pixel_buffer_.\n");
        return;
      }
      graphics_2d_context_ = new pp::Graphics2D(this, size_, false);
      if (!BindGraphics(*graphics_2d_context_)) {
        DebugPrintf("couldn't bind the device context.\n");
        return;
      }
      ready_ = true;
      if (window_width_ != position.size().width() ||
          window_height_ != position.size().height()) {
        // Got a resize, repaint the plugin.
        window_width_ = position.size().width();
        window_height_ = position.size().height();
        Repaint();
      }
    }
  }

  void Repaint() {
    if (ready_ != true) return;

    Earth_Draw(static_cast<uint32_t*>(pixel_buffer_->data()));

    // Don't use ReplaceContents; it causes the image to flicker!
    graphics_2d_context_->PaintImageData(*pixel_buffer_, pp::Point());
    graphics_2d_context_->Flush(pp::CompletionCallback(&RepaintCallback, this));
  }

 private:
  bool ready_;
  int window_width_;
  int window_height_;
  pp::Size size_;
  pp::ImageData* pixel_buffer_;
  pp::Graphics2D* graphics_2d_context_;
};

void RepaintCallback(void* data, int32_t result) {
  static_cast<GlobeInstance*>(data)->Repaint();
}

class GlobeModule : public pp::Module {
 public:
  // Override CreateInstance to create your customized Instance object.
  virtual pp::Instance* CreateInstance(PP_Instance instance) {
    return new GlobeInstance(instance);
  }
};

namespace pp {

// factory function for your specialization of the Module object
Module* CreateModule() {
  Module* mm;
  mm = new GlobeModule();
  return mm;
}

}  // namespace pp
