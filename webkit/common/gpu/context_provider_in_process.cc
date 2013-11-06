// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/common/gpu/context_provider_in_process.h"

#include <set>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "cc/output/managed_memory_policy.h"
#include "gpu/command_buffer/client/gles2_implementation.h"
#include "webkit/common/gpu/grcontext_for_webgraphicscontext3d.h"

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

// static
scoped_refptr<ContextProviderInProcess> ContextProviderInProcess::Create(
    scoped_ptr<WebGraphicsContext3DInProcessCommandBufferImpl> context3d,
    const std::string& debug_name) {
  if (!context3d)
    return NULL;
  return new ContextProviderInProcess(context3d.Pass(), debug_name);
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
          attributes), "Offscreen");
}

ContextProviderInProcess::ContextProviderInProcess(
    scoped_ptr<WebGraphicsContext3DInProcessCommandBufferImpl> context3d,
    const std::string& debug_name)
    : context3d_(context3d.Pass()),
      destroyed_(false),
      debug_name_(debug_name) {
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

  std::string unique_context_name =
      base::StringPrintf("%s-%p", debug_name_.c_str(), context3d_.get());
  context3d_->pushGroupMarkerEXT(unique_context_name.c_str());

  lost_context_callback_proxy_.reset(new LostContextCallbackProxy(this));
  swap_buffers_complete_callback_proxy_.reset(
      new SwapBuffersCompleteCallbackProxy(this));
  return true;
}

cc::ContextProvider::Capabilities
ContextProviderInProcess::ContextCapabilities() {
  // We always use a WebGraphicsContext3DInProcessCommandBufferImpl which
  // provides the following capabilities:
  Capabilities caps;
  caps.discard_backbuffer = true;
  caps.map_image = true;
  caps.map_sub = true;
  caps.set_visibility = true;
  caps.shallow_flush = true;
  caps.texture_format_bgra8888 = true;
  caps.texture_rectangle = true;

  WebKit::WebString extensions =
      context3d_->getString(0x1F03 /* GL_EXTENSIONS */);
  std::vector<std::string> extension_list;
  base::SplitString(extensions.utf8(), ' ', &extension_list);
  std::set<std::string> extension_set(extension_list.begin(),
                                      extension_list.end());

  caps.post_sub_buffer = extension_set.count("GL_CHROMIUM_post_sub_buffer") > 0;
  return caps;
}

WebKit::WebGraphicsContext3D* ContextProviderInProcess::Context3d() {
  DCHECK(context3d_);
  DCHECK(lost_context_callback_proxy_);  // Is bound to thread.
  DCHECK(context_thread_checker_.CalledOnValidThread());

  return context3d_.get();
}

::gpu::ContextSupport* ContextProviderInProcess::ContextSupport() {
  DCHECK(context3d_);
  DCHECK(lost_context_callback_proxy_);  // Is bound to thread.
  DCHECK(context_thread_checker_.CalledOnValidThread());

  return context3d_->GetContextSupport();
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

bool ContextProviderInProcess::IsContextLost() {
  DCHECK(context3d_);
  DCHECK(lost_context_callback_proxy_);  // Is bound to thread.
  DCHECK(context_thread_checker_.CalledOnValidThread());

  return context3d_->isContextLost();
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
  // There's no memory manager for the in-process implementation.
}

}  // namespace gpu
}  // namespace webkit
