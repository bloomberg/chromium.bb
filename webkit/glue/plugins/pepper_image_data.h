// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_PLUGINS_PEPPER_IMAGE_DATA_H_
#define WEBKIT_GLUE_PLUGINS_PEPPER_IMAGE_DATA_H_

#include "base/scoped_ptr.h"
#include "third_party/ppapi/c/ppb_image_data.h"
#include "webkit/glue/plugins/pepper_plugin_delegate.h"
#include "webkit/glue/plugins/pepper_resource.h"

typedef struct _ppb_ImageData PPB_ImageData;

namespace skia {
class PlatformCanvas;
}

class SkBitmap;

namespace pepper {

class PluginInstance;

class ImageData : public Resource {
 public:
  explicit ImageData(PluginModule* module);
  virtual ~ImageData();

  int width() const { return width_; }
  int height() const { return height_; }

  // Returns true if this image is valid and can be used. False means that the
  // image is invalid and can't be used.
  bool is_valid() const { return !!platform_image_.get(); }

  // Returns a pointer to the interface implementing PPB_ImageData that is
  // exposed to the plugin.
  static const PPB_ImageData* GetInterface();

  // Resource overrides.
  ImageData* AsImageData() { return this; }

  // PPB_ImageData implementation.
  bool Init(PP_ImageDataFormat format,
            int width, int height,
            bool init_to_zero);
  void Describe(PP_ImageDataDesc* desc) const;
  void* Map();
  void Unmap();

  skia::PlatformCanvas* mapped_canvas() const { return mapped_canvas_.get(); }

  const SkBitmap& GetMappedBitmap() const;

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

}  // namespace pepper

#endif  // WEBKIT_GLUE_PLUGINS_PEPPER_IMAGE_DATA_H_
