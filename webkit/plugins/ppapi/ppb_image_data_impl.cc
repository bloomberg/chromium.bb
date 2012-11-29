// Copyright (c) 2012 The Chromium Authors. All rights reserved.
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

PPB_ImageData_Impl::PPB_ImageData_Impl(PP_Instance instance,
                                       ImageDataType type)
    : Resource(::ppapi::OBJECT_IS_IMPL, instance),
      format_(PP_IMAGEDATAFORMAT_BGRA_PREMUL),
      width_(0),
      height_(0) {
  switch (type) {
    case PLATFORM:
      backend_.reset(new ImageDataPlatformBackend);
      return;
    case NACL:
      backend_.reset(new ImageDataNaClBackend);
      return;
    // No default: so that we get a compiler warning if any types are added.
  }
  NOTREACHED();
}

PPB_ImageData_Impl::~PPB_ImageData_Impl() {
}

bool PPB_ImageData_Impl::Init(PP_ImageDataFormat format,
                              int width, int height,
                              bool init_to_zero) {
  // TODO(brettw) this should be called only on the main thread!
  if (!IsImageDataFormatSupported(format))
    return false;  // Only support this one format for now.
  if (width <= 0 || height <= 0)
    return false;
  if (static_cast<int64>(width) * static_cast<int64>(height) >=
      std::numeric_limits<int32>::max() / 4)
    return false;  // Prevent overflow of signed 32-bit ints.

  format_ = format;
  width_ = width;
  height_ = height;
  return backend_->Init(this, format, width, height, init_to_zero);
}

// static
PP_Resource PPB_ImageData_Impl::CreatePlatform(PP_Instance instance,
                                               PP_ImageDataFormat format,
                                               const PP_Size& size,
                                               PP_Bool init_to_zero) {
  scoped_refptr<PPB_ImageData_Impl>
      data(new PPB_ImageData_Impl(instance, PLATFORM));
  if (!data->Init(format, size.width, size.height, !!init_to_zero))
    return 0;
  return data->GetReference();
}

// static
PP_Resource PPB_ImageData_Impl::CreateNaCl(PP_Instance instance,
                                               PP_ImageDataFormat format,
                                               const PP_Size& size,
                                               PP_Bool init_to_zero) {
  scoped_refptr<PPB_ImageData_Impl>
      data(new PPB_ImageData_Impl(instance, NACL));
  if (!data->Init(format, size.width, size.height, !!init_to_zero))
    return 0;
  return data->GetReference();
}

PPB_ImageData_API* PPB_ImageData_Impl::AsPPB_ImageData_API() {
  return this;
}

bool PPB_ImageData_Impl::IsMapped() const {
  return backend_->IsMapped();
}

PluginDelegate::PlatformImage2D* PPB_ImageData_Impl::PlatformImage() const {
  return backend_->PlatformImage();
}

PP_Bool PPB_ImageData_Impl::Describe(PP_ImageDataDesc* desc) {
  desc->format = format_;
  desc->size.width = width_;
  desc->size.height = height_;
  desc->stride = width_ * 4;
  return PP_TRUE;
}

void* PPB_ImageData_Impl::Map() {
  return backend_->Map();
}

void PPB_ImageData_Impl::Unmap() {
  backend_->Unmap();
}

int32_t PPB_ImageData_Impl::GetSharedMemory(int* handle, uint32_t* byte_count) {
  return backend_->GetSharedMemory(handle, byte_count);
}

skia::PlatformCanvas* PPB_ImageData_Impl::GetPlatformCanvas() {
  return backend_->GetPlatformCanvas();
}

SkCanvas* PPB_ImageData_Impl::GetCanvas() {
  return backend_->GetCanvas();
}

void PPB_ImageData_Impl::SetUsedInReplaceContents() {
  // Nothing to do since we don't support image data re-use in-process.
}

const SkBitmap* PPB_ImageData_Impl::GetMappedBitmap() const {
  return backend_->GetMappedBitmap();
}

// ImageDataPlatformBackend --------------------------------------------------

ImageDataPlatformBackend::ImageDataPlatformBackend() {
}

ImageDataPlatformBackend::~ImageDataPlatformBackend() {
}

bool ImageDataPlatformBackend::Init(PPB_ImageData_Impl* impl,
                                     PP_ImageDataFormat format,
                                     int width, int height,
                                     bool init_to_zero) {
  PluginDelegate* plugin_delegate = ResourceHelper::GetPluginDelegate(impl);
  if (!plugin_delegate)
    return false;

  // TODO(brettw) use init_to_zero when we implement caching.
  platform_image_.reset(plugin_delegate->CreateImage2D(width, height));
  return !!platform_image_.get();
}

bool ImageDataPlatformBackend::IsMapped() const {
  return !!mapped_canvas_.get();
}

PluginDelegate::PlatformImage2D*
ImageDataPlatformBackend::PlatformImage() const {
  return platform_image_.get();
}

void* ImageDataPlatformBackend::Map() {
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

void ImageDataPlatformBackend::Unmap() {
  // This is currently unimplemented, which is OK. The data will just always
  // be around once it's mapped. Chrome's TransportDIB isn't currently
  // unmappable without freeing it, but this may be something we want to support
  // in the future to save some memory.
}

int32_t ImageDataPlatformBackend::GetSharedMemory(int* handle,
                                                   uint32_t* byte_count) {
  *handle = platform_image_->GetSharedMemoryHandle(byte_count);
  return PP_OK;
}

skia::PlatformCanvas* ImageDataPlatformBackend::GetPlatformCanvas() {
  return mapped_canvas_.get();
}

SkCanvas* ImageDataPlatformBackend::GetCanvas() {
  return mapped_canvas_.get();
}

const SkBitmap* ImageDataPlatformBackend::GetMappedBitmap() const {
  if (!mapped_canvas_.get())
    return NULL;
  return &skia::GetTopDevice(*mapped_canvas_)->accessBitmap(false);
}

// ImageDataNaClBackend ------------------------------------------------------

ImageDataNaClBackend::ImageDataNaClBackend()
    : map_count_(0) {
}

ImageDataNaClBackend::~ImageDataNaClBackend() {
}

bool ImageDataNaClBackend::Init(PPB_ImageData_Impl* impl,
                                 PP_ImageDataFormat format,
                                 int width, int height,
                                 bool init_to_zero) {
  skia_bitmap_.setConfig(SkBitmap::kARGB_8888_Config,
                         impl->width(), impl->height());
  PluginDelegate* plugin_delegate = ResourceHelper::GetPluginDelegate(impl);
  if (!plugin_delegate)
    return false;
  shared_memory_.reset(
      plugin_delegate->CreateAnonymousSharedMemory(skia_bitmap_.getSize()));
  return !!shared_memory_.get();
}

bool ImageDataNaClBackend::IsMapped() const {
  return map_count_ > 0;
}

PluginDelegate::PlatformImage2D* ImageDataNaClBackend::PlatformImage() const {
  return NULL;
}

void* ImageDataNaClBackend::Map() {
  DCHECK(shared_memory_.get());
  if (map_count_++ == 0) {
    shared_memory_->Map(skia_bitmap_.getSize());
    skia_bitmap_.setPixels(shared_memory_->memory());
    // Our platform bitmaps are set to opaque by default, which we don't want.
    skia_bitmap_.setIsOpaque(false);
    skia_canvas_.reset(new SkCanvas(skia_bitmap_));
    return skia_bitmap_.getAddr32(0, 0);
  }
  return shared_memory_->memory();
}

void ImageDataNaClBackend::Unmap() {
  if (--map_count_ == 0)
    shared_memory_->Unmap();
}

int32_t ImageDataNaClBackend::GetSharedMemory(int* handle,
                                                uint32_t* byte_count) {
  *byte_count = skia_bitmap_.getSize();
#if defined(OS_POSIX)
  *handle = shared_memory_->handle().fd;
#elif defined(OS_WIN)
  *handle = reinterpret_cast<int>(shared_memory_->handle());
#else
#error "Platform not supported."
#endif
  return PP_OK;
}

skia::PlatformCanvas* ImageDataNaClBackend::GetPlatformCanvas() {
  return NULL;
}

SkCanvas* ImageDataNaClBackend::GetCanvas() {
  if (!IsMapped())
    return NULL;
  return skia_canvas_.get();
}

const SkBitmap* ImageDataNaClBackend::GetMappedBitmap() const {
  if (!IsMapped())
    return NULL;
  return &skia_bitmap_;
}

}  // namespace ppapi
}  // namespace webkit
