// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/plugins/ppapi/ppb_image_data_impl.h"

#include <algorithm>
#include <limits>

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "skia/ext/platform_canvas.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/ppb_image_data.h"
#include "ppapi/c/trusted/ppb_image_data_trusted.h"
#include "ppapi/thunk/thunk.h"
#include "third_party/skia/include/core/SkColorPriv.h"
#include "webkit/plugins/ppapi/common.h"
#include "webkit/plugins/ppapi/ppapi_plugin_instance.h"
#include "webkit/plugins/ppapi/resource_helper.h"

using ::ppapi::thunk::PPB_ImageData_API;

namespace webkit {
namespace ppapi {

PPB_ImageData_Impl::PPB_ImageData_Impl(PP_Instance instance)
    : Resource(instance),
      format_(PP_IMAGEDATAFORMAT_BGRA_PREMUL),
      width_(0),
      height_(0) {
}

PPB_ImageData_Impl::~PPB_ImageData_Impl() {
}

// static
PP_Resource PPB_ImageData_Impl::Create(PP_Instance instance,
                                       PP_ImageDataFormat format,
                                       const PP_Size& size,
                                       PP_Bool init_to_zero) {
  scoped_refptr<PPB_ImageData_Impl> data(new PPB_ImageData_Impl(instance));
  if (!data->Init(format, size.width, size.height, !!init_to_zero))
    return 0;
  return data->GetReference();
}

PPB_ImageData_API* PPB_ImageData_Impl::AsPPB_ImageData_API() {
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
  if (static_cast<int64>(width) * static_cast<int64>(height) * 4 >=
      std::numeric_limits<int32>::max())
    return false;  // Prevent overflow of signed 32-bit ints.

  PluginDelegate* plugin_delegate = ResourceHelper::GetPluginDelegate(this);
  if (!plugin_delegate)
    return false;

  platform_image_.reset(plugin_delegate->CreateImage2D(width, height));
  format_ = format;
  width_ = width;
  height_ = height;
  return !!platform_image_.get();
}

PP_Bool PPB_ImageData_Impl::Describe(PP_ImageDataDesc* desc) {
  desc->format = format_;
  desc->size.width = width_;
  desc->size.height = height_;
  desc->stride = width_ * 4;
  return PP_TRUE;
}

void* PPB_ImageData_Impl::Map() {
  if (!mapped_canvas_.get()) {
    mapped_canvas_.reset(platform_image_->Map());
    if (!mapped_canvas_.get())
      return NULL;
  }
  const SkBitmap& bitmap =
      skia::GetTopDevice(*mapped_canvas_)->accessBitmap(true);

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

int32_t PPB_ImageData_Impl::GetSharedMemory(int* handle,
                                            uint32_t* byte_count) {
  *handle = platform_image_->GetSharedMemoryHandle(byte_count);
  return PP_OK;
}

const SkBitmap* PPB_ImageData_Impl::GetMappedBitmap() const {
  if (!mapped_canvas_.get())
    return NULL;
  return &skia::GetTopDevice(*mapped_canvas_)->accessBitmap(false);
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

