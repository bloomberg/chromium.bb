// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "Document.h"
#include "DocumentLoader.h"
#include "FormState.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "FrameLoadRequest.h"
#include "HTMLFormElement.h"
#include "KURL.h"
#include "PlatformString.h"
#include "ResourceResponse.h"
#include "ScriptController.h"
#include "ScriptValue.h"
#include "Widget.h"

#undef LOG
#include "base/gfx/rect.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/string_util.h"
#include "net/base/escape.h"
#include "webkit/api/public/WebCursorInfo.h"
#include "webkit/api/public/WebData.h"
#include "webkit/api/public/WebHTTPBody.h"
#include "webkit/api/public/WebHTTPHeaderVisitor.h"
#include "webkit/api/public/WebInputEvent.h"
#include "webkit/api/public/WebKit.h"
#include "webkit/api/public/WebKitClient.h"
#include "webkit/api/public/WebRect.h"
#include "webkit/api/public/WebString.h"
#include "webkit/api/public/WebURL.h"
#include "webkit/api/public/WebURLLoader.h"
#include "webkit/api/public/WebURLLoaderClient.h"
#include "webkit/api/public/WebURLResponse.h"
#include "webkit/api/public/WebVector.h"
#include "webkit/api/src/WebPluginContainerImpl.h"
#include "webkit/glue/chrome_client_impl.h"
#include "webkit/glue/glue_util.h"
#include "webkit/glue/multipart_response_delegate.h"
#include "webkit/glue/webkit_glue.h"
#include "webkit/glue/webplugin_impl.h"
#include "webkit/glue/plugins/plugin_host.h"
#include "webkit/glue/plugins/plugin_instance.h"
#include "webkit/glue/webplugin_delegate.h"
#include "webkit/glue/webview_impl.h"
#include "googleurl/src/gurl.h"

using WebKit::WebCanvas;
using WebKit::WebCursorInfo;
using WebKit::WebData;
using WebKit::WebHTTPBody;
using WebKit::WebHTTPHeaderVisitor;
using WebKit::WebInputEvent;
using WebKit::WebKeyboardEvent;
using WebKit::WebMouseEvent;
using WebKit::WebPluginContainer;
using WebKit::WebPluginContainerImpl;
using WebKit::WebRect;
using WebKit::WebString;
using WebKit::WebURLError;
using WebKit::WebURLLoader;
using WebKit::WebURLLoaderClient;
using WebKit::WebURLRequest;
using WebKit::WebURLResponse;
using WebKit::WebVector;
using webkit_glue::MultipartResponseDelegate;

namespace {

// This class handles individual multipart responses. It is instantiated when
// we receive HTTP status code 206 in the HTTP response. This indicates
// that the response could have multiple parts each separated by a boundary
// specified in the response header.
class MultiPartResponseClient : public WebURLLoaderClient {
 public:
  MultiPartResponseClient(WebPluginResourceClient* resource_client)
      : resource_client_(resource_client) {
    Clear();
  }

  virtual void willSendRequest(
      WebURLLoader*, WebURLRequest&, const WebURLResponse&) {}
  virtual void didSendData(
      WebURLLoader*, unsigned long long, unsigned long long) {}

  // Called when the multipart parser encounters an embedded multipart
  // response.
  virtual void didReceiveResponse(
      WebURLLoader*, const WebURLResponse& response) {
    if (!MultipartResponseDelegate::ReadContentRanges(
            response,
            &byte_range_lower_bound_,
            &byte_range_upper_bound_)) {
      NOTREACHED();
      return;
    }

    resource_response_ = response;
  }

  // Receives individual part data from a multipart response.
  virtual void didReceiveData(
      WebURLLoader*, const char* data, int data_size, long long) {
    // TODO(ananta)
    // We should defer further loads on multipart resources on the same lines
    // as regular resources requested by plugins to prevent reentrancy.
    resource_client_->DidReceiveData(
        data, data_size, byte_range_lower_bound_);
  }

  virtual void didFinishLoading(WebURLLoader*) {}
  virtual void didFail(WebURLLoader*, const WebURLError&) {}

  void Clear() {
    resource_response_.reset();
    byte_range_lower_bound_ = 0;
    byte_range_upper_bound_ = 0;
  }

 private:
  WebURLResponse resource_response_;
  // The lower bound of the byte range.
  int byte_range_lower_bound_;
  // The upper bound of the byte range.
  int byte_range_upper_bound_;
  // The handler for the data.
  WebPluginResourceClient* resource_client_;
};

class HeaderFlattener : public WebHTTPHeaderVisitor {
 public:
  HeaderFlattener(std::string* buf) : buf_(buf) {
  }

  virtual void visitHeader(const WebString& name, const WebString& value) {
    // TODO(darin): Should we really exclude headers with an empty value?
    if (!name.isEmpty() && !value.isEmpty()) {
      buf_->append(name.utf8());
      buf_->append(": ");
      buf_->append(value.utf8());
      buf_->append("\n");
    }
  }

 private:
  std::string* buf_;
};

std::string GetAllHeaders(const WebURLResponse& response) {
  // TODO(darin): It is possible for httpStatusText to be empty and still have
  // an interesting response, so this check seems wrong.
  std::string result;
  const WebString& status = response.httpStatusText();
  if (status.isEmpty())
    return result;

  // TODO(darin): Shouldn't we also report HTTP version numbers?
  result.append("HTTP ");
  result.append(WideToUTF8(FormatNumber(response.httpStatusCode())));
  result.append(" ");
  result.append(status.utf8());
  result.append("\n");

  HeaderFlattener flattener(&result);
  response.visitHTTPHeaderFields(&flattener);

  return result;
}

struct ResponseInfo {
  GURL url;
  std::string mime_type;
  uint32 last_modified;
  uint32 expected_length;
};

void GetResponseInfo(const WebURLResponse& response,
                     ResponseInfo* response_info) {
  response_info->url = response.url();
  response_info->mime_type = response.mimeType().utf8();

  // Measured in seconds since 12:00 midnight GMT, January 1, 1970.
  response_info->last_modified =
      static_cast<uint32>(response.lastModifiedDate());

  // If the length comes in as -1, then it indicates that it was not
  // read off the HTTP headers. We replicate Safari webkit behavior here,
  // which is to set it to 0.
  response_info->expected_length =
      static_cast<uint32>(std::max(response.expectedContentLength(), 0LL));

  WebString content_encoding =
      response.httpHeaderField(WebString::fromUTF8("Content-Encoding"));
  if (!content_encoding.isNull() &&
      !EqualsASCII(content_encoding, "identity")) {
    // Don't send the compressed content length to the plugin, which only
    // cares about the decoded length.
    response_info->expected_length = 0;
  }
}

}  // namespace

PassRefPtr<WebCore::Widget> WebPluginImpl::Create(
    const GURL& url,
    char** argn,
    char** argv,
    int argc,
    WebCore::HTMLPlugInElement* element,
    WebFrameImpl* frame,
    WebPluginDelegate* delegate,
    bool load_manually,
    const std::string& mime_type) {
  // NOTE: frame contains element

  WebPluginImpl* webplugin = new WebPluginImpl(
      frame, delegate, url, load_manually, mime_type, argc, argn, argv);

  if (!delegate->Initialize(url, argn, argv, argc, webplugin, load_manually)) {
    delegate->PluginDestroyed();
    delete webplugin;
    return NULL;
  }

  PassRefPtr<WebPluginContainerImpl> container =
      WebPluginContainerImpl::create(element, webplugin);
  webplugin->SetContainer(container.get());
  return container;
}

WebPluginImpl::WebPluginImpl(WebFrameImpl* webframe,
                             WebPluginDelegate* delegate,
                             const GURL& plugin_url,
                             bool load_manually,
                             const std::string& mime_type,
                             int arg_count,
                             char** arg_names,
                             char** arg_values)
    : windowless_(false),
      window_(NULL),
      webframe_(webframe),
      delegate_(delegate),
      container_(NULL),
      plugin_url_(plugin_url),
      load_manually_(load_manually),
      first_geometry_update_(true),
      ignore_response_error_(false),
      mime_type_(mime_type),
      ALLOW_THIS_IN_INITIALIZER_LIST(method_factory_(this)) {

  ArrayToVector(arg_count, arg_names, &arg_names_);
  ArrayToVector(arg_count, arg_values, &arg_values_);
}

// WebKit::WebPlugin -----------------------------------------------------------

void WebPluginImpl::destroy() {
  SetContainer(NULL);
  MessageLoop::current()->DeleteSoon(FROM_HERE, this);
}

NPObject* WebPluginImpl::scriptableObject() {
  return delegate_->GetPluginScriptableObject();
}

void WebPluginImpl::paint(WebCanvas* canvas, const WebRect& paint_rect) {
  // Note that |context| is only used when in windowless mode.
#if WEBKIT_USING_SKIA
  gfx::NativeDrawingContext context = canvas->beginPlatformPaint();
#elif WEBKIT_USING_CG
  gfx::NativeDrawingContext context = canvas;
#endif

  delegate_->Paint(context, paint_rect);

#if WEBKIT_USING_SKIA
  canvas->endPlatformPaint();
#endif
}

void WebPluginImpl::updateGeometry(
    const WebRect& window_rect, const WebRect& clip_rect,
    const WebVector<WebRect>& cutout_rects, bool is_visible) {
  if (window_) {
    WebViewDelegate* view_delegate = GetWebViewDelegate();
    if (view_delegate) {
      // Notify the window hosting the plugin (the WebViewDelegate) that
      // it needs to adjust the plugin, so that all the HWNDs can be moved
      // at the same time.
      WebPluginGeometry move;
      move.window = window_;
      move.window_rect = window_rect;
      move.clip_rect = clip_rect;
      for (size_t i = 0; i < cutout_rects.size(); ++i)
        move.cutout_rects.push_back(cutout_rects[i]);
      move.rects_valid = true;
      move.visible = is_visible;

      view_delegate->DidMovePlugin(move);
    }
  }

  if (first_geometry_update_ || window_rect != window_rect_ ||
      clip_rect != clip_rect_) {
    window_rect_ = window_rect;
    clip_rect_ = clip_rect;
    // Notify the plugin that its parameters have changed.
    delegate_->UpdateGeometry(window_rect_, clip_rect_);

    // Initiate a download on the plugin url. This should be done for the
    // first update geometry sequence. We need to ensure that the plugin
    // receives the geometry update before it starts receiving data.
    if (first_geometry_update_) {
      first_geometry_update_ = false;
      // An empty url corresponds to an EMBED tag with no src attribute.
      if (!load_manually_ && plugin_url_.is_valid()) {
        // The Flash plugin hangs for a while if it receives data before
        // receiving valid plugin geometry. By valid geometry we mean the
        // geometry received by a call to setFrameRect in the Webkit
        // layout code path. To workaround this issue we download the
        // plugin source url on a timer.
        MessageLoop::current()->PostDelayedTask(
            FROM_HERE, method_factory_.NewRunnableMethod(
                &WebPluginImpl::OnDownloadPluginSrcUrl), 0);
      }
    }
  }
}

void WebPluginImpl::updateFocus(bool focused) {
  if (focused && windowless_)
    delegate_->SetFocus();
}

void WebPluginImpl::updateVisibility(bool visible) {
  if (!window_)
    return;

  WebViewDelegate* view_delegate = GetWebViewDelegate();
  if (!view_delegate)
    return;

  WebPluginGeometry move;
  move.window = window_;
  move.window_rect = gfx::Rect();
  move.clip_rect = gfx::Rect();
  move.rects_valid = false;
  move.visible = visible;

  view_delegate->DidMovePlugin(move);
}

bool WebPluginImpl::acceptsInputEvents() {
  return windowless_;
}

bool WebPluginImpl::handleInputEvent(
    const WebInputEvent& event, WebCursorInfo& cursor_info) {
  return delegate_->HandleInputEvent(event, &cursor_info);
}

void WebPluginImpl::didReceiveResponse(const WebURLResponse& response) {
  ignore_response_error_ = false;

  ResponseInfo response_info;
  GetResponseInfo(response, &response_info);

  delegate_->DidReceiveManualResponse(
      response_info.url,
      response_info.mime_type,
      GetAllHeaders(response),
      response_info.expected_length,
      response_info.last_modified);
}

void WebPluginImpl::didReceiveData(const char* data, int data_length) {
  delegate_->DidReceiveManualData(data, data_length);
}

void WebPluginImpl::didFinishLoading() {
  delegate_->DidFinishManualLoading();
}

void WebPluginImpl::didFailLoading(const WebURLError& error) {
  if (!ignore_response_error_)
    delegate_->DidManualLoadFail();
}

// -----------------------------------------------------------------------------

WebPluginImpl::~WebPluginImpl() {
}


void WebPluginImpl::SetWindow(gfx::PluginWindowHandle window) {
  if (window) {
    DCHECK(!windowless_);  // Make sure not called twice.
    window_ = window;
    WebViewDelegate* view_delegate = GetWebViewDelegate();
    if (view_delegate) {
      // Tell the view delegate that the plugin window was created, so that it
      // can create necessary container widgets.
      view_delegate->CreatedPluginWindow(window);
    }
  } else {
    DCHECK(!window_);  // Make sure not called twice.
    windowless_ = true;
  }
}

void WebPluginImpl::WillDestroyWindow(gfx::PluginWindowHandle window) {
  DCHECK_EQ(window, window_);
  window_ = NULL;
  WebViewDelegate* view_delegate = GetWebViewDelegate();
  if (!view_delegate)
    return;
  view_delegate->WillDestroyPluginWindow(window);
}

GURL WebPluginImpl::CompleteURL(const char* url) {
  if (!webframe_) {
    NOTREACHED();
    return GURL();
  }
  // TODO(darin): Is conversion from UTF8 correct here?
  return webframe_->completeURL(WebString::fromUTF8(url));
}

bool WebPluginImpl::ExecuteScript(const std::string& url,
                                  const std::wstring& script,
                                  bool notify_needed,
                                  intptr_t notify_data,
                                  bool popups_allowed) {
  // This could happen if the WebPluginContainer was already deleted.
  if (!frame())
    return false;

  // Pending resource fetches should also not trigger a callback.
  webframe_->set_plugin_delegate(NULL);

  WebCore::String script_str(webkit_glue::StdWStringToString(script));

  // Note: the call to executeScript might result in the frame being
  // deleted, so add an extra reference to it in this scope.
  // For KJS, keeping a pointer to the JSBridge is enough, but for V8
  // we also need to addref the frame.
  WTF::RefPtr<WebCore::Frame> cur_frame(frame());

  WebCore::ScriptValue result =
      frame()->loader()->executeScript(script_str, popups_allowed);
  WebCore::String script_result;
  std::wstring wresult;
  bool succ = false;
  if (result.getString(script_result)) {
    succ = true;
    wresult = webkit_glue::StringToStdWString(script_result);
  }

  // delegate_ could be NULL because executeScript caused the container to be
  // deleted.
  if (delegate_)
    delegate_->SendJavaScriptStream(url, wresult, succ, notify_needed,
                                    notify_data);

  return succ;
}

void WebPluginImpl::CancelResource(int id) {
  for (size_t i = 0; i < clients_.size(); ++i) {
    if (clients_[i].id == id) {
      if (clients_[i].loader.get()) {
        clients_[i].loader->setDefersLoading(false);
        clients_[i].loader->cancel();
        RemoveClient(i);
      }
      return;
    }
  }
}

bool WebPluginImpl::SetPostData(WebURLRequest* request,
                                const char *buf,
                                uint32 length) {
  std::vector<std::string> names;
  std::vector<std::string> values;
  std::vector<char> body;
  bool rv = NPAPI::PluginHost::SetPostData(buf, length, &names, &values, &body);

  for (size_t i = 0; i < names.size(); ++i) {
    request->addHTTPHeaderField(webkit_glue::StdStringToWebString(names[i]),
                                webkit_glue::StdStringToWebString(values[i]));
  }

  WebString content_type_header = WebString::fromUTF8("Content-Type");
  const WebString& content_type =
      request->httpHeaderField(content_type_header);
  if (content_type.isEmpty()) {
    request->setHTTPHeaderField(
        content_type_header,
        WebString::fromUTF8("application/x-www-form-urlencoded"));
  }

  WebHTTPBody http_body;
  if (body.size()) {
    http_body.initialize();
    http_body.appendData(WebData(&body[0], body.size()));
  }
  request->setHTTPBody(http_body);

  return rv;
}

RoutingStatus WebPluginImpl::RouteToFrame(const char *method,
                                          bool is_javascript_url,
                                          const char* target, unsigned int len,
                                          const char* buf, bool is_file_data,
                                          bool notify, const char* url,
                                          GURL* unused) {
  // If there is no target, there is nothing to do
  if (!target)
    return NOT_ROUTED;

  // This could happen if the WebPluginContainer was already deleted.
  if (!frame())
    return NOT_ROUTED;

  // Take special action for JavaScript URLs
  WebCore::String str_target = target;
  if (is_javascript_url) {
    WebCore::Frame *frameTarget = frame()->tree()->find(str_target);
    // For security reasons, do not allow JavaScript on frames
    // other than this frame.
    if (frameTarget != frame()) {
      // FIXME - might be good to log this into a security
      //         log somewhere.
      return ROUTED;
    }

    // Route javascript calls back to the plugin.
    return NOT_ROUTED;
  }

  // If we got this far, we're routing content to a target frame.
  // Go fetch the URL.

  GURL complete_url = CompleteURL(url);

  if (strcmp(method, "GET") != 0) {
    // We're only going to route HTTP/HTTPS requests
    if (!(complete_url.SchemeIs("http") || complete_url.SchemeIs("https")))
      return INVALID_URL;
  }

  WebURLRequest request(complete_url);
  request.setHTTPMethod(WebString::fromUTF8(method));
  if (len > 0) {
    if (!is_file_data) {
      if (!SetPostData(&request, buf, len)) {
        // Uhoh - we're in trouble.  There isn't a good way
        // to recover at this point.  Break out.
        NOTREACHED();
        return ROUTED;
      }
    } else {
      // TODO: Support "file" mode.  For now, just break out
      // since proceeding may do something unintentional.
      NOTREACHED();
      return ROUTED;
    }
  }

  // TODO(darin): Eliminate these WebCore dependencies.

  WebCore::FrameLoadRequest load_request(
      *webkit_glue::WebURLRequestToResourceRequest(&request));
  load_request.setFrameName(str_target);
  WebCore::FrameLoader *loader = frame()->loader();
  // we actually don't know whether usergesture is true or false,
  // passing true since all we can do is assume it is okay.
  loader->loadFrameRequest(
      load_request,
      false,  // lock history
      false,  // lock back forward list
      0,      // event
      0);     // form state

  // loadFrameRequest() can cause the frame to go away.
  if (webframe_) {
    WebPluginDelegate* last_plugin = webframe_->plugin_delegate();
    if (last_plugin) {
      last_plugin->DidFinishLoadWithReason(NPRES_USER_BREAK);
      webframe_->set_plugin_delegate(NULL);
    }

    if (notify)
      webframe_->set_plugin_delegate(delegate_);
  }

  return ROUTED;
}

NPObject* WebPluginImpl::GetWindowScriptNPObject() {
  if (!webframe_) {
    NOTREACHED();
    return NULL;
  }
  return webframe_->windowObject();
}

NPObject* WebPluginImpl::GetPluginElement() {
  return container_->scriptableObjectForElement();
}

void WebPluginImpl::SetCookie(const GURL& url,
                              const GURL& policy_url,
                              const std::string& cookie) {
  WebKit::webKitClient()->setCookies(
      url, policy_url, WebString::fromUTF8(cookie));
}

std::string WebPluginImpl::GetCookies(const GURL& url, const GURL& policy_url) {
  return UTF16ToUTF8(WebKit::webKitClient()->cookies(url, policy_url));
}

void WebPluginImpl::ShowModalHTMLDialog(const GURL& url, int width, int height,
                                        const std::string& json_arguments,
                                        std::string* json_retval) {
  WebViewDelegate* view_delegate = GetWebViewDelegate();
  if (view_delegate) {
    view_delegate->ShowModalHTMLDialog(
        url, width, height, json_arguments, json_retval);
  }
}

void WebPluginImpl::OnMissingPluginStatus(int status) {
  NOTREACHED();
}

void WebPluginImpl::Invalidate() {
  if (container_)
    container_->invalidate();
}

void WebPluginImpl::InvalidateRect(const gfx::Rect& rect) {
  if (container_)
    container_->invalidateRect(rect);
}

void WebPluginImpl::OnDownloadPluginSrcUrl() {
  HandleURLRequestInternal("GET", false, NULL, 0, NULL, false, false,
                           plugin_url_.spec().c_str(), NULL, false,
                           false);
}

WebPluginResourceClient* WebPluginImpl::GetClientFromLoader(
    WebURLLoader* loader) {
  ClientInfo* client_info = GetClientInfoFromLoader(loader);
  if (client_info)
    return client_info->client;
  return NULL;
}

WebPluginImpl::ClientInfo* WebPluginImpl::GetClientInfoFromLoader(
    WebURLLoader* loader) {
  for (size_t i = 0; i < clients_.size(); ++i) {
    if (clients_[i].loader.get() == loader)
      return &clients_[i];
  }

  NOTREACHED();
  return 0;
}

void WebPluginImpl::willSendRequest(WebURLLoader* loader,
                                    WebURLRequest& request,
                                    const WebURLResponse&) {
  WebPluginResourceClient* client = GetClientFromLoader(loader);
  if (client)
    client->WillSendRequest(request.url());
}

void WebPluginImpl::didSendData(WebURLLoader* loader,
                                unsigned long long bytes_sent,
                                unsigned long long total_bytes_to_be_sent) {
}

void WebPluginImpl::didReceiveResponse(WebURLLoader* loader,
                                       const WebURLResponse& response) {
  static const int kHttpPartialResponseStatusCode = 206;
  static const int kHttpResponseSuccessStatusCode = 200;

  WebPluginResourceClient* client = GetClientFromLoader(loader);
  if (!client)
    return;

  ResponseInfo response_info;
  GetResponseInfo(response, &response_info);

  bool cancel = false;
  bool request_is_seekable = true;
  if (client->IsMultiByteResponseExpected()) {
    if (response.httpStatusCode() == kHttpPartialResponseStatusCode) {
      HandleHttpMultipartResponse(response, client);
      return;
    } else if (response.httpStatusCode() == kHttpResponseSuccessStatusCode) {
      // If the client issued a byte range request and the server responds with
      // HTTP 200 OK, it indicates that the server does not support byte range
      // requests.
      // We need to emulate Firefox behavior by doing the following:-
      // 1. Destroy the plugin instance in the plugin process. Ensure that
      //    existing resource requests initiated for the plugin instance
      //    continue to remain valid.
      // 2. Create a new plugin instance and notify it about the response
      //    received here.
      if (!ReinitializePluginForResponse(loader)) {
        NOTREACHED();
        return;
      }

      // The server does not support byte range requests. No point in creating
      // seekable streams.
      request_is_seekable = false;

      delete client;
      client = NULL;

      // Create a new resource client for this request.
      for (size_t i = 0; i < clients_.size(); ++i) {
        if (clients_[i].loader.get() == loader) {
          WebPluginResourceClient* resource_client =
              delegate_->CreateResourceClient(clients_[i].id, plugin_url_,
                                              false, 0, NULL);
          clients_[i].client = resource_client;
          client = resource_client;
          break;
        }
      }

      DCHECK(client != NULL);
    }
  }

  // Calling into a plugin could result in reentrancy if the plugin yields
  // control to the OS like entering a modal loop etc. Prevent this by
  // stopping further loading until the plugin notifies us that it is ready to
  // accept data
  loader->setDefersLoading(true);

  client->DidReceiveResponse(
      response_info.mime_type,
      GetAllHeaders(response),
      response_info.expected_length,
      response_info.last_modified,
      request_is_seekable);

  // Bug http://b/issue?id=925559. The flash plugin would not handle the HTTP
  // error codes in the stream header and as a result, was unaware of the
  // fate of the HTTP requests issued via NPN_GetURLNotify. Webkit and FF
  // destroy the stream and invoke the NPP_DestroyStream function on the
  // plugin if the HTTP request fails.
  const GURL& url = response.url();
  if (url.SchemeIs("http") || url.SchemeIs("https")) {
    if (response.httpStatusCode() < 100 || response.httpStatusCode() >= 400) {
      // The plugin instance could be in the process of deletion here.
      // Verify if the WebPluginResourceClient instance still exists before
      // use.
      WebPluginResourceClient* resource_client = GetClientFromLoader(loader);
      if (resource_client) {
        loader->cancel();
        resource_client->DidFail();
        RemoveClient(loader);
      }
    }
  }
}

void WebPluginImpl::didReceiveData(WebURLLoader* loader,
                                   const char *buffer,
                                   int length, long long) {
  WebPluginResourceClient* client = GetClientFromLoader(loader);
  if (!client)
    return;
  MultiPartResponseHandlerMap::iterator index =
      multi_part_response_map_.find(client);
  if (index != multi_part_response_map_.end()) {
    MultipartResponseDelegate* multi_part_handler = (*index).second;
    DCHECK(multi_part_handler != NULL);
    multi_part_handler->OnReceivedData(buffer, length);
  } else {
    loader->setDefersLoading(true);
    client->DidReceiveData(buffer, length, 0);
  }
}

void WebPluginImpl::didFinishLoading(WebURLLoader* loader) {
  ClientInfo* client_info = GetClientInfoFromLoader(loader);
  if (client_info && client_info->client) {
    MultiPartResponseHandlerMap::iterator index =
      multi_part_response_map_.find(client_info->client);
    if (index != multi_part_response_map_.end()) {
      delete (*index).second;
      multi_part_response_map_.erase(index);

      WebView* webview = webframe_->GetWebViewImpl();
      webview->GetDelegate()->DidStopLoading(webview);
    }
    loader->setDefersLoading(true);
    WebPluginResourceClient* resource_client = client_info->client;
    // The ClientInfo can get deleted in the call to DidFinishLoading below.
    // It is not safe to access this structure after that.
    client_info->client = NULL;
    resource_client->DidFinishLoading();
  }
}

void WebPluginImpl::didFail(WebURLLoader* loader,
                            const WebURLError&) {
  ClientInfo* client_info = GetClientInfoFromLoader(loader);
  if (client_info && client_info->client) {
    loader->setDefersLoading(true);
    WebPluginResourceClient* resource_client = client_info->client;
    // The ClientInfo can get deleted in the call to DidFail below.
    // It is not safe to access this structure after that.
    client_info->client = NULL;
    resource_client->DidFail();
  }
}

void WebPluginImpl::RemoveClient(size_t i) {
  clients_.erase(clients_.begin() + i);
}

void WebPluginImpl::RemoveClient(WebURLLoader* loader) {
  for (size_t i = 0; i < clients_.size(); ++i) {
    if (clients_[i].loader.get() == loader) {
      RemoveClient(i);
      return;
    }
  }
}

void WebPluginImpl::SetContainer(WebPluginContainer* container) {
  if (container == NULL) {
    TearDownPluginInstance(NULL);
  }
  container_ = container;
}

void WebPluginImpl::HandleURLRequest(const char *method,
                                     bool is_javascript_url,
                                     const char* target, unsigned int len,
                                     const char* buf, bool is_file_data,
                                     bool notify, const char* url,
                                     intptr_t notify_data, bool popups_allowed) {
  HandleURLRequestInternal(method, is_javascript_url, target, len, buf,
                           is_file_data, notify, url, notify_data,
                           popups_allowed, true);
}

void WebPluginImpl::HandleURLRequestInternal(
    const char *method, bool is_javascript_url, const char* target,
    unsigned int len, const char* buf, bool is_file_data, bool notify,
    const char* url, intptr_t notify_data, bool popups_allowed,
    bool use_plugin_src_as_referrer) {
  // For this request, we either route the output to a frame
  // because a target has been specified, or we handle the request
  // here, i.e. by executing the script if it is a javascript url
  // or by initiating a download on the URL, etc. There is one special
  // case in that the request is a javascript url and the target is "_self",
  // in which case we route the output to the plugin rather than routing it
  // to the plugin's frame.
  GURL complete_url;
  int routing_status = RouteToFrame(method, is_javascript_url, target, len,
                                    buf, is_file_data, notify, url,
                                    &complete_url);
  if (routing_status == ROUTED) {
    // The delegate could have gone away because of this call.
    if (delegate_)
      delegate_->URLRequestRouted(url, notify, notify_data);
    return;
  }

  if (is_javascript_url) {
    std::string original_url = url;

    // Convert the javascript: URL to javascript by unescaping. WebCore uses
    // decode_string for this, so we do, too.
    std::string escaped_script = original_url.substr(strlen("javascript:"));
    WebCore::String script = WebCore::decodeURLEscapeSequences(
        WebCore::String(escaped_script.data(),
                                  static_cast<int>(escaped_script.length())));

    ExecuteScript(original_url, webkit_glue::StringToStdWString(script), notify,
                  notify_data, popups_allowed);
  } else {
    GURL complete_url = CompleteURL(url);

    int resource_id = GetNextResourceId();
    WebPluginResourceClient* resource_client = delegate_->CreateResourceClient(
        resource_id, complete_url, notify, notify_data, NULL);

    // If the RouteToFrame call returned a failure then inform the result
    // back to the plugin asynchronously.
    if ((routing_status == INVALID_URL) ||
        (routing_status == GENERAL_FAILURE)) {
      resource_client->DidFail();
      return;
    }

    InitiateHTTPRequest(resource_id, resource_client, method, buf, len,
                        complete_url, NULL, use_plugin_src_as_referrer);
  }
}

int WebPluginImpl::GetNextResourceId() {
  static int next_id = 0;
  return ++next_id;
}

bool WebPluginImpl::InitiateHTTPRequest(int resource_id,
                                        WebPluginResourceClient* client,
                                        const char* method, const char* buf,
                                        int buf_len,
                                        const GURL& url,
                                        const char* range_info,
                                        bool use_plugin_src_as_referrer) {
  if (!client) {
    NOTREACHED();
    return false;
  }

  ClientInfo info;
  info.id = resource_id;
  info.client = client;
  info.request.initialize();
  info.request.setURL(url);
  info.request.setRequestorProcessID(delegate_->GetProcessId());
  info.request.setTargetType(WebURLRequest::TargetIsObject);
  info.request.setHTTPMethod(WebString::fromUTF8(method));

  if (range_info) {
    info.request.addHTTPHeaderField(WebString::fromUTF8("Range"),
                                    WebString::fromUTF8(range_info));
  }

  if (strcmp(method, "POST") == 0) {
    // Adds headers or form data to a request.  This must be called before
    // we initiate the actual request.
    SetPostData(&info.request, buf, buf_len);
  }

  // GetURL/PostURL requests initiated explicitly by plugins should specify the
  // plugin SRC url as the referrer if it is available.
  GURL referrer_url;
  if (use_plugin_src_as_referrer && !plugin_url_.spec().empty())
    referrer_url = plugin_url_;
  webframe_->setReferrerForRequest(info.request, referrer_url);

  // Sets the routing id to associate the ResourceRequest with the RenderView.
  webframe_->dispatchWillSendRequest(info.request);

  info.loader.reset(WebKit::webKitClient()->createURLLoader());
  if (!info.loader.get())
    return false;
  info.loader->loadAsynchronously(info.request, this);

  clients_.push_back(info);
  return true;
}

void WebPluginImpl::CancelDocumentLoad() {
  if (frame()->loader()->activeDocumentLoader()) {
    ignore_response_error_ = true;
    frame()->loader()->activeDocumentLoader()->stopLoading();
  }
}

void WebPluginImpl::InitiateHTTPRangeRequest(const char* url,
                                             const char* range_info,
                                             intptr_t existing_stream,
                                             bool notify_needed,
                                             intptr_t notify_data) {
  int resource_id = GetNextResourceId();
  GURL complete_url = CompleteURL(url);

  WebPluginResourceClient* resource_client = delegate_->CreateResourceClient(
      resource_id, complete_url, notify_needed, notify_data, existing_stream);
  InitiateHTTPRequest(
      resource_id, resource_client, "GET", NULL, 0, complete_url, range_info,
      true);
}

void WebPluginImpl::SetDeferResourceLoading(int resource_id, bool defer) {
  std::vector<ClientInfo>::iterator client_index = clients_.begin();
  while (client_index != clients_.end()) {
    ClientInfo& client_info = *client_index;

    if (client_info.id == resource_id) {
      client_info.loader->setDefersLoading(defer);
      break;
    }
    client_index++;
  }
}

void WebPluginImpl::HandleHttpMultipartResponse(
    const WebURLResponse& response, WebPluginResourceClient* client) {
  std::string multipart_boundary;
  if (!MultipartResponseDelegate::ReadMultipartBoundary(
          response, &multipart_boundary)) {
    NOTREACHED();
    return;
  }

  WebView* webview = webframe_->GetWebViewImpl();
  webview->GetDelegate()->DidStartLoading(webview);

  MultiPartResponseClient* multi_part_response_client =
      new MultiPartResponseClient(client);

  MultipartResponseDelegate* multi_part_response_handler =
      new MultipartResponseDelegate(multi_part_response_client, NULL,
                                    response,
                                    multipart_boundary);
  multi_part_response_map_[client] = multi_part_response_handler;
}

bool WebPluginImpl::ReinitializePluginForResponse(
    WebURLLoader* loader) {
  WebFrameImpl* webframe = webframe_;
  if (!webframe)
    return false;

  WebViewImpl* webview = webframe->GetWebViewImpl();
  if (!webview)
    return false;

  WebPluginContainer* container_widget = container_;

  // Destroy the current plugin instance.
  TearDownPluginInstance(loader);

  container_ = container_widget;
  webframe_ = webframe;

  WebViewDelegate* webview_delegate = webview->GetDelegate();
  std::string actual_mime_type;
  WebPluginDelegate* plugin_delegate =
      webview_delegate->CreatePluginDelegate(webview, plugin_url_,
                                             mime_type_, std::string(),
                                             &actual_mime_type);

  char** arg_names = new char*[arg_names_.size()];
  char** arg_values = new char*[arg_values_.size()];

  for (unsigned int index = 0; index < arg_names_.size(); ++index) {
    arg_names[index] = const_cast<char*>(arg_names_[index].c_str());
    arg_values[index] = const_cast<char*>(arg_values_[index].c_str());
  }

  bool init_ok = plugin_delegate->Initialize(plugin_url_, arg_names,
                                             arg_values, arg_names_.size(),
                                             this, load_manually_);
  delete[] arg_names;
  delete[] arg_values;

  if (!init_ok) {
    container_ = NULL;
    // TODO(iyengar) Should we delete the current plugin instance here?
    return false;
  }

  mime_type_ = actual_mime_type;
  delegate_ = plugin_delegate;

  // Force a geometry update to occur to ensure that the plugin becomes
  // visible.  TODO(darin): Avoid this cast!
  static_cast<WebPluginContainerImpl*>(container_)->frameRectsChanged();
  // The plugin move sequences accumulated via DidMove are sent to the browser
  // whenever the renderer paints. Force a paint here to ensure that changes
  // to the plugin window are propagated to the browser.
  container_->invalidate();
  return true;
}

void WebPluginImpl::ArrayToVector(int total_values, char** values,
                                  std::vector<std::string>* value_vector) {
  DCHECK(value_vector != NULL);
  for (int index = 0; index < total_values; ++index) {
    value_vector->push_back(values[index]);
  }
}

void WebPluginImpl::TearDownPluginInstance(
    WebURLLoader* loader_to_ignore) {
  // The frame maintains a list of JSObjects which are related to this
  // plugin.  Tell the frame we're gone so that it can invalidate all
  // of those sub JSObjects.
  if (frame()) {
    ASSERT(container_);
    // TODO(darin): Avoid this cast!
    frame()->script()->cleanupScriptObjectsForPlugin(
        static_cast<WebKit::WebPluginContainerImpl*>(container_));
  }

  if (delegate_) {
    // Call PluginDestroyed() first to prevent the plugin from calling us back
    // in the middle of tearing down the render tree.
    delegate_->PluginDestroyed();
    delegate_ = NULL;
  }

  // Cancel any pending requests because otherwise this deleted object will
  // be called by the ResourceDispatcher.
  std::vector<ClientInfo>::iterator client_index = clients_.begin();
  while (client_index != clients_.end()) {
    ClientInfo& client_info = *client_index;

    if (loader_to_ignore == client_info.loader) {
      client_index++;
      continue;
    }

    if (client_info.loader.get())
      client_info.loader->cancel();

    WebPluginResourceClient* resource_client = client_info.client;
    client_index = clients_.erase(client_index);
    if (resource_client)
      resource_client->DidFail();
  }

  // This needs to be called now and not in the destructor since the
  // webframe_ might not be valid anymore.
  webframe_->set_plugin_delegate(NULL);
  webframe_ = NULL;
  method_factory_.RevokeAll();
}

WebViewDelegate* WebPluginImpl::GetWebViewDelegate() {
  if (!webframe_)
    return NULL;
  WebViewImpl* webview = webframe_->GetWebViewImpl();
  return webview ? webview->delegate() : NULL;
}
