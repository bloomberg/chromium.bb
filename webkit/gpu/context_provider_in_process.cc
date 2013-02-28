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
    provider_->OnLostContext();
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

  void onMemoryAllocationChanged(WebKit::WebGraphicsMemoryAllocation alloc) {
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
  if (destroyed_)
    return false;
  if (context3d_)
    return true;

  context3d_ = CreateOffscreenContext3d().Pass();
  destroyed_ = !context3d_;
  return !!context3d_;
}

bool ContextProviderInProcess::BindToCurrentThread() {
  if (lost_context_callback_proxy_)
    return true;

  bool result = context3d_->makeContextCurrent();
  lost_context_callback_proxy_.reset(new LostContextCallbackProxy(this));
  return result;
}

WebKit::WebGraphicsContext3D* ContextProviderInProcess::Context3d() {
  return context3d_.get();
}

class GrContext* ContextProviderInProcess::GrContext() {
  if (gr_context_)
    return gr_context_->get();

  gr_context_.reset(
      new webkit::gpu::GrContextForWebGraphicsContext3D(context3d_.get()));
  memory_allocation_callback_proxy_.reset(
      new MemoryAllocationCallbackProxy(this));
  return gr_context_->get();
}

void ContextProviderInProcess::VerifyContexts() {
  if (!destroyed_ && context3d_->isContextLost())
    OnLostContext();
}

void ContextProviderInProcess::OnLostContext() {
  base::AutoLock lock(destroyed_lock_);
  destroyed_ = true;
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
