// Copyright (c) 2011 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include <stdint.h>

#include <cmath>
#include <limits>
#include <string>

#include <nacl/nacl_check.h>
#include <nacl/nacl_log.h>

#include "ppapi/c/pp_rect.h"
#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/cpp/rect.h"
#include "ppapi/cpp/graphics_2d.h"
#include "ppapi/cpp/image_data.h"
#include "ppapi/cpp/completion_callback.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/module.h"

const uint32_t kDefaultColor = 0x2266aa;

void FillRect(pp::ImageData* image, int left, int top, int width, int height,
              uint32_t color) {
  for (int y = std::max(0, top);
       y < std::min(image->size().height() - 1, top + height);
       y++) {
    for (int x = std::max(0, left);
         x < std::min(image->size().width() - 1, left + width);
         x++)
      *image->GetAddr32(pp::Point(x, y)) = color;
  }
}

class MyInstance : public pp::Instance {
 private:
  uint32_t color_;
  pp::Size size_;
  pp::Graphics2D device_context_;

  void ParseArgs(uint32_t argc, const char* argn[], const char* argv[]) {
    for (uint32_t i = 0; i < argc; ++i) {
      const std::string tag = argn[i];
      if (tag == "color") color_ = strtol(argv[i], 0, 0);
    }
  }

  static void FlushCallback(void* thiz, int32_t result) {
    // If necessary we can get at the instabce like so:
    // MyInstance* instance = static_cast<MyInstance*>(thiz);
  }

 public:
  explicit MyInstance(PP_Instance instance)
    : pp::Instance(instance), color_(kDefaultColor), size_(0, 0) {}

  virtual bool Init(uint32_t argc, const char* argn[], const char* argv[]) {
    ParseArgs(argc, argn, argv);
    return true;
  }

  virtual void DidChangeView(const pp::Rect& position, const pp::Rect& clip) {
    if (position.size().width() == size_.width() &&
        position.size().height() == size_.height()) {
      // No change. We don't care about the position, only the size.
      return;
    }

    device_context_ = pp::Graphics2D(this, position.size(), false);
    CHECK(BindGraphics(device_context_));
    size_ = position.size();

    pp::ImageData image(this, PP_IMAGEDATAFORMAT_BGRA_PREMUL, size_, true);
    CHECK(!image.is_null());
    FillRect(&image, 0, 0, size_.width(), size_.height(), color_);

    device_context_.ReplaceContents(&image);
    pp::CompletionCallback cc(FlushCallback, this);

    device_context_.Flush(cc);
  }
};


class MyModule : public pp::Module {
 public:
  // Override CreateInstance to create your customized Instance object.
  virtual pp::Instance* CreateInstance(PP_Instance instance) {
    return new MyInstance(instance);
  }
};

namespace pp {

// Factory function for your specialization of the Module object.
Module* CreateModule() {
  return new MyModule();
}

}  // namespace pp
