// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/gpu/context_provider_in_process.h"

#include "webkit/gpu/grcontext_for_webgraphicscontext3d.h"
#include "webkit/gpu/webgraphicscontext3d_in_process_command_buffer_impl.h"
#include "webkit/gpu/webgraphicscontext3d_in_process_impl.h"

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

ContextProviderInProcess::ContextProviderInProcess(InProcessType type)
    : type_(type),
      destroyed_(false) {
}

ContextProviderInProcess::~ContextProviderInProcess() {}

bool ContextProviderInProcess::InitializeOnMainThread() {
  DCHECK(!context3d_);
  context3d_ = CreateOffscreenContext3d().Pass();
  return !!context3d_;
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

static scoped_ptr<WebKit::WebGraphicsContext3D> CreateInProcessImpl(
    const WebKit::WebGraphicsContext3D::Attributes& attributes) {
  return make_scoped_ptr(
      webkit::gpu::WebGraphicsContext3DInProcessImpl::CreateForWebView(
          attributes, false)).PassAs<WebKit::WebGraphicsContext3D>();
}

static scoped_ptr<WebKit::WebGraphicsContext3D> CreateCommandBufferImpl(
    const WebKit::WebGraphicsContext3D::Attributes& attributes) {
  scoped_ptr<webkit::gpu::WebGraphicsContext3DInProcessCommandBufferImpl> ctx(
      new webkit::gpu::WebGraphicsContext3DInProcessCommandBufferImpl());
  if (!ctx->Initialize(attributes, NULL))
    return scoped_ptr<WebKit::WebGraphicsContext3D>();
  return ctx.PassAs<WebKit::WebGraphicsContext3D>();
}

scoped_ptr<WebKit::WebGraphicsContext3D>
ContextProviderInProcess::CreateOffscreenContext3d() {
  WebKit::WebGraphicsContext3D::Attributes attributes;
  attributes.depth = false;
  attributes.stencil = true;
  attributes.antialias = false;
  attributes.shareResources = true;
  attributes.noAutomaticFlushes = true;

  switch (type_) {
    case IN_PROCESS:
      return CreateInProcessImpl(attributes).Pass();
    case IN_PROCESS_COMMAND_BUFFER:
      return CreateCommandBufferImpl(attributes).Pass();
  }
  NOTREACHED();
  return CreateInProcessImpl(attributes).Pass();
}

}  // namespace gpu
}  // namespace webkit
