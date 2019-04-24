// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_capture/browser/content_capture_receiver.h"

#include <utility>

#include "components/content_capture/browser/content_capture_receiver_manager.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"

namespace content_capture {

ContentCaptureReceiver::ContentCaptureReceiver(content::RenderFrameHost* rfh)
    : bindings_(this), rfh_(rfh), id_(GetIdFrom(rfh)) {}

ContentCaptureReceiver::~ContentCaptureReceiver() = default;

int64_t ContentCaptureReceiver::GetIdFrom(content::RenderFrameHost* rfh) {
  return static_cast<int64_t>(rfh->GetProcess()->GetID()) << 32 |
         (rfh->GetRoutingID() & 0xFFFFFFFF);
}

void ContentCaptureReceiver::BindRequest(
    mojom::ContentCaptureReceiverAssociatedRequest request) {
  bindings_.Bind(std::move(request));
}

void ContentCaptureReceiver::DidCaptureContent(const ContentCaptureData& data,
                                               bool first_data) {
  auto* manager = ContentCaptureReceiverManager::FromWebContents(
      content::WebContents::FromRenderFrameHost(rfh_));

  if (first_data) {
    // The session id of this frame isn't changed for new document navigation,
    // so the previous session should be terminated.
    if (frame_content_capture_data_.id != 0)
      manager->DidRemoveSession(this);

    frame_content_capture_data_.id = id_;
    // Copies everything except id and children.
    frame_content_capture_data_.value = data.value;
    frame_content_capture_data_.bounds = data.bounds;
  }
  // We can't avoid copy the data here, because id need to be overriden.
  ContentCaptureData content(data);
  content.id = id_;
  manager->DidCaptureContent(this, content);
}

void ContentCaptureReceiver::DidRemoveContent(
    const std::vector<int64_t>& data) {
  auto* manager = ContentCaptureReceiverManager::FromWebContents(
      content::WebContents::FromRenderFrameHost(rfh_));
  manager->DidRemoveContent(this, data);
}

}  // namespace content_capture
