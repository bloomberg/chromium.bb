// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/glue/plugins/webview_plugin.h"

#include "base/message_loop.h"
#include "third_party/WebKit/WebKit/chromium/public/WebCursorInfo.h"
#include "third_party/WebKit/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/WebKit/chromium/public/WebPluginContainer.h"
#include "third_party/WebKit/WebKit/chromium/public/WebSettings.h"
#include "third_party/WebKit/WebKit/chromium/public/WebSize.h"
#include "third_party/WebKit/WebKit/chromium/public/WebURL.h"
#include "third_party/WebKit/WebKit/chromium/public/WebURLRequest.h"
#include "third_party/WebKit/WebKit/chromium/public/WebView.h"

#if WEBKIT_USING_CG
#include <CoreGraphics/CGContext.h>
#elif WEBKIT_USING_SKIA
#include "skia/ext/platform_canvas.h"
#endif

using WebKit::WebCanvas;
using WebKit::WebCursorInfo;
using WebKit::WebDragData;
using WebKit::WebDragOperationsMask;
using WebKit::WebFrame;
using WebKit::WebImage;
using WebKit::WebInputEvent;
using WebKit::WebPluginContainer;
using WebKit::WebPoint;
using WebKit::WebRect;
using WebKit::WebSize;
using WebKit::WebURLError;
using WebKit::WebURLRequest;
using WebKit::WebVector;
using WebKit::WebView;

WebViewPlugin::WebViewPlugin(WebViewPlugin::Delegate* delegate)
    : delegate_(delegate),
      container_(NULL) {
  web_view_ = WebView::create(this, NULL);
  web_view_->initializeMainFrame(this);
}

WebViewPlugin::~WebViewPlugin() {
  web_view_->close();
}

bool WebViewPlugin::initialize(WebPluginContainer* container) {
  container_ = container;
  return true;
}

void WebViewPlugin::destroy() {
  delegate_->WillDestroyPlugin();
  delegate_ = NULL;
  MessageLoop::current()->DeleteSoon(FROM_HERE, this);
}

void WebViewPlugin::paint(WebCanvas* canvas, const WebRect& rect) {
  gfx::Rect paintRect(rect_.Intersect(rect));
  if (paintRect.IsEmpty())
    return;

  paintRect.Offset(-rect_.x(), -rect_.y());

#if WEBKIT_USING_CG
  CGContextRef context = canvas;
  CGContextTranslateCTM(context, rect_.x(), rect_.y());
  CGContextSaveGState(context);
#elif WEBKIT_USING_SKIA
  skia::PlatformCanvas* platform_canvas = canvas;
  platform_canvas->translate(SkIntToScalar(rect_.x()),
                             SkIntToScalar(rect_.y()));
  platform_canvas->save();
#endif

  web_view_->layout();
  web_view_->paint(canvas, paintRect);

#if WEBKIT_USING_SKIA
  platform_canvas->restore();
#elif WEBKIT_USING_CG
  CGContextRestoreGState(context);
#endif
}

// Coordinates are relative to the containing window.
void WebViewPlugin::updateGeometry(
    const WebRect& frame_rect, const WebRect& clip_rect,
    const WebVector<WebRect>& cut_out_rects, bool is_visible) {
  if (frame_rect != rect_) {
    rect_ = frame_rect;
    web_view_->resize(WebSize(frame_rect.width, frame_rect.height));
  }
}

bool WebViewPlugin::handleInputEvent(const WebInputEvent& event,
                                     WebCursorInfo& cursor) {
  current_cursor_ = cursor;
  bool handled = web_view_->handleInputEvent(event);
  cursor = current_cursor_;
  return handled;
}

void WebViewPlugin::startDragging(const WebDragData&,
                                  WebDragOperationsMask,
                                  const WebImage&,
                                  const WebPoint&) {
  // Immediately stop dragging.
  web_view_->dragSourceSystemDragEnded();
}

void WebViewPlugin::didInvalidateRect(const WebRect& rect) {
  if (container_)
    container_->invalidateRect(WebRect(rect));
}

void WebViewPlugin::didChangeCursor(const WebCursorInfo& cursor) {
  current_cursor_ = cursor;
}

void WebViewPlugin::didClearWindowObject(WebFrame* frame) {
  delegate_->BindWebFrame(frame);
}

bool WebViewPlugin::canHandleRequest(WebFrame* frame,
                                     const WebURLRequest& request) {
  return GURL(request.url()).SchemeIs("chrome");
}

WebURLError WebViewPlugin::cancelledError(WebFrame* frame,
                                          const WebURLRequest& request) {
  // Return an error with a non-zero reason so isNull() on the corresponding
  // ResourceError is false.
  WebURLError error;
  error.domain = "WebViewPlugin";
  error.reason = -1;
  error.unreachableURL = request.url();
  return error;
}
