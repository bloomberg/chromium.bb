// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/plugins/ppapi/ppb_image_data_impl.h"

#include <algorithm>
#include <limits>

#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "skia/ext/platform_canvas.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/ppb_image_data.h"
#include "ppapi/c/trusted/ppb_image_data_trusted.h"
#include "third_party/skia/include/core/SkColorPriv.h"
#include "webkit/plugins/ppapi/common.h"
#include "webkit/plugins/ppapi/ppapi_plugin_instance.h"

namespace webkit {
namespace ppapi {

namespace {

PP_ImageDataFormat GetNativeImageDataFormat() {
  return PPB_ImageData_Impl::GetNativeImageDataFormat();
}

PP_Bool IsImageDataFormatSupported(PP_ImageDataFormat format) {
  return BoolToPPBool(PPB_ImageData_Impl::IsImageDataFormatSupported(format));
}

PP_Resource Create(PP_Instance instance_id,
                   PP_ImageDataFormat format,
                   const PP_Size* size,
                   PP_Bool init_to_zero) {
  PluginInstance* instance = ResourceTracker::Get()->GetInstance(instance_id);
  if (!instance)
    return 0;

  scoped_refptr<PPB_ImageData_Impl> data(new PPB_ImageData_Impl(instance));
  if (!data->Init(format,
                  size->width,
                  size->height,
                  PPBoolToBool(init_to_zero))) {
    return 0;
  }

  return data->GetReference();
}

PP_Bool IsImageData(PP_Resource resource) {
  return BoolToPPBool(!!Resource::GetAs<PPB_ImageData_Impl>(resource));
}

PP_Bool Describe(PP_Resource resource, PP_ImageDataDesc* desc) {
  // Give predictable values on failure.
  memset(desc, 0, sizeof(PP_ImageDataDesc));

  scoped_refptr<PPB_ImageData_Impl> image_data(
      Resource::GetAs<PPB_ImageData_Impl>(resource));
  if (!image_data)
    return PP_FALSE;
  image_data->Describe(desc);
  return PP_TRUE;
}

void* Map(PP_Resource resource) {
  scoped_refptr<PPB_ImageData_Impl> image_data(
      Resource::GetAs<PPB_ImageData_Impl>(resource));
  if (!image_data)
    return NULL;
  return image_data->Map();
}

void Unmap(PP_Resource resource) {
  scoped_refptr<PPB_ImageData_Impl> image_data(
      Resource::GetAs<PPB_ImageData_Impl>(resource));
  if (image_data)
    image_data->Unmap();
}

int32_t GetSharedMemory(PP_Resource resource,
                        int* handle,
                        uint32_t* byte_count) {
  scoped_refptr<PPB_ImageData_Impl> image_data(
      Resource::GetAs<PPB_ImageData_Impl>(resource));
  if (image_data) {
    *handle = image_data->GetSharedMemoryHandle(byte_count);
    return PP_OK;
  }
  return PP_ERROR_BADRESOURCE;
}

const PPB_ImageData ppb_imagedata = {
  &GetNativeImageDataFormat,
  &IsImageDataFormatSupported,
  &Create,
  &IsImageData,
  &Describe,
  &Map,
  &Unmap,
};

const PPB_ImageDataTrusted ppb_imagedata_trusted = {
  &GetSharedMemory,
};

}  // namespace

PPB_ImageData_Impl::PPB_ImageData_Impl(PluginInstance* instance)
    : Resource(instance),
      format_(PP_IMAGEDATAFORMAT_BGRA_PREMUL),
      width_(0),
      height_(0) {
}

PPB_ImageData_Impl::~PPB_ImageData_Impl() {
}

// static
const PPB_ImageData* PPB_ImageData_Impl::GetInterface() {
  return &ppb_imagedata;
}

// static
const PPB_ImageDataTrusted* PPB_ImageData_Impl::GetTrustedInterface() {
  return &ppb_imagedata_trusted;
}

PPB_ImageData_Impl* PPB_ImageData_Impl::AsPPB_ImageData_Impl() {
  return this;
}

bool PPB_ImageData_Impl::Init(PP_ImageDataFormat format,
                              int width, int height,
                              bool init_to_zero) {
  // TODO(brettw) this should be called only on the main thread!
  // TODO(brettw) use init_to_zero when we implement caching.
  if (!IsImageDataFormatSupported(format))
    return false;  // Only support this one format for now.
  if (width <= 0 || height <= 0)
    return false;
  if (static_cast<int64>(width) * static_cast<int64>(height) >=
      std::numeric_limits<int32>::max())
    return false;  // Prevent overflow of signed 32-bit ints.

  platform_image_.reset(
      instance()->delegate()->CreateImage2D(width, height));
  format_ = format;
  width_ = width;
  height_ = height;
  return !!platform_image_.get();
}

void PPB_ImageData_Impl::Describe(PP_ImageDataDesc* desc) const {
  desc->format = format_;
  desc->size.width = width_;
  desc->size.height = height_;
  desc->stride = width_ * 4;
}

void* PPB_ImageData_Impl::Map() {
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

void PPB_ImageData_Impl::Unmap() {
  // This is currently unimplemented, which is OK. The data will just always
  // be around once it's mapped. Chrome's TransportDIB isn't currently
  // unmappable without freeing it, but this may be something we want to support
  // in the future to save some memory.
}

int PPB_ImageData_Impl::GetSharedMemoryHandle(uint32* byte_count) const {
  return platform_image_->GetSharedMemoryHandle(byte_count);
}

const SkBitmap* PPB_ImageData_Impl::GetMappedBitmap() const {
  if (!mapped_canvas_.get())
    return NULL;
  return &mapped_canvas_->getTopPlatformDevice().accessBitmap(false);
}

void PPB_ImageData_Impl::Swap(PPB_ImageData_Impl* other) {
  swap(other->platform_image_, platform_image_);
  swap(other->mapped_canvas_, mapped_canvas_);
  std::swap(other->format_, format_);
  std::swap(other->width_, width_);
  std::swap(other->height_, height_);
}

}  // namespace ppapi
}  // namespace webkit

