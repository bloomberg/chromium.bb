// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "ChromiumDataObject.h"
#include "ClipboardChromium.h"
#include "Frame.h"
#undef LOG

#include "webkit/api/public/WebDragData.h"
#include "webkit/api/public/WebViewClient.h"
#include "webkit/glue/dragclient_impl.h"
#include "webkit/glue/glue_util.h"
#include "webkit/glue/webview_impl.h"

using WebKit::WebDragData;
using WebKit::WebPoint;

void DragClientImpl::willPerformDragDestinationAction(
    WebCore::DragDestinationAction,
    WebCore::DragData*) {
  // FIXME
}

void DragClientImpl::willPerformDragSourceAction(
    WebCore::DragSourceAction,
    const WebCore::IntPoint&,
    WebCore::Clipboard*) {
  // FIXME
}

WebCore::DragDestinationAction DragClientImpl::actionMaskForDrag(
    WebCore::DragData*) {
  if (webview_->client() && webview_->client()->acceptsLoadDrops()) {
    return WebCore::DragDestinationActionAny;
  } else {
    return static_cast<WebCore::DragDestinationAction>
        (WebCore::DragDestinationActionDHTML |
         WebCore::DragDestinationActionEdit);
  }
}

WebCore::DragSourceAction DragClientImpl::dragSourceActionMaskForPoint(
    const WebCore::IntPoint& window_point) {
  // We want to handle drag operations for all source types.
  return WebCore::DragSourceActionAny;
}

void DragClientImpl::startDrag(WebCore::DragImageRef drag_image,
                               const WebCore::IntPoint& drag_image_origin,
                               const WebCore::IntPoint& event_pos,
                               WebCore::Clipboard* clipboard,
                               WebCore::Frame* frame,
                               bool is_link_drag) {
  // Add a ref to the frame just in case a load occurs mid-drag.
  RefPtr<WebCore::Frame> frame_protector = frame;

  WebDragData drag_data = webkit_glue::ChromiumDataObjectToWebDragData(
      static_cast<WebCore::ClipboardChromium*>(clipboard)->dataObject());

  WebCore::DragOperation drag_operation_mask;
  if (!clipboard->sourceOperation(drag_operation_mask))
    drag_operation_mask = WebCore::DragOperationEvery;

  webview_->StartDragging(
      webkit_glue::IntPointToWebPoint(event_pos),
      drag_data,
      static_cast<WebKit::WebDragOperationsMask>(drag_operation_mask));
}

WebCore::DragImageRef DragClientImpl::createDragImageForLink(
    WebCore::KURL&,
    const WebCore::String& label,
    WebCore::Frame*) {
  // FIXME
  return 0;
}

void DragClientImpl::dragControllerDestroyed() {
  // Our lifetime is bound to the WebViewImpl.
}
