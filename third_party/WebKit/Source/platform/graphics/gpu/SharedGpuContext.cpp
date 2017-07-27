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
                                  thread_specific_instance, ());
  return thread_specific_instance;
}

SharedGpuContext::SharedGpuContext() {}

void SharedGpuContext::CreateContextProviderOnMainThread(
    WaitableEvent* waitable_event) {
  DCHECK(IsMainThread());
  Platform::ContextAttributes context_attributes;
  context_attributes.web_gl_version = 1;  // GLES2
  Platform::GraphicsInfo graphics_info;
  context_provider_ = WTF::WrapUnique(new WebGraphicsContext3DProviderWrapper(
      Platform::Current()->CreateOffscreenGraphicsContext3DProvider(
          context_attributes, WebURL(), nullptr, &graphics_info)));
  if (waitable_event)
    waitable_event->Signal();
}

WeakPtr<WebGraphicsContext3DProviderWrapper>
SharedGpuContext::ContextProviderWrapper() {
  SharedGpuContext* this_ptr = GetInstanceForCurrentThread();
  this_ptr->CreateContextProviderIfNeeded();
  if (!this_ptr->context_provider_)
    return nullptr;
  return this_ptr->context_provider_->CreateWeakPtr();
}

void SharedGpuContext::CreateContextProviderIfNeeded() {
  if (context_provider_ && context_provider_->ContextProvider()
                                   ->ContextGL()
                                   ->GetGraphicsResetStatusKHR() == GL_NO_ERROR)
    return;

  if (context_provider_factory_) {
    // This path should only be used in unit tests
    context_provider_ = WTF::WrapUnique(
        new WebGraphicsContext3DProviderWrapper(context_provider_factory_()));
  } else if (IsMainThread()) {
    context_provider_ = WTF::WrapUnique(new WebGraphicsContext3DProviderWrapper(
        blink::Platform::Current()
            ->CreateSharedOffscreenGraphicsContext3DProvider()));
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
    if (context_provider_ &&
        !context_provider_->ContextProvider()->BindToCurrentThread())
      context_provider_ = nullptr;
  }
}

void SharedGpuContext::SetContextProviderFactoryForTesting(
    ContextProviderFactory factory) {
  SharedGpuContext* this_ptr = GetInstanceForCurrentThread();
  this_ptr->context_provider_.reset();
  this_ptr->context_provider_factory_ = factory;
}

bool SharedGpuContext::IsValidWithoutRestoring() {
  SharedGpuContext* this_ptr = GetInstanceForCurrentThread();
  if (!this_ptr->context_provider_)
    return false;
  return this_ptr->context_provider_->ContextProvider()
             ->ContextGL()
             ->GetGraphicsResetStatusKHR() == GL_NO_ERROR;
}

bool SharedGpuContext::AllowSoftwareToAcceleratedCanvasUpgrade() {
  SharedGpuContext* this_ptr = GetInstanceForCurrentThread();
  this_ptr->CreateContextProviderIfNeeded();
  if (!this_ptr->context_provider_)
    return false;
  return this_ptr->context_provider_->ContextProvider()
      ->GetCapabilities()
      .software_to_accelerated_canvas_upgrade;
}

}  // blink
