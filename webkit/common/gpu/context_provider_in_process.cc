// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/common/gpu/context_provider_in_process.h"

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "cc/output/managed_memory_policy.h"
#include "webkit/common/gpu/grcontext_for_webgraphicscontext3d.h"
#include "webkit/common/gpu/managed_memory_policy_convert.h"

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

class ContextProviderInProcess::SwapBuffersCompleteCallbackProxy
    : public WebKit::WebGraphicsContext3D::
          WebGraphicsSwapBuffersCompleteCallbackCHROMIUM {
 public:
  explicit SwapBuffersCompleteCallbackProxy(ContextProviderInProcess* provider)
      : provider_(provider) {
    provider_->context3d_->setSwapBuffersCompleteCallbackCHROMIUM(this);
  }

  virtual ~SwapBuffersCompleteCallbackProxy() {
    provider_->context3d_->setSwapBuffersCompleteCallbackCHROMIUM(NULL);
  }

  virtual void onSwapBuffersComplete() {
    provider_->OnSwapBuffersComplete();
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
      WebKit::WebGraphicsMemoryAllocation allocation) {
    provider_->OnMemoryAllocationChanged(allocation);
  }

 private:
  ContextProviderInProcess* provider_;
};

// static
scoped_refptr<ContextProviderInProcess> ContextProviderInProcess::Create(
    scoped_ptr<WebGraphicsContext3DInProcessCommandBufferImpl> context3d) {
  if (!context3d)
    return NULL;
  return new ContextProviderInProcess(context3d.Pass());
}

// static
scoped_refptr<ContextProviderInProcess>
ContextProviderInProcess::CreateOffscreen() {
  WebKit::WebGraphicsContext3D::Attributes attributes;
  attributes.depth = false;
  attributes.stencil = true;
  attributes.antialias = false;
  attributes.shareResources = true;
  attributes.noAutomaticFlushes = true;

  return Create(
      WebGraphicsContext3DInProcessCommandBufferImpl::CreateOffscreenContext(
          attributes));
}

ContextProviderInProcess::ContextProviderInProcess(
    scoped_ptr<WebGraphicsContext3DInProcessCommandBufferImpl> context3d)
    : context3d_(context3d.Pass()),
      destroyed_(false) {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  DCHECK(context3d_);
  context_thread_checker_.DetachFromThread();
}

ContextProviderInProcess::~ContextProviderInProcess() {
  DCHECK(main_thread_checker_.CalledOnValidThread() ||
         context_thread_checker_.CalledOnValidThread());
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
  swap_buffers_complete_callback_proxy_.reset(
      new SwapBuffersCompleteCallbackProxy(this));
  memory_allocation_callback_proxy_.reset(
      new MemoryAllocationCallbackProxy(this));
  return true;
}

webkit::gpu::WebGraphicsContext3DInProcessCommandBufferImpl*
ContextProviderInProcess::Context3d() {
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

void ContextProviderInProcess::OnSwapBuffersComplete() {
  DCHECK(context_thread_checker_.CalledOnValidThread());
  if (!swap_buffers_complete_callback_.is_null())
    swap_buffers_complete_callback_.Run();
}

void ContextProviderInProcess::OnMemoryAllocationChanged(
    const WebKit::WebGraphicsMemoryAllocation& allocation) {
  DCHECK(context_thread_checker_.CalledOnValidThread());

  if (gr_context_) {
    bool nonzero_allocation = !!allocation.gpuResourceSizeInBytes;
    gr_context_->SetMemoryLimit(nonzero_allocation);
  }

  if (memory_policy_changed_callback_.is_null())
    return;

  bool discard_backbuffer_when_not_visible;
  cc::ManagedMemoryPolicy policy =
      ManagedMemoryPolicyConvert::Convert(allocation,
                                          &discard_backbuffer_when_not_visible);

  memory_policy_changed_callback_.Run(
      policy, discard_backbuffer_when_not_visible);
}

bool ContextProviderInProcess::DestroyedOnMainThread() {
  DCHECK(main_thread_checker_.CalledOnValidThread());

  base::AutoLock lock(destroyed_lock_);
  return destroyed_;
}

void ContextProviderInProcess::SetLostContextCallback(
    const LostContextCallback& lost_context_callback) {
  DCHECK(context_thread_checker_.CalledOnValidThread());
  DCHECK(lost_context_callback_.is_null() ||
         lost_context_callback.is_null());
  lost_context_callback_ = lost_context_callback;
}

void ContextProviderInProcess::SetSwapBuffersCompleteCallback(
    const SwapBuffersCompleteCallback& swap_buffers_complete_callback) {
  DCHECK(context_thread_checker_.CalledOnValidThread());
  DCHECK(swap_buffers_complete_callback_.is_null() ||
         swap_buffers_complete_callback.is_null());
  swap_buffers_complete_callback_ = swap_buffers_complete_callback;
}

void ContextProviderInProcess::SetMemoryPolicyChangedCallback(
    const MemoryPolicyChangedCallback& memory_policy_changed_callback) {
  DCHECK(context_thread_checker_.CalledOnValidThread());
  DCHECK(memory_policy_changed_callback_.is_null() ||
         memory_policy_changed_callback.is_null());
  memory_policy_changed_callback_ = memory_policy_changed_callback;
}

}  // namespace gpu
}  // namespace webkit
