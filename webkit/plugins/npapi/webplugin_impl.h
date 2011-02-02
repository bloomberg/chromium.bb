// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_PLUGINS_NPAPI_WEBPLUGIN_IMPL_H_
#define WEBKIT_PLUGINS_NPAPI_WEBPLUGIN_IMPL_H_

#include <string>
#include <map>
#include <vector>

#include "base/basictypes.h"
#include "base/file_path.h"
#include "base/task.h"
#include "base/weak_ptr.h"
#include "gfx/native_widget_types.h"
#include "googleurl/src/gurl.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebPlugin.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebRect.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebString.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebURLLoaderClient.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebURLRequest.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebVector.h"
#include "webkit/plugins/npapi/webplugin.h"

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
}  // namespace webkit_glue

namespace webkit {
namespace npapi {

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
      const FilePath& file_path,
      const std::string& mime_type,
      const base::WeakPtr<WebPluginPageDelegate>& page_delegate);
  virtual ~WebPluginImpl();

  // Helper function for sorting post data.
  static bool SetPostData(WebKit::WebURLRequest* request,
                          const char* buf,
                          uint32 length);

  virtual WebPluginDelegate* delegate();

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
  virtual unsigned getBackingTextureId();
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
  virtual bool supportsPaginatedPrint();
  virtual int printBegin(const WebKit::WebRect& printable_area,
                         int printer_dpi);
  virtual bool printPage(int page_number, WebKit::WebCanvas* canvas);
  virtual void printEnd();
  virtual bool hasSelection() const;
  virtual WebKit::WebString selectionAsText() const;
  virtual WebKit::WebString selectionAsMarkup() const;
  virtual void setZoomFactor(float scale, bool text_only);
  virtual bool startFind(const WebKit::WebString& search_text,
                         bool case_sensitive,
                         int identifier);
  virtual void selectFindResult(bool forward);
  virtual void stopFind();

  // WebPlugin implementation:
  virtual void SetWindow(gfx::PluginWindowHandle window);
  virtual void SetAcceptsInputEvents(bool accepts);
  virtual void WillDestroyWindow(gfx::PluginWindowHandle window);
#if defined(OS_WIN)
  void SetWindowlessPumpEvent(HANDLE pump_messages_event) { }
#endif
  virtual void CancelResource(unsigned long id);
  virtual void Invalidate();
  virtual void InvalidateRect(const gfx::Rect& rect);
  virtual NPObject* GetWindowScriptNPObject();
  virtual NPObject* GetPluginElement();
  virtual void SetCookie(const GURL& url,
                         const GURL& first_party_for_cookies,
                         const std::string& cookie);
  virtual std::string GetCookies(const GURL& url,
                                 const GURL& first_party_for_cookies);
  virtual void ShowModalHTMLDialog(const GURL& url, int width, int height,
                                   const std::string& json_arguments,
                                   std::string* json_retval);
  virtual void OnMissingPluginStatus(int status);

  virtual void URLRedirectResponse(bool allow, int resource_id);

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
  RoutingStatus RouteToFrame(const char* url,
                             bool is_javascript_url,
                             bool popups_allowed,
                             const char* method,
                             const char* target,
                             const char* buf,
                             unsigned int len,
                             int notify_id,
                             Referrer referrer_flag);

  // Returns the next avaiable resource id. Returns 0 if the operation fails.
  // It may fail if the page has already been closed.
  unsigned long GetNextResourceId();

  // Initiates HTTP GET/POST requests.
  // Returns true on success.
  bool InitiateHTTPRequest(unsigned long resource_id,
                           WebPluginResourceClient* client,
                           const GURL& url,
                           const char* method,
                           const char* buf,
                           int len,
                           const char* range_info,
                           Referrer referrer_flag,
                           bool notify_redirects);

  gfx::Rect GetWindowClipRect(const gfx::Rect& rect);

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
                               const WebKit::WebURLResponse& response);
  virtual void didSendData(WebKit::WebURLLoader* loader,
                           unsigned long long bytes_sent,
                           unsigned long long total_bytes_to_be_sent);
  virtual void didReceiveResponse(WebKit::WebURLLoader* loader,
                                  const WebKit::WebURLResponse& response);
  virtual void didReceiveData(WebKit::WebURLLoader* loader, const char *buffer,
                              int length);
  virtual void didFinishLoading(WebKit::WebURLLoader* loader,
                                double finishTime);
  virtual void didFail(WebKit::WebURLLoader* loader,
                       const WebKit::WebURLError& error);

  // Helper function to remove the stored information about a resource
  // request given its index in m_clients.
  void RemoveClient(size_t i);

  // Helper function to remove the stored information about a resource
  // request given a handle.
  void RemoveClient(WebKit::WebURLLoader* loader);

  virtual void HandleURLRequest(const char* url,
                                const char *method,
                                const char* target,
                                const char* buf,
                                unsigned int len,
                                int notify_id,
                                bool popups_allowed,
                                bool notify_redirects);

  virtual void CancelDocumentLoad();

  virtual void InitiateHTTPRangeRequest(
      const char* url, const char* range_info, int pending_request_id);

  virtual void SetDeferResourceLoading(unsigned long resource_id, bool defer);

  // Ignore in-process plugins mode for this flag.
  virtual bool IsOffTheRecord();

  // Handles HTTP multipart responses, i.e. responses received with a HTTP
  // status code of 206.
  void HandleHttpMultipartResponse(const WebKit::WebURLResponse& response,
                                   WebPluginResourceClient* client);

  void HandleURLRequestInternal(const char* url,
                                const char* method,
                                const char* target,
                                const char* buf,
                                unsigned int len,
                                int notify_id,
                                bool popups_allowed,
                                Referrer referrer_flag,
                                bool notify_redirects);

  // Tears down the existing plugin instance and creates a new plugin instance
  // to handle the response identified by the loader parameter.
  bool ReinitializePluginForResponse(WebKit::WebURLLoader* loader);

  // Delayed task for downloading the plugin source URL.
  void OnDownloadPluginSrcUrl();

  struct ClientInfo;

  // Helper functions
  WebPluginResourceClient* GetClientFromLoader(WebKit::WebURLLoader* loader);
  ClientInfo* GetClientInfoFromLoader(WebKit::WebURLLoader* loader);

  // Helper function to set the referrer on the request passed in.
  void SetReferrer(WebKit::WebURLRequest* request, Referrer referrer_flag);

  // Returns DevToolsAgent for the frame or 0.
  WebKit::WebDevToolsAgent* GetDevToolsAgent();

  // Check for invalid chars like @, ;, \ before the first / (in path).
  bool IsValidUrl(const GURL& url, Referrer referrer_flag);

  std::vector<ClientInfo> clients_;

  bool windowless_;
  gfx::PluginWindowHandle window_;
  bool accepts_input_events_;
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
  WebPluginGeometry geometry_;

  // The location of the plugin on disk.
  FilePath file_path_;

  // The mime type of the plugin.
  std::string mime_type_;

  // Holds the list of argument names and values passed to the plugin.  We keep
  // these so that we can re-initialize the plugin if we need to.
  std::vector<std::string> arg_names_;
  std::vector<std::string> arg_values_;

  ScopedRunnableMethodFactory<WebPluginImpl> method_factory_;

  DISALLOW_COPY_AND_ASSIGN(WebPluginImpl);
};

}  // namespace npapi
}  // namespace webkit

#endif  // WEBKIT_PLUGINS_NPAPI_WEBPLUGIN_IMPL_H_
