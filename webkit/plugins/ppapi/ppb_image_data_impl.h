// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_PLUGINS_PPAPI_PPB_IMAGE_DATA_IMPL_H_
#define WEBKIT_PLUGINS_PPAPI_PPB_IMAGE_DATA_IMPL_H_

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "ppapi/c/ppb_image_data.h"
#include "ppapi/shared_impl/ppb_image_data_shared.h"
#include "ppapi/shared_impl/resource.h"
#include "ppapi/thunk/ppb_image_data_api.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "webkit/plugins/ppapi/plugin_delegate.h"
#include "webkit/plugins/webkit_plugins_export.h"

class SkBitmap;
class SkCanvas;

namespace webkit {
namespace ppapi {

class WEBKIT_PLUGINS_EXPORT PPB_ImageData_Impl
    : public ::ppapi::Resource,
      public ::ppapi::PPB_ImageData_Shared,
      public NON_EXPORTED_BASE(::ppapi::thunk::PPB_ImageData_API) {
 public:
  // We delegate most of our implementation to a back-end class that either uses
  // a PlatformCanvas (for most trusted stuff) or bare shared memory (for use by
  // NaCl). This makes it cheap & easy to implement Swap.
  class Backend {
   public:
    virtual ~Backend() {};
    virtual bool Init(PPB_ImageData_Impl* impl, PP_ImageDataFormat format,
                      int width, int height, bool init_to_zero) = 0;
    virtual bool IsMapped() const = 0;
    virtual PluginDelegate::PlatformImage2D* PlatformImage() const = 0;
    virtual void* Map() = 0;
    virtual void Unmap() = 0;
    virtual int32_t GetSharedMemory(int* handle, uint32_t* byte_count) = 0;
    virtual SkCanvas* GetPlatformCanvas() = 0;
    virtual SkCanvas* GetCanvas() = 0;
    virtual const SkBitmap* GetMappedBitmap() const = 0;
  };

  // If you call this constructor, you must also call Init before use. Normally
  // you should use the static Create function, but this constructor is needed
  // for some internal uses of ImageData (like Graphics2D).
  enum ImageDataType { PLATFORM, NACL };
  PPB_ImageData_Impl(PP_Instance instance, ImageDataType type);
  virtual ~PPB_ImageData_Impl();

  bool Init(PP_ImageDataFormat format,
            int width, int height,
            bool init_to_zero);

  // Create an ImageData backed by a PlatformCanvas. You must use this if you
  // intend the ImageData to be usable in platform-specific APIs (like font
  // rendering or rendering widgets like scrollbars).
  static PP_Resource CreatePlatform(PP_Instance pp_instance,
                                    PP_ImageDataFormat format,
                                    const PP_Size& size,
                                    PP_Bool init_to_zero);

  // Use this to create an ImageData for use with NaCl. This is backed by a
  // simple shared memory buffer.
  static PP_Resource CreateNaCl(PP_Instance pp_instance,
                                PP_ImageDataFormat format,
                                const PP_Size& size,
                                PP_Bool init_to_zero);

  int width() const { return width_; }
  int height() const { return height_; }

  // Returns the image format.
  PP_ImageDataFormat format() const { return format_; }

  // Returns true if this image is mapped. False means that the image is either
  // invalid or not mapped. See ImageDataAutoMapper below.
  bool IsMapped() const;
  PluginDelegate::PlatformImage2D* PlatformImage() const;

  // Resource override.
  virtual ::ppapi::thunk::PPB_ImageData_API* AsPPB_ImageData_API() OVERRIDE;

  // PPB_ImageData_API implementation.
  virtual PP_Bool Describe(PP_ImageDataDesc* desc) OVERRIDE;
  virtual void* Map() OVERRIDE;
  virtual void Unmap() OVERRIDE;
  virtual int32_t GetSharedMemory(int* handle, uint32_t* byte_count) OVERRIDE;
  virtual SkCanvas* GetPlatformCanvas() OVERRIDE;
  virtual SkCanvas* GetCanvas() OVERRIDE;
  virtual void SetUsedInReplaceContents() OVERRIDE;

  const SkBitmap* GetMappedBitmap() const;

 private:
  PP_ImageDataFormat format_;
  int width_;
  int height_;
  scoped_ptr<Backend> backend_;

  DISALLOW_COPY_AND_ASSIGN(PPB_ImageData_Impl);
};

class ImageDataPlatformBackend : public PPB_ImageData_Impl::Backend {
 public:
  ImageDataPlatformBackend();
  virtual ~ImageDataPlatformBackend();

  // PPB_ImageData_Impl::Backend implementation.
  virtual bool Init(PPB_ImageData_Impl* impl, PP_ImageDataFormat format,
                    int width, int height, bool init_to_zero) OVERRIDE;
  virtual bool IsMapped() const OVERRIDE;
  virtual PluginDelegate::PlatformImage2D* PlatformImage() const OVERRIDE;
  virtual void* Map() OVERRIDE;
  virtual void Unmap() OVERRIDE;
  virtual int32_t GetSharedMemory(int* handle, uint32_t* byte_count) OVERRIDE;
  virtual SkCanvas* GetPlatformCanvas() OVERRIDE;
  virtual SkCanvas* GetCanvas() OVERRIDE;
  virtual const SkBitmap* GetMappedBitmap() const OVERRIDE;

 private:
  // This will be NULL before initialization, and if this PPB_ImageData_Impl is
  // swapped with another.
  scoped_ptr<PluginDelegate::PlatformImage2D> platform_image_;

  // When the device is mapped, this is the image. Null when umapped.
  scoped_ptr<SkCanvas> mapped_canvas_;

  DISALLOW_COPY_AND_ASSIGN(ImageDataPlatformBackend);
};

class ImageDataNaClBackend : public PPB_ImageData_Impl::Backend {
 public:
  ImageDataNaClBackend();
  virtual ~ImageDataNaClBackend();

  // PPB_ImageData_Impl::Backend implementation.
  bool Init(PPB_ImageData_Impl* impl, PP_ImageDataFormat format,
            int width, int height, bool init_to_zero) OVERRIDE;
  virtual bool IsMapped() const OVERRIDE;
  PluginDelegate::PlatformImage2D* PlatformImage() const OVERRIDE;
  virtual void* Map() OVERRIDE;
  virtual void Unmap() OVERRIDE;
  virtual int32_t GetSharedMemory(int* handle, uint32_t* byte_count) OVERRIDE;
  virtual SkCanvas* GetPlatformCanvas() OVERRIDE;
  virtual SkCanvas* GetCanvas() OVERRIDE;
  virtual const SkBitmap* GetMappedBitmap() const OVERRIDE;

 private:
  scoped_ptr<base::SharedMemory> shared_memory_;
  // skia_bitmap_ is backed by shared_memory_.
  SkBitmap skia_bitmap_;
  scoped_ptr<SkCanvas> skia_canvas_;
  uint32 map_count_;

  DISALLOW_COPY_AND_ASSIGN(ImageDataNaClBackend);
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
    if (image_data_->IsMapped()) {
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
