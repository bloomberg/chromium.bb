// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_H_

#include <memory>

#include "third_party/blink/public/platform/web_graphics_context_3d_provider.h"
#include "third_party/blink/renderer/core/execution_context/context_lifecycle_observer.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"

namespace blink {

class GPUAdapter;
class GPUAdapterDescriptor;

class GPU final : public ScriptWrappable, public ContextLifecycleObserver {
  DEFINE_WRAPPERTYPEINFO();
  USING_GARBAGE_COLLECTED_MIXIN(GPU);

 public:
  static GPU* Create(ExecutionContext& execution_context);
  explicit GPU(ExecutionContext& execution_context,
               std::unique_ptr<WebGraphicsContext3DProvider> context_provider);
  ~GPU() override;

  // ScriptWrappable overrides
  void Trace(blink::Visitor* visitor) override;

  // ContextLifecycleObserver overrides
  void ContextDestroyed(ExecutionContext* execution_context) override;

  GPUAdapter* getAdapter(const GPUAdapterDescriptor*);

 private:
  std::unique_ptr<WebGraphicsContext3DProvider> context_provider_;

  DISALLOW_COPY_AND_ASSIGN(GPU);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_H_
