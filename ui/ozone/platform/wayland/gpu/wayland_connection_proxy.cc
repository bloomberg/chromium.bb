// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/gpu/wayland_connection_proxy.h"

#include <utility>

#include "base/bind.h"
#include "base/process/process.h"
#include "mojo/public/cpp/bindings/associated_interface_ptr.h"
#include "third_party/khronos/EGL/egl.h"
#include "ui/ozone/common/linux/drm_util_linux.h"
#include "ui/ozone/platform/wayland/gpu/gbm_surfaceless_wayland.h"
#include "ui/ozone/platform/wayland/gpu/wayland_surface_factory.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"

namespace ui {

WaylandConnectionProxy::WaylandConnectionProxy(WaylandConnection* connection,
                                               WaylandSurfaceFactory* factory)
    : connection_(connection),
      factory_(factory),
      associated_binding_(this),
      gpu_thread_runner_(base::ThreadTaskRunnerHandle::Get()) {}

WaylandConnectionProxy::~WaylandConnectionProxy() = default;

void WaylandConnectionProxy::SetWaylandConnection(
    ozone::mojom::WaylandConnectionPtr wc_ptr) {
  // This is an IO child thread. To satisfy our needs, we pass interface here
  // and bind it again on a gpu main thread, where buffer swaps happen.
  wc_ptr_info_ = wc_ptr.PassInterface();
}

void WaylandConnectionProxy::ResetGbmDevice() {
#if defined(WAYLAND_GBM)
  gbm_device_.reset();
#else
  NOTREACHED();
#endif
}

void WaylandConnectionProxy::OnSubmission(gfx::AcceleratedWidget widget,
                                          uint32_t buffer_id,
                                          gfx::SwapResult swap_result) {
  DCHECK(gpu_thread_runner_->BelongsToCurrentThread());
  DCHECK_NE(widget, gfx::kNullAcceleratedWidget);
  auto* surface = factory_->GetSurface(widget);
  // There can be a race between destruction and submitting the last frames. The
  // surface can be destroyed by the time the host receives a request to destroy
  // a buffer, and is able to call the OnSubmission for that specific buffer.
  if (surface)
    surface->OnSubmission(buffer_id, swap_result);
}

void WaylandConnectionProxy::OnPresentation(
    gfx::AcceleratedWidget widget,
    uint32_t buffer_id,
    const gfx::PresentationFeedback& feedback) {
  DCHECK(gpu_thread_runner_->BelongsToCurrentThread());
  DCHECK_NE(widget, gfx::kNullAcceleratedWidget);
  auto* surface = factory_->GetSurface(widget);
  // There can be a race between destruction and presenting the last frames. The
  // surface can be destroyed by the time the host receives a request to destroy
  // a buffer, and is able to call the OnPresentation for that specific buffer.
  if (surface)
    surface->OnPresentation(buffer_id, feedback);
}

void WaylandConnectionProxy::CreateZwpLinuxDmabuf(
    gfx::AcceleratedWidget widget,
    base::File file,
    gfx::Size size,
    const std::vector<uint32_t>& strides,
    const std::vector<uint32_t>& offsets,
    const std::vector<uint64_t>& modifiers,
    uint32_t current_format,
    uint32_t planes_count,
    uint32_t buffer_id) {
  DCHECK(gpu_thread_runner_);
  // Do a mojo call on the GpuMainThread instead of the io child thread to
  // ensure proper functionality.
  gpu_thread_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&WaylandConnectionProxy::CreateZwpLinuxDmabufInternal,
                     base::Unretained(this), widget, std::move(file),
                     std::move(size), std::move(strides), std::move(offsets),
                     std::move(modifiers), current_format, planes_count,
                     buffer_id));
}

void WaylandConnectionProxy::CreateZwpLinuxDmabufInternal(
    gfx::AcceleratedWidget widget,
    base::File file,
    gfx::Size size,
    const std::vector<uint32_t>& strides,
    const std::vector<uint32_t>& offsets,
    const std::vector<uint64_t>& modifiers,
    uint32_t current_format,
    uint32_t planes_count,
    uint32_t buffer_id) {
  // The interface pointer is passed on an IO child thread, which is different
  // from the thread, which is used to call these methods. Thus, rebind the
  // interface on a first call to ensure mojo calls will always happen on a
  // sequence we want.
  if (!wc_ptr_.is_bound())
    BindHostInterface();

  DCHECK(gpu_thread_runner_->BelongsToCurrentThread());
  DCHECK(wc_ptr_);
  wc_ptr_->CreateZwpLinuxDmabuf(widget, std::move(file), size, strides, offsets,
                                modifiers, current_format, planes_count,
                                buffer_id);
}

void WaylandConnectionProxy::DestroyZwpLinuxDmabuf(
    gfx::AcceleratedWidget widget,
    uint32_t buffer_id) {
  DCHECK(gpu_thread_runner_);

  // Do a mojo call on the GpuMainThread instead of the io child thread to
  // ensure proper functionality.
  gpu_thread_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&WaylandConnectionProxy::DestroyZwpLinuxDmabufInternal,
                     base::Unretained(this), widget, buffer_id));
}

void WaylandConnectionProxy::DestroyZwpLinuxDmabufInternal(
    gfx::AcceleratedWidget widget,
    uint32_t buffer_id) {
  DCHECK(gpu_thread_runner_->BelongsToCurrentThread());
  DCHECK(wc_ptr_);

  wc_ptr_->DestroyZwpLinuxDmabuf(widget, buffer_id);
}

void WaylandConnectionProxy::CommitBuffer(gfx::AcceleratedWidget widget,
                                          uint32_t buffer_id,
                                          const gfx::Rect& damage_region) {
  DCHECK(gpu_thread_runner_->BelongsToCurrentThread());
  DCHECK(wc_ptr_);

  wc_ptr_->CommitBuffer(widget, buffer_id, damage_region);
}

void WaylandConnectionProxy::CreateShmBufferForWidget(
    gfx::AcceleratedWidget widget,
    base::File file,
    size_t length,
    const gfx::Size size) {
  if (!wc_ptr_.is_bound())
    BindHostInterface();

  DCHECK(wc_ptr_);
  wc_ptr_->CreateShmBufferForWidget(widget, std::move(file), length, size);
}

void WaylandConnectionProxy::PresentShmBufferForWidget(
    gfx::AcceleratedWidget widget,
    const gfx::Rect& damage) {
  DCHECK(wc_ptr_);
  wc_ptr_->PresentShmBufferForWidget(widget, damage);
}

void WaylandConnectionProxy::DestroyShmBuffer(gfx::AcceleratedWidget widget) {
  DCHECK(wc_ptr_);
  wc_ptr_->DestroyShmBuffer(widget);
}

WaylandWindow* WaylandConnectionProxy::GetWindow(
    gfx::AcceleratedWidget widget) const {
  if (connection_)
    return connection_->GetWindow(widget);
  return nullptr;
}

void WaylandConnectionProxy::ScheduleFlush() {
  if (connection_)
    return connection_->ScheduleFlush();

  LOG(FATAL) << "Flush mustn't be called directly on the WaylandConnection, "
                "when multi-process moe is used";
}

intptr_t WaylandConnectionProxy::Display() const {
  if (connection_)
    return reinterpret_cast<intptr_t>(connection_->display());

#if defined(WAYLAND_GBM)
  return EGL_DEFAULT_DISPLAY;
#else
  return 0;
#endif
}

void WaylandConnectionProxy::AddBindingWaylandConnectionClient(
    ozone::mojom::WaylandConnectionClientRequest request) {
  bindings_.AddBinding(this, std::move(request));
}

void WaylandConnectionProxy::BindHostInterface() {
  DCHECK(!wc_ptr_.is_bound());
  wc_ptr_.Bind(std::move(wc_ptr_info_));

  // Setup associated interface.
  ozone::mojom::WaylandConnectionClientAssociatedPtrInfo client_ptr_info;
  auto request = MakeRequest(&client_ptr_info);
  wc_ptr_->SetWaylandConnectionClient(std::move(client_ptr_info));
  associated_binding_.Bind(std::move(request));
}

}  // namespace ui
