// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/gpu/dawn_control_client_holder.h"

#include "base/check.h"
#include "third_party/blink/renderer/platform/graphics/gpu/webgpu_resource_provider_cache.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

// static
scoped_refptr<DawnControlClientHolder> DawnControlClientHolder::Create(
    std::unique_ptr<WebGraphicsContext3DProvider> context_provider,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  auto dawn_control_client_holder =
      base::MakeRefCounted<DawnControlClientHolder>(std::move(context_provider),
                                                    std::move(task_runner));
  dawn_control_client_holder->context_provider_->ContextProvider()
      ->SetLostContextCallback(WTF::BindRepeating(
          &DawnControlClientHolder::Destroy, dawn_control_client_holder));
  return dawn_control_client_holder;
}

DawnControlClientHolder::DawnControlClientHolder(
    std::unique_ptr<WebGraphicsContext3DProvider> context_provider,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : context_provider_(std::make_unique<WebGraphicsContext3DProviderWrapper>(
          std::move(context_provider))),
      task_runner_(task_runner),
      api_channel_(context_provider_->ContextProvider()
                       ->WebGPUInterface()
                       ->GetAPIChannel()),
      procs_(api_channel_->GetProcs()),
      recyclable_resource_cache_(GetContextProviderWeakPtr(), task_runner) {}

DawnControlClientHolder::~DawnControlClientHolder() = default;

void DawnControlClientHolder::Destroy() {
  api_channel_->Disconnect();

  // Destroy the WebGPU context.
  // This ensures that GPU resources are eagerly reclaimed.
  // Because we have disconnected the wire client, any JavaScript which uses
  // WebGPU will do nothing.
  if (context_provider_) {
    // If the context provider is destroyed during a real lost context event, it
    // causes the CommandBufferProxy that the context provider owns, which is
    // what issued the lost context event in the first place, to be destroyed
    // before the event is done being handled. This causes a crash when an
    // outstanding AutoLock goes out of scope. To avoid this, we create a no-op
    // task to hold a reference to the context provider until this function is
    // done executing, and drop it after.
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce([](std::unique_ptr<WebGraphicsContext3DProviderWrapper>
                              context_provider) {},
                       std::move(context_provider_)));
  }
}

base::WeakPtr<WebGraphicsContext3DProviderWrapper>
DawnControlClientHolder::GetContextProviderWeakPtr() const {
  if (!context_provider_) {
    return nullptr;
  }
  return context_provider_->GetWeakPtr();
}

bool DawnControlClientHolder::IsContextLost() const {
  return !context_provider_;
}

std::unique_ptr<RecyclableCanvasResource>
DawnControlClientHolder::GetOrCreateCanvasResource(
    const IntSize& size,
    const CanvasResourceParams& params,
    bool is_origin_top_left) {
  return recyclable_resource_cache_.GetOrCreateCanvasResource(
      size, params, is_origin_top_left);
}

}  // namespace blink
