// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ws/gpu_host/arc_gpu_client.h"

#include "services/viz/privileged/interfaces/gl/gpu_service.mojom.h"

namespace ws {
namespace gpu_host {

ArcGpuClient::ArcGpuClient(viz::mojom::GpuService* gpu_service)
    : gpu_service_(gpu_service) {}

ArcGpuClient::~ArcGpuClient() {}

void ArcGpuClient::CreateVideoDecodeAccelerator(
    arc::mojom::VideoDecodeAcceleratorRequest vda_request) {
  gpu_service_->CreateArcVideoDecodeAccelerator(std::move(vda_request));
}

void ArcGpuClient::CreateVideoEncodeAccelerator(
    arc::mojom::VideoEncodeAcceleratorRequest vea_request) {
  gpu_service_->CreateArcVideoEncodeAccelerator(std::move(vea_request));
}

void ArcGpuClient::CreateVideoProtectedBufferAllocator(
    arc::mojom::VideoProtectedBufferAllocatorRequest pba_request) {
  gpu_service_->CreateArcVideoProtectedBufferAllocator(std::move(pba_request));
}

void ArcGpuClient::CreateProtectedBufferManager(
    arc::mojom::ProtectedBufferManagerRequest pbm_request) {
  gpu_service_->CreateArcProtectedBufferManager(std::move(pbm_request));
}

}  // namespace gpu_host
}  // namespace ws
