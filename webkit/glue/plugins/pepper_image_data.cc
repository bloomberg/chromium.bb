// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/glue/plugins/pepper_image_data.h"

#include "base/scoped_ptr.h"
#include "skia/ext/platform_canvas.h"
#include "third_party/ppapi/c/pp_instance.h"
#include "third_party/ppapi/c/pp_module.h"
#include "third_party/ppapi/c/pp_resource.h"
#include "third_party/ppapi/c/ppb_image_data.h"
#include "webkit/glue/plugins/pepper_plugin_instance.h"
#include "webkit/glue/plugins/pepper_plugin_module.h"
#include "webkit/glue/plugins/pepper_resource_tracker.h"

namespace pepper {

namespace {

ImageData* ResourceAsImageData(PP_Resource resource) {
  scoped_refptr<Resource> image_resource =
      ResourceTracker::Get()->GetResource(resource);
  if (!image_resource.get())
    return NULL;
  return image_resource->AsImageData();
}

PP_Resource Create(PP_Module module_id,
                   PP_ImageDataFormat format,
                   int32_t width,
                   int32_t height) {
  PluginModule* module = PluginModule::FromPPModule(module_id);
  if (!module)
    return NullPPResource();

  scoped_refptr<ImageData> data(new ImageData(module));
  if (!data->Init(format, width, height))
    return NullPPResource();
  data->AddRef();  // AddRef for the caller.

  return data->GetResource();
}

bool IsImageData(PP_Resource resource) {
  scoped_refptr<Resource> image_resource =
      ResourceTracker::Get()->GetResource(resource);
  if (!image_resource.get())
    return false;
  return !!image_resource->AsImageData();
}

bool Describe(PP_Resource resource,
              PP_ImageDataDesc* desc) {
  // Give predictable values on failure.
  memset(desc, 0, sizeof(PP_ImageDataDesc));

  ImageData* image_data = ResourceAsImageData(resource);
  if (!image_data)
    return false;
  image_data->Describe(desc);
  return true;
}

void* Map(PP_Resource resource) {
  ImageData* image_data = ResourceAsImageData(resource);
  if (!image_data)
    return NULL;
  return image_data->Map();
}

void Unmap(PP_Resource resource) {
  ImageData* image_data = ResourceAsImageData(resource);
  if (!image_data)
    return;
  return image_data->Unmap();
}

const PPB_ImageData ppb_imagedata = {
  &Create,
  &IsImageData,
  &Describe,
  &Map,
  &Unmap,
};

}  // namespace

ImageData::ImageData(PluginModule* module)
    : Resource(module),
      width_(0),
      height_(0) {
}

ImageData::~ImageData() {
}

// static
const PPB_ImageData* ImageData::GetInterface() {
  return &ppb_imagedata;
}

bool ImageData::Init(PP_ImageDataFormat format,
                     int width,
                     int height) {
  // TODO(brettw) this should be called only on the main thread!
  platform_image_.reset(
      module()->GetSomeInstance()->delegate()->CreateImage2D(width, height));
  width_ = width;
  height_ = height;
  return !!platform_image_.get();
}

void ImageData::Describe(PP_ImageDataDesc* desc) const {
  desc->format = PP_IMAGEDATAFORMAT_BGRA_PREMUL;
  desc->width = width_;
  desc->height = height_;
  desc->stride = width_ * 4;
}

void* ImageData::Map() {
  if (!mapped_canvas_.get()) {
    mapped_canvas_.reset(platform_image_->Map());
    if (!mapped_canvas_.get())
      return NULL;
  }
  const SkBitmap& bitmap =
      mapped_canvas_->getTopPlatformDevice().accessBitmap(true);

  // Our platform bitmaps are set to opaque by default, which we don't want.
  const_cast<SkBitmap&>(bitmap).setIsOpaque(false);

  bitmap.lockPixels();
  return bitmap.getAddr32(0, 0);
}

void ImageData::Unmap() {
  // This is currently unimplemented, which is OK. The data will just always
  // be around once it's mapped. Chrome's TransportDIB isn't currently
  // unmappable without freeing it, but this may be something we want to support
  // in the future to save some memory.
}

const SkBitmap& ImageData::GetMappedBitmap() const {
  return mapped_canvas_->getTopPlatformDevice().accessBitmap(false);
}

}  // namespace pepper
