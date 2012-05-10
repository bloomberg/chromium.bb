// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/plugins/webview_plugin.h"

#include "base/message_loop.h"
#include "base/metrics/histogram.h"
#include "skia/ext/platform_canvas.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebCursorInfo.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebElement.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebInputEvent.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebPluginContainer.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebSize.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebURL.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebURLRequest.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebURLResponse.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebView.h"
#include "webkit/glue/webpreferences.h"

using WebKit::WebCanvas;
using WebKit::WebCursorInfo;
using WebKit::WebDragData;
using WebKit::WebDragOperationsMask;
using WebKit::WebFrame;
using WebKit::WebImage;
using WebKit::WebInputEvent;
using WebKit::WebMouseEvent;
using WebKit::WebPlugin;
using WebKit::WebPluginContainer;
using WebKit::WebPoint;
using WebKit::WebRect;
using WebKit::WebSize;
using WebKit::WebString;
using WebKit::WebURLError;
using WebKit::WebURLRequest;
using WebKit::WebURLResponse;
using WebKit::WebVector;
using WebKit::WebView;

namespace webkit {

WebViewPlugin::WebViewPlugin(WebViewPlugin::Delegate* delegate)
    : delegate_(delegate),
      container_(NULL),
      finished_loading_(false) {
  web_view_ = WebView::create(this);
  web_view_->initializeMainFrame(this);
}

// static
WebViewPlugin* WebViewPlugin::Create(WebViewPlugin::Delegate* delegate,
                                     const WebPreferences& preferences,
                                     const std::string& html_data,
                                     const GURL& url) {
  WebViewPlugin* plugin = new WebViewPlugin(delegate);
  WebView* web_view = plugin->web_view();
  preferences.Apply(web_view);
  web_view->mainFrame()->loadHTMLString(html_data, url);
  return plugin;
}

WebViewPlugin::~WebViewPlugin() {
  web_view_->close();
}

void WebViewPlugin::ReplayReceivedData(WebPlugin* plugin) {
  if (!response_.isNull()) {
    plugin->didReceiveResponse(response_);
    size_t total_bytes = 0;
    for (std::list<std::string>::iterator it = data_.begin();
        it != data_.end(); ++it) {
      plugin->didReceiveData(it->c_str(), it->length());
      total_bytes += it->length();
    }
    UMA_HISTOGRAM_MEMORY_KB("PluginDocument.Memory", (total_bytes / 1024));
    UMA_HISTOGRAM_COUNTS("PluginDocument.NumChunks", data_.size());
  }
  if (finished_loading_) {
    plugin->didFinishLoading();
  }
  if (error_.get()) {
    plugin->didFailLoading(*error_);
  }
}

void WebViewPlugin::RestoreTitleText() {
  if (container_)
    container_->element().setAttribute("title", old_title_);
}


bool WebViewPlugin::initialize(WebPluginContainer* container) {
  container_ = container;
  if (container_)
    old_title_ = container_->element().getAttribute("title");
  return true;
}

void WebViewPlugin::destroy() {
  if (delegate_) {
    delegate_->WillDestroyPlugin();
    delegate_ = NULL;
  }
  container_ = NULL;
  MessageLoop::current()->DeleteSoon(FROM_HERE, this);
}

NPObject* WebViewPlugin::scriptableObject() {
  return NULL;
}

bool WebViewPlugin::getFormValue(WebString& value) {
  return false;
}

void WebViewPlugin::paint(WebCanvas* canvas, const WebRect& rect) {
  gfx::Rect paintRect(rect_.Intersect(rect));
  if (paintRect.IsEmpty())
    return;

  paintRect.Offset(-rect_.x(), -rect_.y());

  canvas->translate(SkIntToScalar(rect_.x()), SkIntToScalar(rect_.y()));
  canvas->save();

  web_view_->layout();
  web_view_->paint(canvas, paintRect);

  canvas->restore();
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

bool WebViewPlugin::acceptsInputEvents() {
  return true;
}

bool WebViewPlugin::handleInputEvent(const WebInputEvent& event,
                                     WebCursorInfo& cursor) {
  if (event.type == WebInputEvent::ContextMenu) {
    if (delegate_) {
      const WebMouseEvent& mouse_event =
          reinterpret_cast<const WebMouseEvent&>(event);
      delegate_->ShowContextMenu(mouse_event);
    }
    return true;
  }
  current_cursor_ = cursor;
  bool handled = web_view_->handleInputEvent(event);
  cursor = current_cursor_;
  return handled;
}

void WebViewPlugin::didReceiveResponse(const WebURLResponse& response) {
  DCHECK(response_.isNull());
  response_ = response;
}

void WebViewPlugin::didReceiveData(const char* data, int data_length) {
  data_.push_back(std::string(data, data_length));
}

void WebViewPlugin::didFinishLoading() {
  DCHECK(!finished_loading_);
  finished_loading_ = true;
}

void WebViewPlugin::didFailLoading(const WebURLError& error) {
  DCHECK(!error_.get());
  error_.reset(new WebURLError(error));
}

bool WebViewPlugin::acceptsLoadDrops() {
  return false;
}

void WebViewPlugin::setToolTipText(const WebKit::WebString& text,
                                   WebKit::WebTextDirection hint) {
  if (container_)
    container_->element().setAttribute("title", text);
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
    container_->invalidateRect(rect);
}

void WebViewPlugin::didChangeCursor(const WebCursorInfo& cursor) {
  current_cursor_ = cursor;
}

void WebViewPlugin::didClearWindowObject(WebFrame* frame) {
  if (delegate_)
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

void WebViewPlugin::didReceiveResponse(WebFrame* frame,
                                       unsigned identifier,
                                       const WebURLResponse& response) {
  WebFrameClient::didReceiveResponse(frame, identifier, response);
}

}  // namespace webkit
