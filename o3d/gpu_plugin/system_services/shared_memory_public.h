// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef O3D_GPU_PLUGIN_SYSTEM_SERVICES_SHARED_MEMORY_PUBLIC_H_
#define O3D_GPU_PLUGIN_SYSTEM_SERVICES_SHARED_MEMORY_PUBLIC_H_

// Mapped memory range. Size is zero and pointer is null until object is
// mapped. Each process needs to separately map the object. It is safe to
// map an already mapped object.

#include "third_party/npapi/bindings/npruntime.h"

#if defined(__cplusplus)

struct CHRSharedMemory : NPObject {
  size_t size;
  void* ptr;
  void* handle;
};

#else

typedef struct _CHRSharedMemory  {
  NPObject object;
  size_t size;
  void* ptr;
  void* handle;
} CHRSharedMemory;

#endif

#endif  // O3D_GPU_PLUGIN_SYSTEM_SERVICES_SHARED_MEMORY_PUBLIC_H_
