// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_PLUGINS_WEBVIEW_PLUGIN_H_
#define WEBKIT_GLUE_PLUGINS_WEBVIEW_PLUGIN_H_

#include <list>

#include "base/scoped_ptr.h"
#include "base/task.h"
#include "third_party/WebKit/WebKit/chromium/public/WebCursorInfo.h"
#include "third_party/WebKit/WebKit/chromium/public/WebFrameClient.h"
#include "third_party/WebKit/WebKit/chromium/public/WebPlugin.h"
#include "third_party/WebKit/WebKit/chromium/public/WebURLResponse.h"
#include "third_party/WebKit/WebKit/chromium/public/WebViewClient.h"

// This class implements the WebPlugin interface by forwarding drawing and
// handling input events to a WebView.
// It can be used as a placeholder for an actual plugin, using HTML for the UI.
// To show HTML data inside the WebViewPlugin,
// call web_view->mainFrame()->loadHTMLString() with the HTML data and a fake
// chrome:// URL as origin.

class WebViewPlugin: public WebKit::WebPlugin, public WebKit::WebViewClient,
                     public WebKit::WebFrameClient {
 public:
  class Delegate {
   public:
    // Bind |frame| to a Javascript object, enabling the delegate to receive
    // callback methods from Javascript inside the WebFrame.
    // This method is called from WebFrameClient::didClearWindowObject.
    virtual void BindWebFrame(WebKit::WebFrame* frame) = 0;

    // Called before the WebViewPlugin is destroyed. The delegate should delete
    // itself here.
    virtual void WillDestroyPlugin() = 0;
  };

  explicit WebViewPlugin(Delegate* delegate);

  WebKit::WebView* web_view() { return web_view_; }

  WebKit::WebPluginContainer* container() { return container_; }

  // When loading a plug-in document (i.e. a full page plug-in not embedded in
  // another page), we save all data that has been received, and replay it with
  // this method on the actual plug-in.
  void ReplayReceivedData(WebKit::WebPlugin* plugin);

  // WebPlugin methods:
  virtual bool initialize(WebKit::WebPluginContainer*);
  virtual void destroy();

  virtual NPObject* scriptableObject() { return NULL; }

  virtual void paint(WebKit::WebCanvas* canvas, const WebKit::WebRect& rect);

  // Coordinates are relative to the containing window.
  virtual void updateGeometry(
      const WebKit::WebRect& frame_rect, const WebKit::WebRect& clip_rect,
      const WebKit::WebVector<WebKit::WebRect>& cut_out_rects, bool is_visible);

  virtual void updateFocus(bool) { }
  virtual void updateVisibility(bool) { }

  virtual bool acceptsInputEvents() { return true; }
  virtual bool handleInputEvent(const WebKit::WebInputEvent& event,
                                WebKit::WebCursorInfo& cursor_info);

  virtual void didReceiveResponse(const WebKit::WebURLResponse& response);
  virtual void didReceiveData(const char* data, int data_length);
  virtual void didFinishLoading();
  virtual void didFailLoading(const WebKit::WebURLError& error);

  // Called in response to WebPluginContainer::loadFrameRequest
  virtual void didFinishLoadingFrameRequest(
      const WebKit::WebURL& url, void* notifyData) { }
  virtual void didFailLoadingFrameRequest(const WebKit::WebURL& url,
                                          void* notify_data,
                                          const WebKit::WebURLError& error) { }

  // WebViewClient methods:
  virtual bool acceptsLoadDrops() { return false; }

  virtual void startDragging(const WebKit::WebDragData& drag_data,
                             WebKit::WebDragOperationsMask mask,
                             const WebKit::WebImage& image,
                             const WebKit::WebPoint& point);

  // WebWidgetClient methods:
  virtual void didInvalidateRect(const WebKit::WebRect&);
  virtual void didChangeCursor(const WebKit::WebCursorInfo& cursor);

  // WebFrameClient methods:
  virtual void didClearWindowObject(WebKit::WebFrame* frame);

  virtual bool canHandleRequest(WebKit::WebFrame* frame,
                                const WebKit::WebURLRequest& request);

  virtual WebKit::WebURLError cancelledError(
      WebKit::WebFrame* frame, const WebKit::WebURLRequest& request);

 private:
  friend class DeleteTask<WebViewPlugin>;
  virtual ~WebViewPlugin();

  Delegate* delegate_;
  WebKit::WebCursorInfo current_cursor_;
  WebKit::WebPluginContainer* container_;
  WebKit::WebView* web_view_;
  gfx::Rect rect_;

  WebKit::WebURLResponse response_;
  std::list<std::string> data_;
  bool finished_loading_;
  scoped_ptr<WebKit::WebURLError> error_;
};

#endif  // WEBKIT_GLUE_PLUGINS_WEBVIEW_PLUGIN_H_
