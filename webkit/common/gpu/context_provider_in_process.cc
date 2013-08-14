// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/common/gpu/context_provider_in_process.h"

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "webkit/common/gpu/grcontext_for_webgraphicscontext3d.h"
#include "webkit/common/gpu/webgraphicscontext3d_in_process_command_buffer_impl.h"

namespace webkit {
namespace gpu {

class ContextProviderInProcess::LostContextCallbackProxy
    : public WebKit::WebGraphicsContext3D::WebGraphicsContextLostCallback {
 public:
  explicit LostContextCallbackProxy(ContextProviderInProcess* provider)
      : provider_(provider) {
    provider_->context3d_->setContextLostCallback(this);
  }

  virtual ~LostContextCallbackProxy() {
    provider_->context3d_->setContextLostCallback(NULL);
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

  virtual ~MemoryAllocationCallbackProxy() {
    provider_->context3d_->setMemoryAllocationChangedCallbackCHROMIUM(NULL);
  }

  virtual void onMemoryAllocationChanged(
      WebKit::WebGraphicsMemoryAllocation alloc) {
    provider_->OnMemoryAllocationChanged(!!alloc.gpuResourceSizeInBytes);
  }

 private:
  ContextProviderInProcess* provider_;
};

// static
scoped_refptr<ContextProviderInProcess> ContextProviderInProcess::Create(
    const CreateCallback& create_callback) {
  scoped_refptr<ContextProviderInProcess> provider =
      new ContextProviderInProcess;
  if (!provider->InitializeOnMainThread(create_callback))
    return NULL;
  return provider;
}

static scoped_ptr<WebGraphicsContext3DInProcessCommandBufferImpl>
CreateOffscreenContext() {
  WebKit::WebGraphicsContext3D::Attributes attributes;
  attributes.depth = false;
  attributes.stencil = true;
  attributes.antialias = false;
  attributes.shareResources = true;
  attributes.noAutomaticFlushes = true;

  return WebGraphicsContext3DInProcessCommandBufferImpl::CreateOffscreenContext(
      attributes).Pass();
}

// static
scoped_refptr<ContextProviderInProcess>
ContextProviderInProcess::CreateOffscreen() {
  return Create(base::Bind(&CreateOffscreenContext));
}

ContextProviderInProcess::ContextProviderInProcess()
    : destroyed_(false) {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  context_thread_checker_.DetachFromThread();
}

ContextProviderInProcess::~ContextProviderInProcess() {
  DCHECK(main_thread_checker_.CalledOnValidThread() ||
         context_thread_checker_.CalledOnValidThread());
}

bool ContextProviderInProcess::InitializeOnMainThread(
    const CreateCallback& create_callback) {
  DCHECK(!context3d_);
  DCHECK(main_thread_checker_.CalledOnValidThread());
  DCHECK(!create_callback.is_null());

  context3d_ = create_callback.Run();
  return context3d_;
}

bool ContextProviderInProcess::BindToCurrentThread() {
  DCHECK(context3d_);

  // This is called on the thread the context will be used.
  DCHECK(context_thread_checker_.CalledOnValidThread());

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
  DCHECK(context_thread_checker_.CalledOnValidThread());

  return context3d_.get();
}

class GrContext* ContextProviderInProcess::GrContext() {
  DCHECK(context3d_);
  DCHECK(lost_context_callback_proxy_);  // Is bound to thread.
  DCHECK(context_thread_checker_.CalledOnValidThread());

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
  DCHECK(context_thread_checker_.CalledOnValidThread());

  if (context3d_->isContextLost())
    OnLostContext();
}

void ContextProviderInProcess::OnLostContext() {
  DCHECK(context_thread_checker_.CalledOnValidThread());
  {
    base::AutoLock lock(destroyed_lock_);
    if (destroyed_)
      return;
    destroyed_ = true;
  }
  if (!lost_context_callback_.is_null())
    base::ResetAndReturn(&lost_context_callback_).Run();
}

bool ContextProviderInProcess::DestroyedOnMainThread() {
  DCHECK(main_thread_checker_.CalledOnValidThread());

  base::AutoLock lock(destroyed_lock_);
  return destroyed_;
}

void ContextProviderInProcess::SetLostContextCallback(
    const LostContextCallback& lost_context_callback) {
  DCHECK(context_thread_checker_.CalledOnValidThread());
  DCHECK(lost_context_callback_.is_null());
  lost_context_callback_ = lost_context_callback;
}

void ContextProviderInProcess::OnMemoryAllocationChanged(
    bool nonzero_allocation) {
  DCHECK(context_thread_checker_.CalledOnValidThread());
  if (gr_context_)
    gr_context_->SetMemoryLimit(nonzero_allocation);
}

}  // namespace gpu
}  // namespace webkit
