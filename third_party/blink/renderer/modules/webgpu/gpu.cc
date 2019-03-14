// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgpu/gpu.h"

#include <utility>

#include "gpu/command_buffer/client/webgpu_interface.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_graphics_context_3d_provider.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_adapter.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_adapter_descriptor.h"

namespace blink {

// static
GPU* GPU::Create(ExecutionContext& execution_context) {
  const auto& url = execution_context.Url();
  Platform::GraphicsInfo info;

  std::unique_ptr<WebGraphicsContext3DProvider> context_provider =
      Platform::Current()->CreateWebGPUGraphicsContext3DProvider(url, &info);

  // TODO(kainino): we will need a better way of accessing the GPU interface
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

  return MakeGarbageCollected<GPU>(execution_context,
                                   std::move(context_provider));
}

GPU::GPU(ExecutionContext& execution_context,
         std::unique_ptr<WebGraphicsContext3DProvider> context_provider)
    : ContextLifecycleObserver(&execution_context),
      context_provider_(std::move(context_provider)) {}

GPU::~GPU() = default;

void GPU::Trace(blink::Visitor* visitor) {
  ScriptWrappable::Trace(visitor);
  ContextLifecycleObserver::Trace(visitor);
}

void GPU::ContextDestroyed(ExecutionContext* execution_context) {
  context_provider_.reset();
}

GPUAdapter* GPU::getAdapter(const GPUAdapterDescriptor* descriptor) {
  return GPUAdapter::Create(descriptor->powerPreference());
}

}  // namespace blink
