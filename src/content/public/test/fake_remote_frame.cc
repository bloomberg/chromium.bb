// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/fake_remote_frame.h"

#include "third_party/blink/public/mojom/timing/resource_timing.mojom.h"

namespace content {

FakeRemoteFrame::FakeRemoteFrame() = default;

FakeRemoteFrame::~FakeRemoteFrame() = default;

void FakeRemoteFrame::Init(blink::AssociatedInterfaceProvider* provider) {
  provider->OverrideBinderForTesting(
      blink::mojom::RemoteFrame::Name_,
      base::BindRepeating(&FakeRemoteFrame::BindFrameHostReceiver,
                          base::Unretained(this)));
}

void FakeRemoteFrame::WillEnterFullscreen(blink::mojom::FullscreenOptionsPtr) {}

void FakeRemoteFrame::EnforceInsecureNavigationsSet(
    const std::vector<uint32_t>& set) {}

void FakeRemoteFrame::SetFrameOwnerProperties(
    blink::mojom::FrameOwnerPropertiesPtr properties) {}

void FakeRemoteFrame::EnforceInsecureRequestPolicy(
    blink::mojom::InsecureRequestPolicy policy) {}

void FakeRemoteFrame::SetReplicatedOrigin(
    const url::Origin& origin,
    bool is_potentially_trustworthy_unique_origin) {}

void FakeRemoteFrame::SetReplicatedIsAdSubframe(bool is_ad_subframe) {}

void FakeRemoteFrame::SetReplicatedName(const std::string& name,
                                        const std::string& unique_name) {}

void FakeRemoteFrame::DispatchLoadEventForFrameOwner() {}

void FakeRemoteFrame::Collapse(bool collapsed) {}

void FakeRemoteFrame::Focus() {}

void FakeRemoteFrame::SetHadStickyUserActivationBeforeNavigation(bool value) {}

void FakeRemoteFrame::SetNeedsOcclusionTracking(bool needs_tracking) {}

void FakeRemoteFrame::BubbleLogicalScroll(
    blink::mojom::ScrollDirection direction,
    ui::ScrollGranularity granularity) {}

void FakeRemoteFrame::UpdateUserActivationState(
    blink::mojom::UserActivationUpdateType update_type,
    blink::mojom::UserActivationNotificationType notification_type) {}

void FakeRemoteFrame::SetEmbeddingToken(
    const base::UnguessableToken& embedding_token) {}

void FakeRemoteFrame::SetPageFocus(bool is_focused) {}

void FakeRemoteFrame::RenderFallbackContent() {}

void FakeRemoteFrame::RenderFallbackContentWithResourceTiming(
    blink::mojom::ResourceTimingInfoPtr,
    const std::string& server_timing_value) {}

void FakeRemoteFrame::AddResourceTimingFromChild(
    blink::mojom::ResourceTimingInfoPtr timing) {}

void FakeRemoteFrame::ScrollRectToVisible(
    const gfx::Rect& rect,
    blink::mojom::ScrollIntoViewParamsPtr params) {}

void FakeRemoteFrame::DidStartLoading() {}

void FakeRemoteFrame::DidStopLoading() {}

void FakeRemoteFrame::IntrinsicSizingInfoOfChildChanged(
    blink::mojom::IntrinsicSizingInfoPtr sizing_info) {}

void FakeRemoteFrame::UpdateOpener(
    const absl::optional<blink::FrameToken>& opener_frame_token) {}

void FakeRemoteFrame::FakeRemoteFrame::BindFrameHostReceiver(
    mojo::ScopedInterfaceEndpointHandle handle) {
  receiver_.Bind(mojo::PendingAssociatedReceiver<blink::mojom::RemoteFrame>(
      std::move(handle)));
}

void FakeRemoteFrame::DetachAndDispose() {}

void FakeRemoteFrame::EnableAutoResize(const gfx::Size& min_size,
                                       const gfx::Size& max_size) {}

void FakeRemoteFrame::DisableAutoResize() {}

void FakeRemoteFrame::DidUpdateVisualProperties(
    const cc::RenderFrameMetadata& metadata) {}

void FakeRemoteFrame::SetFrameSinkId(const viz::FrameSinkId& frame_sink_id) {}

void FakeRemoteFrame::ChildProcessGone() {}
}  // namespace content
