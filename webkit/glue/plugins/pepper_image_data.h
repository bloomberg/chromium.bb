// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_PLUGINS_PEPPER_IMAGE_DATA_H_
#define WEBKIT_GLUE_PLUGINS_PEPPER_IMAGE_DATA_H_

#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "third_party/ppapi/c/ppb_image_data.h"
#include "webkit/glue/plugins/pepper_plugin_delegate.h"
#include "webkit/glue/plugins/pepper_resource.h"

namespace skia {
class PlatformCanvas;
}

struct PPB_ImageDataTrusted;
class SkBitmap;

namespace pepper {

class ImageData : public Resource {
 public:
  explicit ImageData(PluginModule* module);
  virtual ~ImageData();

  int width() const { return width_; }
  int height() const { return height_; }

  // Returns the image format. Currently there is only one format so this
  // always returns the same thing. But if you care about the formation, you
  // should probably check this so when we support multiple formats, we can't
  // forget to update your code.
  PP_ImageDataFormat format() const { return PP_IMAGEDATAFORMAT_BGRA_PREMUL; }

  // Returns true if this image is mapped. False means that the image is either
  // invalid or not mapped. See ImageDataAutoMapper below.
  bool is_mapped() const { return !!mapped_canvas_.get(); }

  PluginDelegate::PlatformImage2D* platform_image() const {
    return platform_image_.get();
  }

  // Returns a pointer to the interface implementing PPB_ImageData that is
  // exposed to the plugin.
  static const PPB_ImageData* GetInterface();
  static const PPB_ImageDataTrusted* GetTrustedInterface();

  // Resource overrides.
  virtual ImageData* AsImageData() { return this; }

  // PPB_ImageData implementation.
  bool Init(PP_ImageDataFormat format,
            int width, int height,
            bool init_to_zero);
  void Describe(PP_ImageDataDesc* desc) const;
  void* Map();
  void Unmap();

  // PPB_ImageDataTrusted implementation.
  uint64 GetNativeMemoryHandle() const;

  // The mapped bitmap and canvas will be NULL if the image is not mapped.
  skia::PlatformCanvas* mapped_canvas() const { return mapped_canvas_.get(); }
  const SkBitmap* GetMappedBitmap() const;

  // Swaps the guts of this image data with another.
  void Swap(ImageData* other);

 private:
  // This will be NULL before initialization, and if this ImageData is
  // swapped with another.
  scoped_ptr<PluginDelegate::PlatformImage2D> platform_image_;

  // When the device is mapped, this is the image. Null when umapped.
  scoped_ptr<skia::PlatformCanvas> mapped_canvas_;

  int width_;
  int height_;

  DISALLOW_COPY_AND_ASSIGN(ImageData);
};

// Manages mapping an image resource if necessary. Use this to ensure the
// image is mapped. The destructor will put the image back into the previous
// state. You must check is_valid() to make sure the image was successfully
// mapped before using it.
//
// Example:
//   ImageDataAutoMapper mapper(image_data);
//   if (!mapper.is_valid())
//     return utter_failure;
//   image_data->mapped_canvas()->blah();  // Guaranteed valid.
class ImageDataAutoMapper {
 public:
  ImageDataAutoMapper(ImageData* image_data) : image_data_(image_data) {
    if (image_data_->is_mapped()) {
      is_valid_ = true;
      needs_unmap_ = false;
    } else {
      is_valid_ = needs_unmap_ = !!image_data_->Map();
    }
  }

  ~ImageDataAutoMapper() {
    if (needs_unmap_)
      image_data_->Unmap();
  }

  // Check this to see if the image was successfully mapped. If this is false,
  // the image could not be mapped and is unusable.
  bool is_valid() const { return is_valid_; }

 private:
  ImageData* image_data_;
  bool is_valid_;
  bool needs_unmap_;

  DISALLOW_COPY_AND_ASSIGN(ImageDataAutoMapper);
};

}  // namespace pepper

#endif  // WEBKIT_GLUE_PLUGINS_PEPPER_IMAGE_DATA_H_
