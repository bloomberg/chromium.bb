// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/gpu/wayland_connection_proxy.h"

#include "base/process/process.h"
#include "third_party/khronos/EGL/egl.h"
#include "ui/ozone/common/linux/drm_util_linux.h"
#include "ui/ozone/platform/wayland/wayland_connection.h"

namespace ui {

WaylandConnectionProxy::WaylandConnectionProxy(WaylandConnection* connection)
    : connection_(connection),
      ui_runner_(base::ThreadTaskRunnerHandle::Get()) {}

WaylandConnectionProxy::~WaylandConnectionProxy() = default;

void WaylandConnectionProxy::SetWaylandConnection(
    ozone::mojom::WaylandConnectionPtr wc_ptr) {
  // Store current thread's task runner to be able to make mojo calls on the
  // right sequence.
  ui_runner_ = base::ThreadTaskRunnerHandle::Get();
  wc_ptr.Bind(wc_ptr.PassInterface());
  wc_ptr_ = std::move(wc_ptr);
}

void WaylandConnectionProxy::CreateZwpLinuxDmabuf(
    base::File file,
    gfx::Size size,
    const std::vector<uint32_t>& strides,
    const std::vector<uint32_t>& offsets,
    const std::vector<uint64_t>& modifiers,
    uint32_t current_format,
    uint32_t planes_count,
    uint32_t buffer_id) {
  DCHECK(ui_runner_->BelongsToCurrentThread());
  DCHECK(wc_ptr_);
  wc_ptr_->CreateZwpLinuxDmabuf(std::move(file), size.width(), size.height(),
                                strides, offsets, current_format, modifiers,
                                planes_count, buffer_id);
}

void WaylandConnectionProxy::DestroyZwpLinuxDmabuf(uint32_t buffer_id) {
  // Mojo calls must be done on the right sequence.
  DCHECK(ui_runner_);
  ui_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&WaylandConnectionProxy::DestroyZwpLinuxDmabufInternal,
                     base::Unretained(this), buffer_id));
}

void WaylandConnectionProxy::DestroyZwpLinuxDmabufInternal(uint32_t buffer_id) {
  DCHECK(ui_runner_->BelongsToCurrentThread());
  DCHECK(wc_ptr_);
  wc_ptr_->DestroyZwpLinuxDmabuf(buffer_id);
}

void WaylandConnectionProxy::ScheduleBufferSwap(gfx::AcceleratedWidget widget,
                                                uint32_t buffer_id) {
  // Mojo calls must be done on the right sequence.
  DCHECK(ui_runner_);
  ui_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&WaylandConnectionProxy::ScheduleBufferSwapInternal,
                     base::Unretained(this), widget, buffer_id));
}

void WaylandConnectionProxy::ScheduleBufferSwapInternal(
    gfx::AcceleratedWidget widget,
    uint32_t buffer_id) {
  DCHECK(ui_runner_->BelongsToCurrentThread());
  DCHECK(wc_ptr_);
  wc_ptr_->ScheduleBufferSwap(widget, buffer_id);
}

WaylandWindow* WaylandConnectionProxy::GetWindow(
    gfx::AcceleratedWidget widget) {
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

wl_shm* WaylandConnectionProxy::shm() {
  wl_shm* shm = nullptr;
  if (connection_)
    shm = connection_->shm();
  return shm;
}

intptr_t WaylandConnectionProxy::Display() {
  if (connection_)
    return reinterpret_cast<intptr_t>(connection_->display());

#if defined(WAYLAND_GBM)
  // It must not be a single process mode. Thus, shared dmabuf approach is used,
  // which requires |gbm_device_|.
  DCHECK(gbm_device_);
  return EGL_DEFAULT_DISPLAY;
#endif
  return 0;
}

void WaylandConnectionProxy::AddBindingWaylandConnectionClient(
    ozone::mojom::WaylandConnectionClientRequest request) {
  bindings_.AddBinding(this, std::move(request));
}

}  // namespace ui
