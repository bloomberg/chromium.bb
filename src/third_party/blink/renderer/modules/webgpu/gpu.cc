// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgpu/gpu.h"

#include <utility>

#include "gpu/command_buffer/client/webgpu_interface.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_graphics_context_3d_provider.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_request_adapter_options.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_adapter.h"
#include "third_party/blink/renderer/platform/graphics/gpu/dawn_control_client_holder.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"

namespace blink {

namespace {

void CreateContextProvider(
    const KURL& url,
    base::WaitableEvent* waitable_event,
    std::unique_ptr<WebGraphicsContext3DProvider>* created_context_provider) {
  DCHECK(IsMainThread());
  *created_context_provider =
      Platform::Current()->CreateWebGPUGraphicsContext3DProvider(url);
  waitable_event->Signal();
}

std::unique_ptr<WebGraphicsContext3DProvider> CreateContextProviderOnMainThread(
    const KURL& url) {
  scoped_refptr<base::SingleThreadTaskRunner> task_runner =
      Thread::MainThread()->GetTaskRunner();

  base::WaitableEvent waitable_event;
  std::unique_ptr<WebGraphicsContext3DProvider> created_context_provider;
  PostCrossThreadTask(
      *task_runner, FROM_HERE,
      CrossThreadBindOnce(&CreateContextProvider, url,
                          CrossThreadUnretained(&waitable_event),
                          CrossThreadUnretained(&created_context_provider)));

  waitable_event.Wait();
  return created_context_provider;
}

}  // anonymous namespace

// static
GPU* GPU::Create(ExecutionContext& execution_context) {
  const KURL& url = execution_context.Url();

  std::unique_ptr<WebGraphicsContext3DProvider> context_provider;
  if (IsMainThread()) {
    context_provider =
        Platform::Current()->CreateWebGPUGraphicsContext3DProvider(url);
  } else {
    context_provider = CreateContextProviderOnMainThread(url);
  }

  // TODO(kainino): we will need a better way of accessing the GPU interface
  // from multiple threads than BindToCurrentThread et al.
  if (context_provider && !context_provider->BindToCurrentThread()) {
    // TODO(crbug.com/973017): Collect GPU info and surface context creation
    // error.
    return nullptr;
  }

  if (!context_provider) {
    // TODO(crbug.com/973017): Collect GPU info and surface context creation
    // error.
    return nullptr;
  }

  return MakeGarbageCollected<GPU>(execution_context,
                                   std::move(context_provider));
}

GPU::GPU(ExecutionContext& execution_context,
         std::unique_ptr<WebGraphicsContext3DProvider> context_provider)
    : ExecutionContextLifecycleObserver(&execution_context),
      dawn_control_client_(base::MakeRefCounted<DawnControlClientHolder>(
          std::move(context_provider))) {}

GPU::~GPU() = default;

void GPU::Trace(Visitor* visitor) {
  ScriptWrappable::Trace(visitor);
  ExecutionContextLifecycleObserver::Trace(visitor);
}

void GPU::ContextDestroyed() {
  dawn_control_client_->Destroy();
}

void GPU::OnRequestAdapterCallback(ScriptPromiseResolver* resolver,
                                   uint32_t adapter_server_id,
                                   const WGPUDeviceProperties& properties) {
  auto* adapter = MakeGarbageCollected<GPUAdapter>(
      "Default", adapter_server_id, properties, dawn_control_client_);
  resolver->Resolve(adapter);
}

ScriptPromise GPU::requestAdapter(ScriptState* script_state,
                                  const GPURequestAdapterOptions* options) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();

  // For now we choose kHighPerformance by default.
  gpu::webgpu::PowerPreference power_preference =
      gpu::webgpu::PowerPreference::kHighPerformance;
  if (options->powerPreference() == "low-power") {
    power_preference = gpu::webgpu::PowerPreference::kLowPower;
  }

  if (!dawn_control_client_->GetInterface()->RequestAdapterAsync(
          power_preference,
          WTF::Bind(&GPU::OnRequestAdapterCallback, WrapPersistent(this),
                    WrapPersistent(resolver)))) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kOperationError, "Fail to request GPUAdapter"));
  }

  return promise;
}

}  // namespace blink
