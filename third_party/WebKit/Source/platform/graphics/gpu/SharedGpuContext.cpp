// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/gpu/SharedGpuContext.h"

#include "gpu/command_buffer/client/gles2_interface.h"
#include "gpu/command_buffer/common/capabilities.h"
#include "platform/CrossThreadFunctional.h"
#include "platform/WaitableEvent.h"
#include "platform/WebTaskRunner.h"
#include "public/platform/Platform.h"
#include "public/platform/WebGraphicsContext3DProvider.h"

namespace blink {

SharedGpuContext* SharedGpuContext::GetInstanceForCurrentThread() {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(ThreadSpecific<SharedGpuContext>,
                                  thread_specific_instance,
                                  new ThreadSpecific<SharedGpuContext>);
  return thread_specific_instance;
}

SharedGpuContext::SharedGpuContext() : context_id_(kNoSharedContext) {
  CreateContextProviderIfNeeded();
}

void SharedGpuContext::CreateContextProviderOnMainThread(
    WaitableEvent* waitable_event) {
  DCHECK(IsMainThread());
  Platform::ContextAttributes context_attributes;
  context_attributes.web_gl_version = 1;  // GLES2
  Platform::GraphicsInfo graphics_info;
  context_provider_ = WTF::WrapUnique(
      Platform::Current()->CreateOffscreenGraphicsContext3DProvider(
          context_attributes, WebURL(), nullptr, &graphics_info));
  if (waitable_event)
    waitable_event->Signal();
}

void SharedGpuContext::CreateContextProviderIfNeeded() {
  if (context_provider_ &&
      context_provider_->ContextGL()->GetGraphicsResetStatusKHR() ==
          GL_NO_ERROR)
    return;

  std::unique_ptr<WebGraphicsContext3DProvider> old_context_provider =
      std::move(context_provider_);
  if (context_provider_factory_) {
    // This path should only be used in unit tests
    context_provider_ = context_provider_factory_();
  } else if (IsMainThread()) {
    context_provider_ =
        WTF::WrapUnique(blink::Platform::Current()
                            ->CreateSharedOffscreenGraphicsContext3DProvider());
  } else {
    // This synchronous round-trip to the main thread is the reason why
    // SharedGpuContext encasulates the context provider: so we only have to do
    // this once per thread.
    WaitableEvent waitable_event;
    RefPtr<WebTaskRunner> task_runner =
        Platform::Current()->MainThread()->GetWebTaskRunner();
    task_runner->PostTask(
        BLINK_FROM_HERE,
        CrossThreadBind(&SharedGpuContext::CreateContextProviderOnMainThread,
                        CrossThreadUnretained(this),
                        CrossThreadUnretained(&waitable_event)));
    waitable_event.Wait();
    if (context_provider_ && !context_provider_->BindToCurrentThread())
      context_provider_ = nullptr;
  }

  if (context_provider_) {
    context_id_++;
    // In the unlikely event of an overflow...
    if (context_id_ == kNoSharedContext)
      context_id_++;
  } else {
    context_provider_ = std::move(old_context_provider);
  }
}

void SharedGpuContext::SetContextProviderFactoryForTesting(
    ContextProviderFactory factory) {
  SharedGpuContext* this_ptr = GetInstanceForCurrentThread();
  this_ptr->context_provider_.reset();
  this_ptr->context_provider_factory_ = factory;
  this_ptr->CreateContextProviderIfNeeded();
}

unsigned SharedGpuContext::ContextId() {
  if (!IsValid())
    return kNoSharedContext;
  SharedGpuContext* this_ptr = GetInstanceForCurrentThread();
  return this_ptr->context_id_;
}

gpu::gles2::GLES2Interface* SharedGpuContext::Gl() {
  if (IsValid()) {
    SharedGpuContext* this_ptr = GetInstanceForCurrentThread();
    return this_ptr->context_provider_->ContextGL();
  }
  return nullptr;
}

GrContext* SharedGpuContext::Gr() {
  if (IsValid()) {
    SharedGpuContext* this_ptr = GetInstanceForCurrentThread();
    return this_ptr->context_provider_->GetGrContext();
  }
  return nullptr;
}

bool SharedGpuContext::IsValid() {
  SharedGpuContext* this_ptr = GetInstanceForCurrentThread();
  this_ptr->CreateContextProviderIfNeeded();
  if (!this_ptr->context_provider_)
    return false;
  return this_ptr->context_provider_->ContextGL()
             ->GetGraphicsResetStatusKHR() == GL_NO_ERROR;
}

bool SharedGpuContext::IsValidWithoutRestoring() {
  SharedGpuContext* this_ptr = GetInstanceForCurrentThread();
  if (!this_ptr->context_provider_)
    return false;
  return this_ptr->context_provider_->ContextGL()
             ->GetGraphicsResetStatusKHR() == GL_NO_ERROR;
}

bool SharedGpuContext::AllowSoftwareToAcceleratedCanvasUpgrade() {
  if (!IsValid())
    return kNoSharedContext;
  SharedGpuContext* this_ptr = GetInstanceForCurrentThread();
  return this_ptr->context_provider_->GetCapabilities()
      .software_to_accelerated_canvas_upgrade;
}

}  // blink
