// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/test/gpu_test_utils.h"

#include "components/viz/test/test_context_provider.h"
#include "components/viz/test/test_gles2_interface.h"
#include "third_party/blink/renderer/platform/graphics/gpu/shared_gpu_context.h"
#include "third_party/blink/renderer/platform/graphics/test/fake_web_graphics_context_3d_provider.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

void InitializeSharedGpuContext(viz::TestContextProvider* context_provider,
                                cc::ImageDecodeCache* cache) {
  auto factory = [](viz::TestGLES2Interface* gl, GrContext* context,
                    cc::ImageDecodeCache* cache, bool* gpu_compositing_disabled)
      -> std::unique_ptr<WebGraphicsContext3DProvider> {
    *gpu_compositing_disabled = false;
    auto context_provider =
        std::make_unique<FakeWebGraphicsContext3DProvider>(gl, cache, context);
    context_provider->SetCapabilities(gl->test_capabilities());
    return context_provider;
  };
  context_provider->BindToCurrentThread();
  viz::TestGLES2Interface* gl = context_provider->TestContextGL();
  GrContext* context = context_provider->GrContext();
  SharedGpuContext::SetContextProviderFactoryForTesting(
      WTF::BindRepeating(factory, WTF::Unretained(gl), WTF::Unretained(context),
                         WTF::Unretained(cache)));
}

}  // namespace blink
