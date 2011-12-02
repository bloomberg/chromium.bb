// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_PLUGINS_PPAPI_PPB_IMAGE_DATA_IMPL_H_
#define WEBKIT_PLUGINS_PPAPI_PPB_IMAGE_DATA_IMPL_H_

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "ppapi/c/ppb_image_data.h"
#include "ppapi/shared_impl/image_data_impl.h"
#include "ppapi/shared_impl/resource.h"
#include "ppapi/thunk/ppb_image_data_api.h"
#include "webkit/plugins/ppapi/plugin_delegate.h"
#include "webkit/plugins/webkit_plugins_export.h"

namespace skia {
class PlatformCanvas;
}

class SkBitmap;

namespace webkit {
namespace ppapi {

class PPB_ImageData_Impl : public ::ppapi::Resource,
                           public ::ppapi::ImageDataImpl,
                           public ::ppapi::thunk::PPB_ImageData_API {
 public:
  // If you call this constructor, you must also call Init before use. Normally
  // you should use the static Create function, but this constructor is needed
  // for some internal uses of ImageData (like Graphics2D).
  WEBKIT_PLUGINS_EXPORT explicit PPB_ImageData_Impl(PP_Instance instance);
  virtual ~PPB_ImageData_Impl();

  static PP_Resource Create(PP_Instance pp_instance,
                            PP_ImageDataFormat format,
                            const PP_Size& size,
                            PP_Bool init_to_zero);

  WEBKIT_PLUGINS_EXPORT bool Init(PP_ImageDataFormat format,
                                  int width, int height,
                                  bool init_to_zero);

  int width() const { return width_; }
  int height() const { return height_; }

  // Returns the image format.
  PP_ImageDataFormat format() const { return format_; }

  // Returns true if this image is mapped. False means that the image is either
  // invalid or not mapped. See ImageDataAutoMapper below.
  bool is_mapped() const { return !!mapped_canvas_.get(); }

  PluginDelegate::PlatformImage2D* platform_image() const {
    return platform_image_.get();
  }

  virtual ::ppapi::thunk::PPB_ImageData_API* AsPPB_ImageData_API() OVERRIDE;

  // PPB_ImageData_API implementation.
  virtual PP_Bool Describe(PP_ImageDataDesc* desc) OVERRIDE;
  virtual void* Map() OVERRIDE;
  virtual void Unmap() OVERRIDE;
  virtual int32_t GetSharedMemory(int* handle, uint32_t* byte_count) OVERRIDE;

  // The mapped bitmap and canvas will be NULL if the image is not mapped.
  skia::PlatformCanvas* mapped_canvas() const { return mapped_canvas_.get(); }
  const SkBitmap* GetMappedBitmap() const;

  // Swaps the guts of this image data with another.
  void Swap(PPB_ImageData_Impl* other);

 private:
  // This will be NULL before initialization, and if this PPB_ImageData_Impl is
  // swapped with another.
  scoped_ptr<PluginDelegate::PlatformImage2D> platform_image_;

  // When the device is mapped, this is the image. Null when umapped.
  scoped_ptr<skia::PlatformCanvas> mapped_canvas_;

  PP_ImageDataFormat format_;
  int width_;
  int height_;

  DISALLOW_COPY_AND_ASSIGN(PPB_ImageData_Impl);
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
  explicit ImageDataAutoMapper(PPB_ImageData_Impl* image_data)
        : image_data_(image_data) {
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
  PPB_ImageData_Impl* image_data_;
  bool is_valid_;
  bool needs_unmap_;

  DISALLOW_COPY_AND_ASSIGN(ImageDataAutoMapper);
};

}  // namespace ppapi
}  // namespace webkit

#endif  // WEBKIT_PLUGINS_PPAPI_PPB_IMAGE_DATA_IMPL_H_
