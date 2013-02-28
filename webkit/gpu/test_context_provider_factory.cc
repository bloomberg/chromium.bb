// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/gpu/test_context_provider_factory.h"

#include "base/logging.h"
#include "cc/context_provider.h"
#include "webkit/gpu/context_provider_in_process.h"

namespace webkit {
namespace gpu {

#ifndef NDEBUG
static bool factory_set_up = false;
#endif
static ContextProviderInProcess::InProcessType context_provider_type;
static TestContextProviderFactory* context_provider_instance = NULL;

// static
void TestContextProviderFactory::SetUpFactoryForTesting(
    webkit_support::GraphicsContext3DImplementation implementation) {
#ifndef NDEBUG
  factory_set_up = true;
#endif

  switch (implementation) {
    case webkit_support::IN_PROCESS:
      context_provider_type = webkit::gpu::ContextProviderInProcess::IN_PROCESS;
      return;
    case webkit_support::IN_PROCESS_COMMAND_BUFFER:
      context_provider_type =
          webkit::gpu::ContextProviderInProcess::IN_PROCESS_COMMAND_BUFFER;
      return;
  }
  NOTREACHED();
  context_provider_type = webkit::gpu::ContextProviderInProcess::IN_PROCESS;
}

// static
TestContextProviderFactory* TestContextProviderFactory::GetInstance() {
#ifndef NDEBUG
  DCHECK(factory_set_up);
#endif
  if (!context_provider_instance)
    context_provider_instance = new TestContextProviderFactory();
  return context_provider_instance;
}

TestContextProviderFactory::TestContextProviderFactory() {}

TestContextProviderFactory::~TestContextProviderFactory() {}

scoped_refptr<cc::ContextProvider> TestContextProviderFactory::
    OffscreenContextProviderForMainThread() {
  if (!main_thread_ || main_thread_->DestroyedOnMainThread()) {
    main_thread_ = new ContextProviderInProcess(context_provider_type);
  }
  return main_thread_;
}

scoped_refptr<cc::ContextProvider> TestContextProviderFactory::
    OffscreenContextProviderForCompositorThread() {
  if (!compositor_thread_ ||
      compositor_thread_->DestroyedOnMainThread()) {
    compositor_thread_ = new ContextProviderInProcess(context_provider_type);
  }
  return compositor_thread_;
}

}  // namespace gpu
}  // namespace webkit
