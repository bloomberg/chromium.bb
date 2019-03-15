// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgpu/gpu_adapter.h"

#include "third_party/blink/renderer/modules/webgpu/dawn_control_client_holder.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_device.h"

namespace blink {

// static
GPUAdapter* GPUAdapter::Create(
    const String& name,
    scoped_refptr<DawnControlClientHolder> dawn_control_client) {
  return MakeGarbageCollected<GPUAdapter>(name, std::move(dawn_control_client));
}

GPUAdapter::GPUAdapter(
    const String& name,
    scoped_refptr<DawnControlClientHolder> dawn_control_client)
    : DawnObject(std::move(dawn_control_client)), name_(name) {}

const String& GPUAdapter::name() const {
  return name_;
}

GPUDevice* GPUAdapter::createDevice(const GPUDeviceDescriptor* descriptor) {
  return GPUDevice::Create(GetDawnControlClient(), this, descriptor);
}

}  // namespace blink
