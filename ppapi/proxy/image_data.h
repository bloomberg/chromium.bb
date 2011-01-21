// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_IMAGE_DATA_H_
#define PPAPI_PROXY_IMAGE_DATA_H_

#include "base/shared_memory.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/ppb_image_data.h"
#include "ppapi/proxy/plugin_resource.h"
#include "ppapi/proxy/serialized_structs.h"
#include "ppapi/shared_impl/image_data_impl.h"

namespace pp {
namespace proxy {

class ImageData : public PluginResource,
                  public pp::shared_impl::ImageDataImpl {
 public:
  ImageData(PP_Instance instance,
            const PP_ImageDataDesc& desc,
            ImageHandle handle);
  virtual ~ImageData();

  // Resource overrides.
  virtual ImageData* AsImageData();

  void* Map();
  void Unmap();

  const PP_ImageDataDesc& desc() const { return desc_; }

  static const ImageHandle NullHandle;
  static ImageHandle HandleFromInt(int32_t i);

 private:
  PP_ImageDataDesc desc_;
  ImageHandle handle_;

  void* mapped_data_;

  DISALLOW_COPY_AND_ASSIGN(ImageData);
};

}  // namespace proxy
}  // namespace pp

#endif  // PPAPI_PROXY_IMAGE_DATA_H_
