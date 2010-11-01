// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_CPP_IMAGE_DATA_H_
#define PPAPI_CPP_IMAGE_DATA_H_

#include "ppapi/c/ppb_image_data.h"
#include "ppapi/cpp/point.h"
#include "ppapi/cpp/size.h"
#include "ppapi/cpp/resource.h"

namespace pp {

class Plugin;

class ImageData : public Resource {
 public:
  // Creates an is_null() ImageData object.
  ImageData();

  // This magic constructor is used when we've gotten a PP_Resource as a return
  // value that has already been addref'ed for us.
  struct PassRef {};
  ImageData(PassRef, PP_Resource resource);

  ImageData(const ImageData& other);

  // Allocates a new ImageData in the browser with the given parameters. The
  // resulting object will be is_null() if the allocation failed.
  ImageData(PP_ImageDataFormat format,
            const Size& size,
            bool init_to_zero);

  virtual ~ImageData();

  ImageData& operator=(const ImageData& other);
  void swap(ImageData& other);

  // Returns the browser's preferred format for images. Using this format
  // guarantees no extra conversions will occur when painting.
  static PP_ImageDataFormat GetNativeImageDataFormat();

  PP_ImageDataFormat format() const { return desc_.format; }

  pp::Size size() const { return desc_.size; }
  int32_t stride() const { return desc_.stride; }

  void* data() const { return data_; }

  // Helper function to retrieve the address of the given pixel for 32-bit
  // pixel formats.
  const uint32_t* GetAddr32(const Point& coord) const;
  uint32_t* GetAddr32(const Point& coord);

 private:
  void PassRefAndInitData(PP_Resource resource);

  PP_ImageDataDesc desc_;
  void* data_;
};

}  // namespace pp

#endif  // PPAPI_CPP_IMAGE_DATA_H_
