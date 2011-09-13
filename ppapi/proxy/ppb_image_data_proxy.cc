// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/ppb_image_data_proxy.h"

#include <string.h>  // For memcpy

#include <vector>

#include "base/logging.h"
#include "build/build_config.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/proxy/plugin_dispatcher.h"
#include "ppapi/proxy/plugin_resource_tracker.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/shared_impl/host_resource.h"
#include "ppapi/shared_impl/resource.h"
#include "ppapi/thunk/thunk.h"
#include "skia/ext/platform_canvas.h"
#include "ui/gfx/surface/transport_dib.h"

namespace ppapi {
namespace proxy {

ImageData::ImageData(const HostResource& resource,
                     const PP_ImageDataDesc& desc,
                     ImageHandle handle)
    : Resource(resource),
      desc_(desc) {
#if defined(OS_WIN)
  transport_dib_.reset(TransportDIB::CreateWithHandle(handle));
#else
  transport_dib_.reset(TransportDIB::Map(handle));
#endif
}

ImageData::~ImageData() {
}

thunk::PPB_ImageData_API* ImageData::AsPPB_ImageData_API() {
  return this;
}

PP_Bool ImageData::Describe(PP_ImageDataDesc* desc) {
  memcpy(desc, &desc_, sizeof(PP_ImageDataDesc));
  return PP_TRUE;
}

void* ImageData::Map() {
  if (!mapped_canvas_.get()) {
    mapped_canvas_.reset(transport_dib_->GetPlatformCanvas(desc_.size.width,
                                                           desc_.size.height));
    if (!mapped_canvas_.get())
      return NULL;
  }
  const SkBitmap& bitmap =
      skia::GetTopDevice(*mapped_canvas_)->accessBitmap(true);

  bitmap.lockPixels();
  return bitmap.getAddr(0, 0);
}

void ImageData::Unmap() {
  // TODO(brettw) have a way to unmap a TransportDIB. Currently this isn't
  // possible since deleting the TransportDIB also frees all the handles.
  // We need to add a method to TransportDIB to release the handles.
}

int32_t ImageData::GetSharedMemory(int* /* handle */,
                                   uint32_t* /* byte_count */) {
  // Not supported in the proxy (this method is for actually implementing the
  // proxy in the host).
  return PP_ERROR_NOACCESS;
}

#if defined(OS_WIN)
const ImageHandle ImageData::NullHandle = NULL;
#elif defined(OS_MACOSX)
const ImageHandle ImageData::NullHandle = ImageHandle();
#else
const ImageHandle ImageData::NullHandle = 0;
#endif

ImageHandle ImageData::HandleFromInt(int32_t i) {
#if defined(OS_WIN)
    return reinterpret_cast<ImageHandle>(i);
#elif defined(OS_MACOSX)
    return ImageHandle(i, false);
#else
    return static_cast<ImageHandle>(i);
#endif
}

}  // namespace proxy
}  // namespace ppapi
