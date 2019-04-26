// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgpu/gpu_adapter.h"

#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
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
    : DawnObjectBase(std::move(dawn_control_client)), name_(name) {}

const String& GPUAdapter::name() const {
  return name_;
}

ScriptPromise GPUAdapter::requestDevice(ScriptState* script_state,
                                        const GPUDeviceDescriptor* descriptor) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();

  ExecutionContext* execution_context = ExecutionContext::From(script_state);
  GPUDevice* device = GPUDevice::Create(
      execution_context, GetDawnControlClient(), this, descriptor);

  resolver->Resolve(device);
  return promise;
}

}  // namespace blink
