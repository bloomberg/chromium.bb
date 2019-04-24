// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/drm/gpu/drm_thread_proxy.h"

#include "base/bind.h"
#include "ui/ozone/platform/drm/gpu/drm_thread_message_proxy.h"
#include "ui/ozone/platform/drm/gpu/drm_window_proxy.h"
#include "ui/ozone/platform/drm/gpu/gbm_pixmap.h"
#include "ui/ozone/platform/drm/gpu/proxy_helpers.h"

namespace ui {

DrmThreadProxy::DrmThreadProxy() {}

DrmThreadProxy::~DrmThreadProxy() {}

// Used only with the paramtraits implementation.
void DrmThreadProxy::BindThreadIntoMessagingProxy(
    InterThreadMessagingProxy* messaging_proxy) {
  messaging_proxy->SetDrmThread(&drm_thread_);
}

// Used only for the mojo implementation.
void DrmThreadProxy::StartDrmThread(base::OnceClosure binding_drainer) {
  drm_thread_.Start(std::move(binding_drainer));
}

std::unique_ptr<DrmWindowProxy> DrmThreadProxy::CreateDrmWindowProxy(
    gfx::AcceleratedWidget widget) {
  return std::make_unique<DrmWindowProxy>(widget, &drm_thread_);
}

void DrmThreadProxy::CreateBuffer(gfx::AcceleratedWidget widget,
                                  const gfx::Size& size,
                                  gfx::BufferFormat format,
                                  gfx::BufferUsage usage,
                                  uint32_t flags,
                                  std::unique_ptr<GbmBuffer>* buffer,
                                  scoped_refptr<DrmFramebuffer>* framebuffer) {
  DCHECK(drm_thread_.task_runner())
      << "no task runner! in DrmThreadProxy::CreateBuffer";
  PostSyncTask(
      drm_thread_.task_runner(),
      base::BindOnce(&DrmThread::CreateBuffer, base::Unretained(&drm_thread_),
                     widget, size, format, usage, flags, buffer, framebuffer));
}

void DrmThreadProxy::CreateBufferFromFds(
    gfx::AcceleratedWidget widget,
    const gfx::Size& size,
    gfx::BufferFormat format,
    std::vector<base::ScopedFD> fds,
    const std::vector<gfx::NativePixmapPlane>& planes,
    std::unique_ptr<GbmBuffer>* buffer,
    scoped_refptr<DrmFramebuffer>* framebuffer) {
  PostSyncTask(drm_thread_.task_runner(),
               base::BindOnce(&DrmThread::CreateBufferFromFds,
                              base::Unretained(&drm_thread_), widget, size,
                              format, base::Passed(std::move(fds)), planes,
                              buffer, framebuffer));
}

void DrmThreadProxy::AddBindingCursorDevice(
    ozone::mojom::DeviceCursorRequest request) {
  drm_thread_.task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&DrmThread::AddBindingCursorDevice,
                     base::Unretained(&drm_thread_), std::move(request)));
}

void DrmThreadProxy::AddBindingDrmDevice(
    ozone::mojom::DrmDeviceRequest request) {
  DCHECK(drm_thread_.task_runner()) << "DrmThreadProxy::AddBindingDrmDevice "
                                       "drm_thread_ task runner missing";

  drm_thread_.task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&DrmThread::AddBindingDrmDevice,
                     base::Unretained(&drm_thread_), std::move(request)));
}

}  // namespace ui
