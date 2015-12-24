// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_DRM_GPU_DRM_THREAD_PROXY_H_
#define UI_OZONE_PLATFORM_DRM_GPU_DRM_THREAD_PROXY_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "ui/ozone/platform/drm/gpu/drm_thread.h"

namespace ui {

class DrmThreadMessageProxy;
class DrmWindowProxy;

// Mediates the communication between GPU main/IO threads and the DRM thread. It
// serves proxy objects that are safe to call on the GPU threads. The proxy
// objects then deal with safely posting the messages to the DRM thread.
class DrmThreadProxy {
 public:
  explicit DrmThreadProxy();
  ~DrmThreadProxy();

  scoped_refptr<DrmThreadMessageProxy> CreateDrmThreadMessageProxy();

  scoped_ptr<DrmWindowProxy> CreateDrmWindowProxy(
      gfx::AcceleratedWidget widget);

  scoped_refptr<GbmBuffer> CreateBuffer(gfx::AcceleratedWidget widget,
                                        const gfx::Size& size,
                                        gfx::BufferFormat format,
                                        gfx::BufferUsage usage);

 private:
  DrmThread drm_thread_;

  DISALLOW_COPY_AND_ASSIGN(DrmThreadProxy);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_DRM_GPU_DRM_THREAD_PROXY_H_
