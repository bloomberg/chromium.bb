// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgpu/gpu_canvas_context.h"

#include "third_party/blink/renderer/bindings/modules/v8/rendering_context.h"

namespace blink {

GPUCanvasContext::Factory::Factory() {}
GPUCanvasContext::Factory::~Factory() {}

CanvasRenderingContext* GPUCanvasContext::Factory::Create(
    CanvasRenderingContextHost* host,
    const CanvasContextCreationAttributesCore& attrs) {
  return MakeGarbageCollected<GPUCanvasContext>(host, attrs);
}

CanvasRenderingContext::ContextType GPUCanvasContext::Factory::GetContextType()
    const {
  return CanvasRenderingContext::kContextGPUPresent;
}

GPUCanvasContext::GPUCanvasContext(
    CanvasRenderingContextHost* host,
    const CanvasContextCreationAttributesCore& attrs)
    : CanvasRenderingContext(host, attrs) {}

GPUCanvasContext::~GPUCanvasContext() {}

CanvasRenderingContext::ContextType GPUCanvasContext::GetContextType() const {
  return CanvasRenderingContext::kContextGPUPresent;
}

void GPUCanvasContext::SetCanvasGetContextResult(RenderingContext& result) {
  result.SetGPUCanvasContext(this);
}

}  // namespace blink
