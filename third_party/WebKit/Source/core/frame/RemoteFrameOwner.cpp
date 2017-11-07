// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#include "core/frame/RemoteFrameOwner.h"

#include "core/frame/LocalFrame.h"
#include "core/frame/WebLocalFrameImpl.h"
#include "public/web/WebFrameClient.h"

namespace blink {

RemoteFrameOwner::RemoteFrameOwner(
    SandboxFlags flags,
    const ParsedFeaturePolicy& container_policy,
    const WebFrameOwnerProperties& frame_owner_properties)
    : sandbox_flags_(flags),
      browsing_context_container_name_(
          static_cast<String>(frame_owner_properties.name)),
      scrolling_(
          static_cast<ScrollbarMode>(frame_owner_properties.scrolling_mode)),
      margin_width_(frame_owner_properties.margin_width),
      margin_height_(frame_owner_properties.margin_height),
      allow_fullscreen_(frame_owner_properties.allow_fullscreen),
      allow_payment_request_(frame_owner_properties.allow_payment_request),
      is_display_none_(frame_owner_properties.is_display_none),
      csp_(frame_owner_properties.required_csp),
      container_policy_(container_policy) {}

void RemoteFrameOwner::Trace(blink::Visitor* visitor) {
  visitor->Trace(frame_);
  FrameOwner::Trace(visitor);
}

void RemoteFrameOwner::SetScrollingMode(
    WebFrameOwnerProperties::ScrollingMode mode) {
  scrolling_ = static_cast<ScrollbarMode>(mode);
}

void RemoteFrameOwner::SetContentFrame(Frame& frame) {
  frame_ = &frame;
}

void RemoteFrameOwner::ClearContentFrame() {
  DCHECK_EQ(frame_->Owner(), this);
  frame_ = nullptr;
}

void RemoteFrameOwner::DispatchLoad() {
  WebLocalFrameImpl* web_frame =
      WebLocalFrameImpl::FromFrame(ToLocalFrame(*frame_));
  web_frame->Client()->DispatchLoad();
}

}  // namespace blink
