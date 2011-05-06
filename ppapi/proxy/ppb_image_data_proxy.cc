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
#include "ppapi/proxy/host_resource.h"
#include "ppapi/proxy/plugin_dispatcher.h"
#include "ppapi/proxy/plugin_resource_tracker.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/thunk/thunk.h"

#if defined(OS_LINUX)
#include <sys/shm.h>
#elif defined(OS_MACOSX)
#include <sys/stat.h>
#include <sys/mman.h>
#elif defined(OS_WIN)
#include <windows.h>
#endif

namespace pp {
namespace proxy {

namespace {

InterfaceProxy* CreateImageDataProxy(Dispatcher* dispatcher,
                                     const void* target_interface) {
  return new PPB_ImageData_Proxy(dispatcher, target_interface);
}

}  // namespace

// PPB_ImageData_Proxy ---------------------------------------------------------

PPB_ImageData_Proxy::PPB_ImageData_Proxy(Dispatcher* dispatcher,
                                         const void* target_interface)
    : InterfaceProxy(dispatcher, target_interface) {
}

PPB_ImageData_Proxy::~PPB_ImageData_Proxy() {
}

// static
const InterfaceProxy::Info* PPB_ImageData_Proxy::GetInfo() {
  static const Info info = {
    ::ppapi::thunk::GetPPB_ImageData_Thunk(),
    PPB_IMAGEDATA_INTERFACE,
    INTERFACE_ID_PPB_IMAGE_DATA,
    false,
    &CreateImageDataProxy,
  };
  return &info;
}

bool PPB_ImageData_Proxy::OnMessageReceived(const IPC::Message& msg) {
  return false;
}

// ImageData -------------------------------------------------------------------

ImageData::ImageData(const HostResource& resource,
                     const PP_ImageDataDesc& desc,
                     ImageHandle handle)
    : PluginResource(resource),
      desc_(desc),
      handle_(handle),
      mapped_data_(NULL) {
}

ImageData::~ImageData() {
  Unmap();
}

::ppapi::thunk::PPB_ImageData_API* ImageData::AsImageData_API() {
  return this;
}

ImageData* ImageData::AsImageData() {
  return this;
}

PP_Bool ImageData::Describe(PP_ImageDataDesc* desc) {
  memcpy(desc, &desc_, sizeof(PP_ImageDataDesc));
  return PP_TRUE;
}

void* ImageData::Map() {
#if defined(OS_WIN)
  mapped_data_ = ::MapViewOfFile(handle_, FILE_MAP_READ | FILE_MAP_WRITE,
                                 0, 0, 0);
  return mapped_data_;
#elif defined(OS_MACOSX)
  struct stat st;
  if (fstat(handle_.fd, &st) != 0)
    return NULL;
  void* memory = mmap(NULL, st.st_size, PROT_READ | PROT_WRITE,
                      MAP_SHARED, handle_.fd, 0);
  if (memory == MAP_FAILED)
    return NULL;
  mapped_data_ = memory;
  return mapped_data_;
#else
  int shmkey = handle_;
  void* address = shmat(shmkey, NULL, 0);
  // Mark for deletion in case we crash so the kernel will clean it up.
  shmctl(shmkey, IPC_RMID, 0);
  if (address == (void*)-1)
    return NULL;
  mapped_data_ = address;
  return address;
#endif
}

void ImageData::Unmap() {
#if defined(OS_WIN)
  if (mapped_data_)
    ::UnmapViewOfFile(mapped_data_);
#elif defined(OS_MACOSX)
  if (mapped_data_) {
    struct stat st;
    if (fstat(handle_.fd, &st) == 0)
      munmap(mapped_data_, st.st_size);
  }
#else
  if (mapped_data_)
    shmdt(mapped_data_);
#endif
  mapped_data_ = NULL;
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
}  // namespace pp
