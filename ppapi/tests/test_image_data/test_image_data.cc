// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/tests/test_instance.h"

#include "ppapi/cpp/device_context_2d.h"
#include "ppapi/cpp/image_data.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/module.h"

class Instance : public TestInstance {
 public:
  Instance(PP_Instance instance) : TestInstance(instance) {}

  virtual bool Init(size_t argc, const char* argn[], const char* argv[]) {
    image_data_interface_ = reinterpret_cast<PPB_ImageData const*>(
        pp::Module::Get()->GetBrowserInterface(PPB_IMAGEDATA_INTERFACE));
    return !!image_data_interface_;
  }

  virtual std::string GetTestCaseName() const {
    return std::string("ImageData");
  }

  virtual void RunTest() {
    LogTest("InvalidFormat", TestInvalidFormat());
    LogTest("InvalidSize", TestInvalidSize());
    LogTest("HugeSize", TestHugeSize());
    LogTest("InitToZero", TestInitToZero());
    LogTest("IsImageData", TestIsImageData());
  }

 private:
  std::string TestInvalidFormat() {
    pp::ImageData a(static_cast<PP_ImageDataFormat>(1337), 16, 16, true);
    if (!a.is_null())
      return "Crazy image data format accepted";

    pp::ImageData b(static_cast<PP_ImageDataFormat>(-1), 16, 16, true);
    if (!b.is_null())
      return "Negative image data format accepted";

    return "";
  }

  std::string TestInvalidSize() {
    pp::ImageData zero_size(PP_IMAGEDATAFORMAT_BGRA_PREMUL, 0, 0, true);
    if (!zero_size.is_null())
      return "Zero width and height accepted";

    pp::ImageData zero_height(PP_IMAGEDATAFORMAT_BGRA_PREMUL, 16, 0, true);
    if (!zero_height.is_null())
      return "Zero height accepted";

    pp::ImageData zero_width(PP_IMAGEDATAFORMAT_BGRA_PREMUL, 0, 16, true);
    if (!zero_width.is_null())
      return "Zero width accepted";

    pp::ImageData negative_height(PP_IMAGEDATAFORMAT_BGRA_PREMUL, 16, -2, true);
    if (!negative_height.is_null())
      return "Negative height accepted";

    pp::ImageData negative_width(PP_IMAGEDATAFORMAT_BGRA_PREMUL, -2, 16, true);
    if (!negative_width.is_null())
      return "Negative width accepted";

    return "";
  }

  std::string TestHugeSize() {
    pp::ImageData huge_size(PP_IMAGEDATAFORMAT_BGRA_PREMUL,
                            100000000, 100000000, true);
    if (!huge_size.is_null())
      return "31-bit overflow size accepted";
    return "";
  }

  std::string TestInitToZero() {
    const int w = 5;
    const int h = 6;
    pp::ImageData img(PP_IMAGEDATAFORMAT_BGRA_PREMUL, w, h, true);
    if (img.is_null())
      return "Could not create bitmap";

    // Basic validity checking of the bitmap. This also tests "describe" since
    // that's where the image data object got its imfo from.
    if (img.width() != w || img.height() != h)
      return "Wrong size";
    if (img.format() != PP_IMAGEDATAFORMAT_BGRA_PREMUL)
      return "Wrong format";
    if (img.stride() < w * 4)
      return "Stride too small";

    // Now check that everything is 0.
    for (int y = 0; y < h; y++) {
      uint32_t* row = img.GetAddr32(0, y);
      for (int x = 0; x < w; x++) {
        if (row[x] != 0)
          return "Image data isn't entirely zero";
      }
    }

    return "";
  }

  std::string TestIsImageData() {
    // Test that a NULL resource isn't an image data.
    pp::Resource null_resource;
    if (image_data_interface_->IsImageData(null_resource.pp_resource()))
      return "Null resource was reported as a valid image";

    // Make another resource type and test it.
    const int w = 16, h = 16;
    pp::DeviceContext2D device(w, h, true);
    if (device.is_null())
      return "Couldn't create device context";
    if (image_data_interface_->IsImageData(device.pp_resource()))
      return "Device context was reported as an image";

    // Make a valid image resource.
    pp::ImageData img(PP_IMAGEDATAFORMAT_BGRA_PREMUL, w, h, true);
    if (img.is_null())
      return "Couldn't create image data";
    if (!image_data_interface_->IsImageData(img.pp_resource()))
      return "Image data should be identified as an image";

    return "";
  }

  // Used by the tests that access the C API directly.
  const PPB_ImageData* image_data_interface_;
};

class Module : public pp::Module {
 public:
  Module() : pp::Module() {}
  virtual ~Module() {}

  virtual pp::Instance* CreateInstance(PP_Instance instance) {
    return new Instance(instance);
  }
};

namespace pp {

Module* CreateModule() {
  return new ::Module();
}

}  // namespace pp
