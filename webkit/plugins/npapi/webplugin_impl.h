// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_PLUGINS_NPAPI_WEBPLUGIN_IMPL_H_
#define WEBKIT_PLUGINS_NPAPI_WEBPLUGIN_IMPL_H_

#include <string>
#include <map>
#include <vector>

#include "base/basictypes.h"
#include "base/file_path.h"
#include "base/memory/weak_ptr.h"
#include "googleurl/src/gurl.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebPlugin.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebRect.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebString.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebURLLoaderClient.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebURLRequest.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebVector.h"
#include "ui/gfx/native_widget_types.h"
#include "webkit/plugins/npapi/webplugin.h"
#include "webkit/plugins/webkit_plugins_export.h"

namespace WebKit {
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
class WEBKIT_PLUGINS_EXPORT WebPluginImpl :
    NON_EXPORTED_BASE(public WebPlugin),
    NON_EXPORTED_BASE(public WebKit::WebPlugin),
    NON_EXPORTED_BASE(public WebKit::WebURLLoaderClient) {
 public:
  WebPluginImpl(
      WebKit::WebFrame* frame,
      const WebKit::WebPluginParams& params,
      const FilePath& file_path,
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
  virtual bool getFormValue(WebKit::WebString* value);
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
  virtual void SetWindow(gfx::PluginWindowHandle window) OVERRIDE;
  virtual void SetAcceptsInputEvents(bool accepts) OVERRIDE;
  virtual void WillDestroyWindow(gfx::PluginWindowHandle window) OVERRIDE;
#if defined(OS_WIN)
  void SetWindowlessPumpEvent(HANDLE pump_messages_event) { }
  void ReparentPluginWindow(HWND window, HWND parent) { }
#endif
  virtual void CancelResource(unsigned long id) OVERRIDE;
  virtual void Invalidate() OVERRIDE;
  virtual void InvalidateRect(const gfx::Rect& rect) OVERRIDE;
  virtual NPObject* GetWindowScriptNPObject() OVERRIDE;
  virtual NPObject* GetPluginElement() OVERRIDE;
  virtual bool FindProxyForUrl(const GURL& url,
                               std::string* proxy_list) OVERRIDE;
  virtual void SetCookie(const GURL& url,
                         const GURL& first_party_for_cookies,
                         const std::string& cookie) OVERRIDE;
  virtual std::string GetCookies(const GURL& url,
                                 const GURL& first_party_for_cookies) OVERRIDE;
  virtual void URLRedirectResponse(bool allow, int resource_id) OVERRIDE;
#if defined(OS_MACOSX)
  virtual void AcceleratedPluginEnabledRendering() OVERRIDE;
  virtual void AcceleratedPluginAllocatedIOSurface(int32 width,
                                                   int32 height,
                                                   uint32 surface_id) OVERRIDE;
  virtual void AcceleratedPluginSwappedIOSurface() OVERRIDE;
#endif

  // Given a (maybe partial) url, completes using the base url.
  GURL CompleteURL(const char* url);

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
                           bool notify_redirects,
                           bool check_mixed_scripting);

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
                              int data_length, int encoded_data_length);
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
                                bool notify_redirects) OVERRIDE;

  virtual void CancelDocumentLoad() OVERRIDE;

  virtual void InitiateHTTPRangeRequest(const char* url,
                                        const char* range_info,
                                        int pending_request_id) OVERRIDE;

  virtual void SetDeferResourceLoading(unsigned long resource_id,
                                       bool defer) OVERRIDE;

  // Ignore in-process plugins mode for this flag.
  virtual bool IsOffTheRecord() OVERRIDE;

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
                                bool notify_redirects,
                                bool check_mixed_scripting);

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

  // Check for invalid chars like @, ;, \ before the first / (in path).
  bool IsValidUrl(const GURL& url, Referrer referrer_flag);

  std::vector<ClientInfo> clients_;

  bool windowless_;
  gfx::PluginWindowHandle window_;
#if defined(OS_MACOSX)
  bool next_io_surface_allocated_;
  int32 next_io_surface_width_;
  int32 next_io_surface_height_;
  uint32 next_io_surface_id_;
#endif
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

  base::WeakPtrFactory<WebPluginImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(WebPluginImpl);
};

}  // namespace npapi
}  // namespace webkit

#endif  // WEBKIT_PLUGINS_NPAPI_WEBPLUGIN_IMPL_H_
