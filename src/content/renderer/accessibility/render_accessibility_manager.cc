// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/accessibility/render_accessibility_manager.h"

#include <utility>

#include "base/bind.h"
#include "base/debug/alias.h"
#include "content/common/render_accessibility.mojom-forward.h"
#include "content/renderer/accessibility/render_accessibility_impl.h"
#include "content/renderer/render_frame_impl.h"

namespace content {

RenderAccessibilityManager::RenderAccessibilityManager(
    RenderFrameImpl* render_frame)
    : render_frame_(render_frame) {}

RenderAccessibilityManager::~RenderAccessibilityManager() = default;

void RenderAccessibilityManager::BindReceiver(
    mojo::PendingAssociatedReceiver<mojom::RenderAccessibility> receiver) {
  DCHECK(!receiver_.is_bound());
  receiver_.Bind(std::move(receiver));
  receiver_.set_disconnect_handler(base::BindOnce(
      [](RenderAccessibilityManager* impl) {
        impl->receiver_.reset();
        impl->SetMode(0);
      },
      base::Unretained(this)));
}

RenderAccessibilityImpl*
RenderAccessibilityManager::GetRenderAccessibilityImpl() {
  return render_accessibility_.get();
}

ui::AXMode RenderAccessibilityManager::GetAccessibilityMode() const {
  if (!render_accessibility_)
    return ui::AXMode();
  return render_accessibility_->GetAccessibilityMode();
}

void RenderAccessibilityManager::SetMode(uint32_t ax_mode) {
  ui::AXMode old_mode = GetAccessibilityMode();
  ui::AXMode new_mode(ax_mode);
  if (old_mode == new_mode)
    return;

  if (new_mode.has_mode(ui::AXMode::kWebContents) &&
      !old_mode.has_mode(ui::AXMode::kWebContents)) {
    render_accessibility_ = std::make_unique<RenderAccessibilityImpl>(
        this, render_frame_, new_mode);
  } else if (!new_mode.has_mode(ui::AXMode::kWebContents) &&
             old_mode.has_mode(ui::AXMode::kWebContents)) {
    render_accessibility_.reset();
  }

  // Notify the RenderFrame when the accessibility mode is changes to ensure it
  // notifies the relevant observers (subclasses of RenderFrameObserver). This
  // includes the RenderAccessibilityImpl instance owned by |this|, which will
  // make update Blink and emit the relevant events back to the browser process
  // according to change in the accessibility mode being made.
  render_frame_->NotifyAccessibilityModeChange(new_mode);
}

void RenderAccessibilityManager::FatalError() {
  NO_CODE_FOLDING();
  CHECK(false) << "Invalid accessibility tree.";
}

void RenderAccessibilityManager::HitTest(
    const gfx::Point& point,
    ax::mojom::Event event_to_fire,
    int request_id,
    mojom::RenderAccessibility::HitTestCallback callback) {
  DCHECK(render_accessibility_);
  render_accessibility_->HitTest(point, event_to_fire, request_id,
                                 std::move(callback));
}

void RenderAccessibilityManager::PerformAction(const ui::AXActionData& data) {
  DCHECK(render_accessibility_);
  render_accessibility_->PerformAction(data);
}

void RenderAccessibilityManager::Reset(int32_t reset_token) {
  DCHECK(render_accessibility_);
  render_accessibility_->Reset(reset_token);
}

void RenderAccessibilityManager::HandleAccessibilityEvents(
    mojom::AXUpdatesAndEventsPtr updates_and_events,
    int32_t reset_token,
    mojom::RenderAccessibilityHost::HandleAXEventsCallback callback) {
  GetOrCreateRemoteRenderAccessibilityHost()->HandleAXEvents(
      std::move(updates_and_events), reset_token, std::move(callback));
}

void RenderAccessibilityManager::HandleLocationChanges(
    std::vector<mojom::LocationChangesPtr> changes) {
  GetOrCreateRemoteRenderAccessibilityHost()->HandleAXLocationChanges(
      std::move(changes));
}

mojo::Remote<mojom::RenderAccessibilityHost>&
RenderAccessibilityManager::GetOrCreateRemoteRenderAccessibilityHost() {
  if (!render_accessibility_host_) {
    render_frame_->GetBrowserInterfaceBroker()->GetInterface(
        render_accessibility_host_.BindNewPipeAndPassReceiver());
  }
  return render_accessibility_host_;
}

void RenderAccessibilityManager::CloseConnection() {
  if (render_accessibility_host_) {
    render_accessibility_host_.reset();
    if (render_accessibility_) {
      render_accessibility_->ConnectionClosed();
    }
  }
}

}  // namespace content
