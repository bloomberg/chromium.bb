// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/gpu/context_provider_in_process.h"

#include "webkit/gpu/grcontext_for_webgraphicscontext3d.h"

namespace webkit {
namespace gpu {

class ContextProviderInProcess::LostContextCallbackProxy
    : public WebKit::WebGraphicsContext3D::WebGraphicsContextLostCallback {
 public:
  explicit LostContextCallbackProxy(ContextProviderInProcess* provider)
      : provider_(provider) {
    provider_->context3d_->setContextLostCallback(this);
  }

  virtual void onContextLost() {
    provider_->OnLostContextInternal();
  }

 private:
  ContextProviderInProcess* provider_;
};

class ContextProviderInProcess::MemoryAllocationCallbackProxy
    : public WebKit::WebGraphicsContext3D::
          WebGraphicsMemoryAllocationChangedCallbackCHROMIUM {
 public:
  explicit MemoryAllocationCallbackProxy(ContextProviderInProcess* provider)
      : provider_(provider) {
    provider_->context3d_->setMemoryAllocationChangedCallbackCHROMIUM(this);
  }

  virtual void onMemoryAllocationChanged(
      WebKit::WebGraphicsMemoryAllocation alloc) {
    provider_->OnMemoryAllocationChanged(!!alloc.gpuResourceSizeInBytes);
  }

 private:
  ContextProviderInProcess* provider_;
};

ContextProviderInProcess::ContextProviderInProcess()
    : destroyed_(false) {
}

ContextProviderInProcess::~ContextProviderInProcess() {}

bool ContextProviderInProcess::InitializeOnMainThread() {
  DCHECK(!context3d_);

  WebKit::WebGraphicsContext3D::Attributes attributes;
  attributes.depth = false;
  attributes.stencil = true;
  attributes.antialias = false;
  attributes.shareResources = true;
  attributes.noAutomaticFlushes = true;

  context3d_.reset(
      new webkit::gpu::WebGraphicsContext3DInProcessCommandBufferImpl(
          attributes));

  return context3d_;
}

bool ContextProviderInProcess::BindToCurrentThread() {
  DCHECK(context3d_);

  if (lost_context_callback_proxy_)
    return true;

  if (!context3d_->makeContextCurrent())
    return false;

  lost_context_callback_proxy_.reset(new LostContextCallbackProxy(this));
  return true;
}

WebKit::WebGraphicsContext3D* ContextProviderInProcess::Context3d() {
  DCHECK(context3d_);
  DCHECK(lost_context_callback_proxy_);  // Is bound to thread.

  return context3d_.get();
}

class GrContext* ContextProviderInProcess::GrContext() {
  DCHECK(context3d_);
  DCHECK(lost_context_callback_proxy_);  // Is bound to thread.

  if (gr_context_)
    return gr_context_->get();

  gr_context_.reset(
      new webkit::gpu::GrContextForWebGraphicsContext3D(context3d_.get()));
  memory_allocation_callback_proxy_.reset(
      new MemoryAllocationCallbackProxy(this));
  return gr_context_->get();
}

void ContextProviderInProcess::VerifyContexts() {
  DCHECK(context3d_);
  DCHECK(lost_context_callback_proxy_);  // Is bound to thread.

  if (context3d_->isContextLost())
    OnLostContextInternal();
}

void ContextProviderInProcess::OnLostContextInternal() {
  {
    base::AutoLock lock(destroyed_lock_);
    if (destroyed_)
      return;
    destroyed_ = true;
  }
  OnLostContext();
}

bool ContextProviderInProcess::DestroyedOnMainThread() {
  base::AutoLock lock(destroyed_lock_);
  return destroyed_;
}

void ContextProviderInProcess::OnMemoryAllocationChanged(
    bool nonzero_allocation) {
  if (gr_context_)
    gr_context_->SetMemoryLimit(nonzero_allocation);
}

}  // namespace gpu
}  // namespace webkit
