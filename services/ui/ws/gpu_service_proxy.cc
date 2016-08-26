// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ui/ws/gpu_service_proxy.h"

#include "services/shell/public/cpp/connection.h"
#include "services/ui/common/gpu_type_converters.h"
#include "services/ui/gpu/gpu_service_internal.h"

namespace ui {

namespace {

const int32_t kLocalGpuChannelClientId = 1;

}  // namespace

GpuServiceProxy::GpuServiceProxy() : next_client_id_(kLocalGpuChannelClientId) {
  // TODO(sad): Once GPU process is split, this would look like:
  //   connector->ConnectToInterface("mojo:gpu", &gpu_service_);
  GpuServiceInternal::GetInstance()->Add(GetProxy(&gpu_service_));
  gpu_service_->Initialize(
      base::Bind(&GpuServiceProxy::OnInitialized, base::Unretained(this)));
}

GpuServiceProxy::~GpuServiceProxy() {}

void GpuServiceProxy::Add(mojom::GpuServiceRequest request) {
  bindings_.AddBinding(this, std::move(request));
}

void GpuServiceProxy::OnInitialized(const gpu::GPUInfo& gpu_info) {
  gpu_info_ = gpu_info;
}

void GpuServiceProxy::OnGpuChannelEstablished(
    const EstablishGpuChannelCallback& callback,
    int32_t client_id,
    mojo::ScopedMessagePipeHandle channel_handle) {
  callback.Run(client_id, std::move(channel_handle), gpu_info_);
}

void GpuServiceProxy::EstablishGpuChannel(
    const EstablishGpuChannelCallback& callback) {
  const int client_id = ++next_client_id_;
  // TODO(sad): crbug.com/617415 figure out how to generate a meaningful tracing
  // id.
  const uint64_t client_tracing_id = 0;
  gpu_service_->EstablishGpuChannel(
      client_id, client_tracing_id,
      base::Bind(&GpuServiceProxy::OnGpuChannelEstablished,
                 base::Unretained(this), callback, client_id));
}

void GpuServiceProxy::CreateGpuMemoryBuffer(
    mojom::GpuMemoryBufferIdPtr id,
    const gfx::Size& size,
    gfx::BufferFormat format,
    gfx::BufferUsage usage,
    uint64_t surface_id,
    const mojom::GpuService::CreateGpuMemoryBufferCallback& callback) {
  NOTIMPLEMENTED();
}

void GpuServiceProxy::DestroyGpuMemoryBuffer(mojom::GpuMemoryBufferIdPtr id,
                                             const gpu::SyncToken& sync_token) {
  NOTIMPLEMENTED();
}

}  // namespace ui
