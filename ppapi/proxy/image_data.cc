// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/image_data.h"

#if defined(OS_LINUX)
#include <sys/shm.h>
#endif

#if defined(OS_MACOSX)
#include <sys/stat.h>
#include <sys/mman.h>
#endif

namespace pp {
namespace proxy {

ImageData::ImageData(PP_Instance instance,
                     const PP_ImageDataDesc& desc,
                     ImageHandle handle)
    : PluginResource(instance),
      desc_(desc),
      handle_(handle),
      mapped_data_(NULL) {
}

ImageData::~ImageData() {
  Unmap();
}

ImageData* ImageData::AsImageData() {
  return this;
}

void* ImageData::Map() {
#if defined(OS_WIN)
  NOTIMPLEMENTED();
  return NULL;
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
  NOTIMPLEMENTED();
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
