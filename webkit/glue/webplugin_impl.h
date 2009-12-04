// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_WEBPLUGIN_IMPL_H_
#define WEBKIT_GLUE_WEBPLUGIN_IMPL_H_

#include <string>
#include <map>
#include <vector>

#include "app/gfx/native_widget_types.h"
#include "base/basictypes.h"
#include "base/linked_ptr.h"
#include "base/task.h"
#include "base/weak_ptr.h"
#include "googleurl/src/gurl.h"
#include "third_party/WebKit/WebKit/chromium/public/WebPlugin.h"
#include "third_party/WebKit/WebKit/chromium/public/WebString.h"
#include "third_party/WebKit/WebKit/chromium/public/WebURLLoaderClient.h"
#include "third_party/WebKit/WebKit/chromium/public/WebURLRequest.h"
#include "third_party/WebKit/WebKit/chromium/public/WebVector.h"
#include "webkit/glue/webplugin.h"

class WebViewDelegate;

namespace WebKit {
class WebDevToolsAgent;
class WebFrame;
class WebPluginContainer;
class WebURLResponse;
class WebURLLoader;
class WebURLRequest;
}

namespace webkit_glue {

class MultipartResponseDelegate;
class WebPluginDelegate;
class WebPluginPageDelegate;

// This is the WebKit side of the plugin implementation that forwards calls,
// after changing out of WebCore types, to a delegate.  The delegate may
// be in a different process.
class WebPluginImpl : public WebPlugin,
                      public WebKit::WebPlugin,
                      public WebKit::WebURLLoaderClient {
 public:
  WebPluginImpl(
      WebKit::WebFrame* frame,
      const WebKit::WebPluginParams& params,
      const base::WeakPtr<WebPluginPageDelegate>& page_delegate);
  ~WebPluginImpl();

  // Helper function for sorting post data.
  static bool SetPostData(WebKit::WebURLRequest* request,
                          const char* buf,
                          uint32 length);

  virtual WebPluginDelegate* delegate() { return delegate_; }

 private:
  // WebKit::WebPlugin methods:
  virtual bool initialize(
      WebKit::WebPluginContainer* container);
  virtual void destroy();
  virtual NPObject* scriptableObject();
  virtual void paint(
      WebKit::WebCanvas* canvas, const WebKit::WebRect& paint_rect);
  virtual void updateGeometry(
      const WebKit::WebRect& frame_rect, const WebKit::WebRect& clip_rect,
      const WebKit::WebVector<WebKit::WebRect>& cut_outs, bool is_visible);
  virtual void updateFocus(bool focused);
  virtual void updateVisibility(bool visible);
  virtual bool acceptsInputEvents();
  virtual bool handleInputEvent(
      const WebKit::WebInputEvent& event, WebKit::WebCursorInfo& cursor_info);
  virtual void didReceiveResponse(const WebKit::WebURLResponse& response);
  virtual void didReceiveData(const char* data, int data_length);
  virtual void didFinishLoading();
  virtual void didFailLoading(const WebKit::WebURLError& error);
  virtual void didFinishLoadingFrameRequest(
      const WebKit::WebURL& url, void* notify_data);
  virtual void didFailLoadingFrameRequest(
      const WebKit::WebURL& url, void* notify_data,
      const WebKit::WebURLError& error);

  // WebPlugin implementation:
  void SetWindow(gfx::PluginWindowHandle window);
  void WillDestroyWindow(gfx::PluginWindowHandle window);
#if defined(OS_WIN)
  void SetWindowlessPumpEvent(HANDLE pump_messages_event) { }
#endif

  // Given a (maybe partial) url, completes using the base url.
  GURL CompleteURL(const char* url);

  // Executes the script passed in. The notify_needed and notify_data arguments
  // are passed in by the plugin process. These indicate whether the plugin
  // expects a notification on script execution. We pass them back to the
  // plugin as is. This avoids having to track the notification arguments in
  // the plugin process.
  bool ExecuteScript(const std::string& url, const std::wstring& script,
                     bool notify_needed, intptr_t notify_data,
                     bool popups_allowed);

  enum RoutingStatus {
    ROUTED,
    NOT_ROUTED,
    INVALID_URL,
    GENERAL_FAILURE
  };

  // Determines the referrer value sent along with outgoing HTTP requests
  // issued by plugins.
  enum Referrer {
    PLUGIN_SRC,
    DOCUMENT_URL,
    NO_REFERRER
  };

  // Given a download request, check if we need to route the output to a frame.
  // Returns ROUTED if the load is done and routed to a frame, NOT_ROUTED or
  // corresponding error codes otherwise.
  RoutingStatus RouteToFrame(const char* method, bool is_javascript_url,
                             const char* target, unsigned int len,
                             const char* buf, bool is_file_data,
                             bool notify_needed, intptr_t notify_data,
                             const char* url, Referrer referrer_flag);

  // Cancels a pending request.
  void CancelResource(unsigned long id);

  // Returns the next avaiable resource id. Returns 0 if the operation fails.
  // It may fail if the page has already been closed.
  unsigned long GetNextResourceId();

  // Initiates HTTP GET/POST requests.
  // Returns true on success.
  bool InitiateHTTPRequest(unsigned long resource_id,
                           WebPluginResourceClient* client,
                           const char* method, const char* buf, int buf_len,
                           const GURL& url, const char* range_info,
                           Referrer referrer_flag);

  gfx::Rect GetWindowClipRect(const gfx::Rect& rect);

  NPObject* GetWindowScriptNPObject();
  NPObject* GetPluginElement();

  void SetCookie(const GURL& url,
                 const GURL& first_party_for_cookies,
                 const std::string& cookie);
  std::string GetCookies(const GURL& url,
                         const GURL& first_party_for_cookies);

  void ShowModalHTMLDialog(const GURL& url, int width, int height,
                           const std::string& json_arguments,
                           std::string* json_retval);
  void OnMissingPluginStatus(int status);
  void Invalidate();
  void InvalidateRect(const gfx::Rect& rect);

  // Sets the actual Widget for the plugin.
  void SetContainer(WebKit::WebPluginContainer* container);

  // Destroys the plugin instance.
  // The response_handle_to_ignore parameter if not NULL indicates the
  // resource handle to be left valid during plugin shutdown.
  void TearDownPluginInstance(WebKit::WebURLLoader* loader_to_ignore);

  // WebURLLoaderClient implementation.  We implement this interface in the
  // renderer process, and then use the simple WebPluginResourceClient interface
  // to relay the callbacks to the plugin.
  virtual void willSendRequest(WebKit::WebURLLoader* loader,
                               WebKit::WebURLRequest& request,
                               const WebKit::WebURLResponse&);
  virtual void didSendData(WebKit::WebURLLoader* loader,
                           unsigned long long bytes_sent,
                           unsigned long long total_bytes_to_be_sent);
  virtual void didReceiveResponse(WebKit::WebURLLoader* loader,
                                  const WebKit::WebURLResponse& response);
  virtual void didReceiveData(WebKit::WebURLLoader* loader, const char *buffer,
                              int length);
  virtual void didFinishLoading(WebKit::WebURLLoader* loader);
  virtual void didFail(WebKit::WebURLLoader* loader, const WebKit::WebURLError&);

  // Helper function to remove the stored information about a resource
  // request given its index in m_clients.
  void RemoveClient(size_t i);

  // Helper function to remove the stored information about a resource
  // request given a handle.
  void RemoveClient(WebKit::WebURLLoader* loader);

  void HandleURLRequest(const char *method,
                        bool is_javascript_url,
                        const char* target, unsigned int len,
                        const char* buf, bool is_file_data,
                        bool notify, const char* url,
                        intptr_t notify_data, bool popups_allowed);

  void CancelDocumentLoad();

  void InitiateHTTPRangeRequest(const char* url, const char* range_info,
                                intptr_t existing_stream, bool notify_needed,
                                intptr_t notify_data);

  void SetDeferResourceLoading(unsigned long resource_id, bool defer);

  // Ignore in-process plugins mode for this flag.
  bool IsOffTheRecord() { return false; }

  // Handles HTTP multipart responses, i.e. responses received with a HTTP
  // status code of 206.
  void HandleHttpMultipartResponse(const WebKit::WebURLResponse& response,
                                   WebPluginResourceClient* client);

  void HandleURLRequestInternal(const char *method, bool is_javascript_url,
                                const char* target, unsigned int len,
                                const char* buf, bool is_file_data,
                                bool notify, const char* url,
                                intptr_t notify_data, bool popups_allowed,
                                Referrer referrer_flag);

  // Tears down the existing plugin instance and creates a new plugin instance
  // to handle the response identified by the loader parameter.
  bool ReinitializePluginForResponse(WebKit::WebURLLoader* loader);

  // Delayed task for downloading the plugin source URL.
  void OnDownloadPluginSrcUrl();

  struct ClientInfo {
    unsigned long id;
    WebPluginResourceClient* client;
    WebKit::WebURLRequest request;
    bool pending_failure_notification;
    linked_ptr<WebKit::WebURLLoader> loader;
  };

  // Helper functions
  WebPluginResourceClient* GetClientFromLoader(WebKit::WebURLLoader* loader);
  ClientInfo* GetClientInfoFromLoader(WebKit::WebURLLoader* loader);

  // Helper function to set the referrer on the request passed in.
  void SetReferrer(WebKit::WebURLRequest* request, Referrer referrer_flag);

  // Returns DevToolsAgent for the frame or 0.
  WebKit::WebDevToolsAgent* GetDevToolsAgent();

  std::vector<ClientInfo> clients_;

  bool windowless_;
  gfx::PluginWindowHandle window_;
  base::WeakPtr<WebPluginPageDelegate> page_delegate_;
  WebKit::WebFrame* webframe_;

  WebPluginDelegate* delegate_;

  // This is just a weak reference.
  WebKit::WebPluginContainer* container_;

  typedef std::map<WebPluginResourceClient*,
                   webkit_glue::MultipartResponseDelegate*>
      MultiPartResponseHandlerMap;
  // Tracks HTTP multipart response handlers instantiated for
  // a WebPluginResourceClient instance.
  MultiPartResponseHandlerMap multi_part_response_map_;

  // The plugin source URL.
  GURL plugin_url_;

  // Indicates if the download would be initiated by the plugin or us.
  bool load_manually_;

  // Indicates if this is the first geometry update received by the plugin.
  bool first_geometry_update_;

  // Set to true if the next response error should be ignored.
  bool ignore_response_error_;

  // The current plugin geometry and clip rectangle.
  gfx::Rect window_rect_;
  gfx::Rect clip_rect_;

  // The mime type of the plugin.
  std::string mime_type_;

  // Holds the list of argument names and values passed to the plugin.  We keep
  // these so that we can re-initialize the plugin if we need to.
  std::vector<std::string> arg_names_;
  std::vector<std::string> arg_values_;

  ScopedRunnableMethodFactory<WebPluginImpl> method_factory_;

  DISALLOW_COPY_AND_ASSIGN(WebPluginImpl);
};

}  // namespace webkit_glue

#endif  // #ifndef WEBKIT_GLUE_WEBPLUGIN_IMPL_H_
