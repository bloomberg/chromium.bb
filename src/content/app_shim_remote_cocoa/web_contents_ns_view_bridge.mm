// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/app_shim_remote_cocoa/web_contents_ns_view_bridge.h"

#include "components/remote_cocoa/app_shim/ns_view_ids.h"
#import "content/app_shim_remote_cocoa/web_contents_view_cocoa.h"
#include "content/browser/web_contents/web_contents_view_mac.h"
#include "ui/gfx/image/image_skia_util_mac.h"

namespace content {

WebContentsNSViewBridge::WebContentsNSViewBridge(
    uint64_t view_id,
    mojom::WebContentsNSViewClientAssociatedPtr client)
    : client_(std::move(client)) {
  cocoa_view_.reset(
      [[WebContentsViewCocoa alloc] initWithViewsHostableView:nullptr]);
  [cocoa_view_ setClient:client_.get()];
  view_id_ = std::make_unique<remote_cocoa::ScopedNSViewIdMapping>(
      view_id, cocoa_view_.get());
}

WebContentsNSViewBridge::WebContentsNSViewBridge(
    uint64_t view_id,
    WebContentsViewMac* web_contents_view) {
  cocoa_view_.reset([[WebContentsViewCocoa alloc]
      initWithViewsHostableView:web_contents_view]);
  [cocoa_view_ setClient:web_contents_view];
  view_id_ = std::make_unique<remote_cocoa::ScopedNSViewIdMapping>(
      view_id, cocoa_view_.get());
}

WebContentsNSViewBridge::~WebContentsNSViewBridge() {
  // This handles the case where a renderer close call was deferred
  // while the user was operating a UI control which resulted in a
  // close.  In that case, the Cocoa view outlives the
  // WebContentsViewMac instance due to Cocoa retain count.
  [cocoa_view_ setClient:nullptr];
  [cocoa_view_ clearViewsHostableView];
  [cocoa_view_ removeFromSuperview];
}

void WebContentsNSViewBridge::SetParentNSView(uint64_t parent_ns_view_id) {
  NSView* parent_ns_view = remote_cocoa::GetNSViewFromId(parent_ns_view_id);
  // If the browser passed an invalid handle, then there is no recovery.
  CHECK(parent_ns_view);
  [parent_ns_view addSubview:cocoa_view_];
}

void WebContentsNSViewBridge::ResetParentNSView() {
  [cocoa_view_ removeFromSuperview];
}

void WebContentsNSViewBridge::SetBounds(const gfx::Rect& bounds_in_window) {
  NSWindow* window = [cocoa_view_ window];
  NSRect window_content_rect = [window contentRectForFrameRect:[window frame]];
  NSRect ns_bounds_in_window =
      NSMakeRect(bounds_in_window.x(),
                 window_content_rect.size.height - bounds_in_window.y() -
                     bounds_in_window.height(),
                 bounds_in_window.width(), bounds_in_window.height());
  NSRect ns_bounds_in_superview =
      [[cocoa_view_ superview] convertRect:ns_bounds_in_window fromView:nil];
  [cocoa_view_ setFrame:ns_bounds_in_superview];
}

void WebContentsNSViewBridge::SetVisible(bool visible) {
  [cocoa_view_ setHidden:!visible];
}

void WebContentsNSViewBridge::MakeFirstResponder() {
  if ([cocoa_view_ acceptsFirstResponder])
    [[cocoa_view_ window] makeFirstResponder:cocoa_view_];
}

void WebContentsNSViewBridge::TakeFocus(bool reverse) {
  if (reverse)
    [[cocoa_view_ window] selectPreviousKeyView:cocoa_view_];
  else
    [[cocoa_view_ window] selectNextKeyView:cocoa_view_];
}

void WebContentsNSViewBridge::StartDrag(const DropData& drop_data,
                                        uint32_t operation_mask,
                                        const gfx::ImageSkia& image,
                                        const gfx::Vector2d& image_offset) {
  NSPoint offset = NSPointFromCGPoint(
      gfx::PointAtOffsetFromOrigin(image_offset).ToCGPoint());
  [cocoa_view_ startDragWithDropData:drop_data
                   dragOperationMask:operation_mask
                               image:gfx::NSImageFromImageSkia(image)
                              offset:offset];
}

}  // namespace content
