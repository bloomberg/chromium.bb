// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/cpp/completion_callback.h"
#include "ppapi/cpp/dev/font_dev.h"
#include "ppapi/cpp/graphics_2d.h"
#include "ppapi/cpp/image_data.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/rect.h"
#include "ppapi/cpp/size.h"

static void DummyCompletionCallback(void* /*user_data*/, int32_t /*result*/) {
}

class MyInstance : public pp::Instance {
 public:
  MyInstance(PP_Instance instance)
      : pp::Instance(instance) {
  }

  virtual void ViewChanged(const pp::Rect& position, const pp::Rect& clip) {
    if (position.size() == last_size_)
      return;
    last_size_ = position.size();

    pp::ImageData image(PP_IMAGEDATAFORMAT_BGRA_PREMUL, last_size_, true);
    pp::Graphics2D device(last_size_, false);
    BindGraphics(device);

    pp::FontDescription_Dev desc;
    desc.set_family(PP_FONTFAMILY_SANSSERIF);
    desc.set_size(30);
    pp::Font_Dev font(desc);

    pp::Rect text_clip(position.size());  // Use entire bounds for clip.
    font.DrawTextAt(&image,
        pp::TextRun_Dev("\xD9\x85\xD8\xB1\xD8\xAD\xD8\xA8\xD8\xA7\xE2\x80\x8E",
                         true, true),
        pp::Point(10, 40), 0xFF008000, clip, false);
    font.DrawTextAt(&image, pp::TextRun_Dev("Hello"),
        pp::Point(10, 80), 0xFF000080, text_clip, false);

    device.PaintImageData(image, pp::Point(0, 0));
    device.Flush(pp::CompletionCallback(&DummyCompletionCallback, NULL));
  }

 private:
  pp::Size last_size_;
};

class MyModule : public pp::Module {
 public:
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
