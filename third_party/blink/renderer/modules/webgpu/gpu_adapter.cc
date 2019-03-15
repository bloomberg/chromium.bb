// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgpu/gpu_adapter.h"

#include "third_party/blink/renderer/modules/webgpu/gpu_device.h"

namespace blink {

// static
GPUAdapter* GPUAdapter::Create(const String& name) {
  return MakeGarbageCollected<GPUAdapter>(name);
}

const String& GPUAdapter::name() const {
  return name_;
}

GPUDevice* GPUAdapter::createDevice(const GPUDeviceDescriptor* descriptor) {
  return GPUDevice::Create(this, descriptor);
}

GPUAdapter::GPUAdapter(const String& name) : name_(name) {}

}  // namespace blink
