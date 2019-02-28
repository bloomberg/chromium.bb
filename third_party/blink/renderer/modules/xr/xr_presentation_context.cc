// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/xr/xr_presentation_context.h"

#include "third_party/blink/renderer/bindings/modules/v8/rendering_context.h"
#include "third_party/blink/renderer/core/origin_trials/origin_trials.h"
#include "third_party/blink/renderer/modules/xr/xr_session.h"

namespace blink {

XRPresentationContext::XRPresentationContext(
    CanvasRenderingContextHost* host,
    const CanvasContextCreationAttributesCore& attrs)
    : ImageBitmapRenderingContextBase(host, attrs) {}

XRPresentationContext::~XRPresentationContext() {}

void XRPresentationContext::SetCanvasGetContextResult(
    RenderingContext& result) {
  result.SetXRPresentationContext(this);
}

CanvasRenderingContext* XRPresentationContext::Factory::Create(
    CanvasRenderingContextHost* host,
    const CanvasContextCreationAttributesCore& attrs) {
  if (!origin_trials::WebXREnabled(host->GetTopExecutionContext()))
    return nullptr;
  return MakeGarbageCollected<XRPresentationContext>(host, attrs);
}

void XRPresentationContext::BindToSession(XRSession* session) {
  if (bound_session_) {
    bound_session_->DetachOutputContext(this);
  }

  bound_session_ = session;
}

void XRPresentationContext::Trace(blink::Visitor* visitor) {
  visitor->Trace(bound_session_);
  ImageBitmapRenderingContextBase::Trace(visitor);
}

}  // namespace blink
