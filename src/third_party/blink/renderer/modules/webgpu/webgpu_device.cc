// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgpu/webgpu_device.h"

#include "gpu/command_buffer/client/webgpu_interface.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/modules/webgpu/webgpu_adapter.h"

namespace blink {

// static
WebGPUDevice* WebGPUDevice::Create(ExecutionContext* execution_context,
                                   WebGPUAdapter* adapter) {
  DCHECK(IsMainThread());

  const auto& url = execution_context->Url();
  Platform::GraphicsInfo info;

  std::unique_ptr<WebGraphicsContext3DProvider> context_provider =
      Platform::Current()->CreateWebGPUGraphicsContext3DProvider(url, &info);

  // TODO(kainino): we will need a better way of accessing the WebGPU interface
  // from multiple threads than BindToCurrentThread et al.
  if (context_provider && context_provider->BindToCurrentThread()) {
    info.error_message =
        String("bindToCurrentThread failed: " + String(info.error_message));
  }

  if (!context_provider) {
    // TODO(kainino): send the error message somewhere
    // (see ExtractWebGLContextCreationError).
    return nullptr;
  }

  return MakeGarbageCollected<WebGPUDevice>(adapter,
                                            std::move(context_provider));
}

WebGPUAdapter* WebGPUDevice::adapter() const {
  return adapter_;
}

void WebGPUDevice::dummy() const {
  Interface()->Dummy();
}

void WebGPUDevice::Trace(blink::Visitor* visitor) {
  visitor->Trace(adapter_);
  ScriptWrappable::Trace(visitor);
}

WebGPUDevice::WebGPUDevice(
    WebGPUAdapter* adapter,
    std::unique_ptr<WebGraphicsContext3DProvider> context_provider)
    : adapter_(adapter), context_provider_(std::move(context_provider)) {}

gpu::webgpu::WebGPUInterface* WebGPUDevice::Interface() const {
  DCHECK(context_provider_);
  return context_provider_->WebGPUInterface();
}

}  // namespace blink
