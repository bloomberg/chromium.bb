// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/cpp/dev/printing_dev.h"
#include "ppapi/cpp/image_data.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/rect.h"
#include "ppapi/utility/completion_callback_factory.h"

namespace {

void FillRect(pp::ImageData* image, int left, int top, int width, int height,
              uint32_t color) {
  for (int y = std::max(0, top);
       y < std::min(image->size().height(), top + height);
       y++) {
    for (int x = std::max(0, left);
         x < std::min(image->size().width(), left + width);
         x++)
      *image->GetAddr32(pp::Point(x, y)) = color;
  }
}

}  // namespace

class MyInstance : public pp::Instance, public pp::Printing_Dev {
 public:
  explicit MyInstance(PP_Instance instance)
      : pp::Instance(instance),
        pp::Printing_Dev(this) {
  }
  virtual ~MyInstance() {}

  virtual bool Init(uint32_t argc, const char* argn[], const char* argv[]) {
    return true;
  }

  virtual uint32_t QuerySupportedPrintOutputFormats() {
    return PP_PRINTOUTPUTFORMAT_RASTER;
  }

  virtual int32_t PrintBegin(const PP_PrintSettings_Dev& print_settings) {
    return 1;
  }

  virtual pp::Resource PrintPages(
      const PP_PrintPageNumberRange_Dev* page_ranges,
      uint32_t page_range_count) {
    pp::Size size(100, 100);
    pp::ImageData image(this, PP_IMAGEDATAFORMAT_BGRA_PREMUL, size, true);
    FillRect(&image, 0, 0, size.width(), size.height(), 0xFFFFFFFF);

    FillRect(&image, 0, 0, 10, 10, 0xFF000000);
    FillRect(&image, size.width() - 11, size.height() - 11, 10, 10, 0xFF000000);
    return image;
  }

  virtual void PrintEnd() {
  }

  virtual bool IsPrintScalingDisabled() {
    return true;
  }
};

class MyModule : public pp::Module {
 public:
  MyModule() : pp::Module() {}
  virtual ~MyModule() {}

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

