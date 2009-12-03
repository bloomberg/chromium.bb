// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/gfx/rect.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "net/base/escape.h"
#include "net/base/net_errors.h"
#include "skia/ext/platform_canvas.h"
#include "third_party/WebKit/WebKit/chromium/public/WebConsoleMessage.h"
#include "third_party/WebKit/WebKit/chromium/public/WebCString.h"
#include "third_party/WebKit/WebKit/chromium/public/WebCursorInfo.h"
#include "third_party/WebKit/WebKit/chromium/public/WebDevToolsAgent.h"
#include "third_party/WebKit/WebKit/chromium/public/WebData.h"
#include "third_party/WebKit/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/WebKit/chromium/public/WebHTTPBody.h"
#include "third_party/WebKit/WebKit/chromium/public/WebHTTPHeaderVisitor.h"
#include "third_party/WebKit/WebKit/chromium/public/WebInputEvent.h"
#include "third_party/WebKit/WebKit/chromium/public/WebKit.h"
#include "third_party/WebKit/WebKit/chromium/public/WebKitClient.h"
#include "third_party/WebKit/WebKit/chromium/public/WebPluginContainer.h"
#include "third_party/WebKit/WebKit/chromium/public/WebPluginParams.h"
#include "third_party/WebKit/WebKit/chromium/public/WebRect.h"
#include "third_party/WebKit/WebKit/chromium/public/WebURL.h"
#include "third_party/WebKit/WebKit/chromium/public/WebURLError.h"
#include "third_party/WebKit/WebKit/chromium/public/WebURLLoader.h"
#include "third_party/WebKit/WebKit/chromium/public/WebURLLoaderClient.h"
#include "third_party/WebKit/WebKit/chromium/public/WebURLResponse.h"
#include "third_party/WebKit/WebKit/chromium/public/WebView.h"
#include "webkit/glue/multipart_response_delegate.h"
#include "webkit/glue/webplugin_impl.h"
#include "webkit/glue/plugins/plugin_host.h"
#include "webkit/glue/plugins/plugin_instance.h"
#include "webkit/glue/webplugin_delegate.h"
#include "webkit/glue/webplugin_page_delegate.h"
#include "googleurl/src/gurl.h"

using WebKit::WebCanvas;
using WebKit::WebConsoleMessage;
using WebKit::WebCString;
using WebKit::WebCursorInfo;
using WebKit::WebData;
using WebKit::WebDataSource;
using WebKit::WebDevToolsAgent;
using WebKit::WebFrame;
using WebKit::WebHTTPBody;
using WebKit::WebHTTPHeaderVisitor;
using WebKit::WebInputEvent;
using WebKit::WebKeyboardEvent;
using WebKit::WebMouseEvent;
using WebKit::WebPluginContainer;
using WebKit::WebPluginParams;
using WebKit::WebRect;
using WebKit::WebString;
using WebKit::WebURL;
using WebKit::WebURLError;
using WebKit::WebURLLoader;
using WebKit::WebURLLoaderClient;
using WebKit::WebURLRequest;
using WebKit::WebURLResponse;
using WebKit::WebVector;
using WebKit::WebView;
using webkit_glue::MultipartResponseDelegate;

namespace webkit_glue {
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
      WebURLLoader*, const char* data, int data_size) {
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
  result = StringPrintf("HTTP %d ", response.httpStatusCode());
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

// WebKit::WebPlugin ----------------------------------------------------------

bool WebPluginImpl::initialize(WebPluginContainer* container) {
  if (!page_delegate_)
    return false;

  std::string actual_mime_type;
  WebPluginDelegate* plugin_delegate = page_delegate_->CreatePluginDelegate(
      plugin_url_, mime_type_, &actual_mime_type);
  if (!plugin_delegate)
    return NULL;

  // Set the container before Initialize because the plugin may
  // synchronously call NPN_GetValue to get its container during its
  // initialization.
  SetContainer(container);
  bool ok = plugin_delegate->Initialize(
      plugin_url_, arg_names_, arg_values_, this, load_manually_);
  if (!ok) {
    plugin_delegate->PluginDestroyed();
    return false;
  }

  if (!actual_mime_type.empty())
    mime_type_ = actual_mime_type;
  delegate_ = plugin_delegate;

  return true;
}

void WebPluginImpl::destroy() {
  SetContainer(NULL);
  MessageLoop::current()->DeleteSoon(FROM_HERE, this);
}

NPObject* WebPluginImpl::scriptableObject() {
  return delegate_->GetPluginScriptableObject();
}

void WebPluginImpl::paint(WebCanvas* canvas, const WebRect& paint_rect) {
  if (!delegate_)
    return;

  // Note that |canvas| is only used when in windowless mode.
  delegate_->Paint(canvas, paint_rect);
}

void WebPluginImpl::updateGeometry(
    const WebRect& window_rect, const WebRect& clip_rect,
    const WebVector<WebRect>& cutout_rects, bool is_visible) {
  if (window_ && page_delegate_) {
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

    page_delegate_->DidMovePlugin(move);
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
  if (!window_ || !page_delegate_)
    return;

  WebPluginGeometry move;
  move.window = window_;
  move.window_rect = gfx::Rect();
  move.clip_rect = gfx::Rect();
  move.rects_valid = false;
  move.visible = visible;

  page_delegate_->DidMovePlugin(move);
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

void WebPluginImpl::didFinishLoadingFrameRequest(
    const WebURL& url, void* notify_data) {
  if (delegate_) {
    delegate_->DidFinishLoadWithReason(
        url, NPRES_DONE, reinterpret_cast<intptr_t>(notify_data));
  }
}

void WebPluginImpl::didFailLoadingFrameRequest(
    const WebURL& url, void* notify_data, const WebURLError& error) {
  if (!delegate_)
    return;

  NPReason reason =
      error.reason == net::ERR_ABORTED ? NPRES_USER_BREAK : NPRES_NETWORK_ERR;
  delegate_->DidFinishLoadWithReason(
      url, reason, reinterpret_cast<intptr_t>(notify_data));
}

// -----------------------------------------------------------------------------

WebPluginImpl::WebPluginImpl(
    WebFrame* webframe, const WebPluginParams& params,
    const base::WeakPtr<WebPluginPageDelegate>& page_delegate)
    : windowless_(false),
      window_(NULL),
      page_delegate_(page_delegate),
      webframe_(webframe),
      delegate_(NULL),
      container_(NULL),
      plugin_url_(params.url),
      load_manually_(params.loadManually),
      first_geometry_update_(true),
      ignore_response_error_(false),
      mime_type_(params.mimeType.utf8()),
      ALLOW_THIS_IN_INITIALIZER_LIST(method_factory_(this)) {
  DCHECK_EQ(params.attributeNames.size(), params.attributeValues.size());
  StringToLowerASCII(&mime_type_);

  for (size_t i = 0; i < params.attributeNames.size(); ++i) {
    arg_names_.push_back(params.attributeNames[i].utf8());
    arg_values_.push_back(params.attributeValues[i].utf8());
  }
}

WebPluginImpl::~WebPluginImpl() {
}

void WebPluginImpl::SetWindow(gfx::PluginWindowHandle window) {
  if (window) {
    DCHECK(!windowless_);  // Make sure not called twice.
    window_ = window;
    if (page_delegate_) {
      // Tell the view delegate that the plugin window was created, so that it
      // can create necessary container widgets.
      page_delegate_->CreatedPluginWindow(window);
    }
  } else {
    DCHECK(!window_);  // Make sure not called twice.
    windowless_ = true;
  }
}

void WebPluginImpl::WillDestroyWindow(gfx::PluginWindowHandle window) {
  DCHECK_EQ(window, window_);
  window_ = NULL;
  if (page_delegate_)
    page_delegate_->WillDestroyPluginWindow(window);
}

GURL WebPluginImpl::CompleteURL(const char* url) {
  if (!webframe_) {
    NOTREACHED();
    return GURL();
  }
  // TODO(darin): Is conversion from UTF8 correct here?
  return webframe_->completeURL(WebString::fromUTF8(url));
}

void WebPluginImpl::CancelResource(unsigned long id) {
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
    request->addHTTPHeaderField(WebString::fromUTF8(names[i]),
                                WebString::fromUTF8(values[i]));
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

WebPluginImpl::RoutingStatus WebPluginImpl::RouteToFrame(
    const char *method,
    bool is_javascript_url,
    const char* target,
    unsigned int len,
    const char* buf,
    bool is_file_data,
    bool notify_needed,
    intptr_t notify_data,
    const char* url) {
  // If there is no target, there is nothing to do
  if (!target)
    return NOT_ROUTED;

  // This could happen if the WebPluginContainer was already deleted.
  if (!webframe_)
    return NOT_ROUTED;

  WebString target_str = WebString::fromUTF8(target);

  // Take special action for JavaScript URLs
  if (is_javascript_url) {
    WebFrame* target_frame =
        webframe_->view()->findFrameByName(target_str, webframe_);
    // For security reasons, do not allow JavaScript on frames
    // other than this frame.
    if (target_frame != webframe_) {
      // TODO(darin): Localize this message.
      const char kMessage[] =
          "Ignoring cross-frame javascript URL load requested by plugin.";
      webframe_->addMessageToConsole(
          WebConsoleMessage(WebConsoleMessage::LevelError,
                            WebString::fromUTF8(kMessage)));
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

  container_->loadFrameRequest(request, target_str, notify_needed,
                               reinterpret_cast<void*>(notify_data));
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
                              const GURL& first_party_for_cookies,
                              const std::string& cookie) {
  WebKit::webKitClient()->setCookies(
      url, first_party_for_cookies, WebString::fromUTF8(cookie));
}

std::string WebPluginImpl::GetCookies(const GURL& url,
                                      const GURL& first_party_for_cookies) {
  return UTF16ToUTF8(WebKit::webKitClient()->cookies(url,
                                                     first_party_for_cookies));
}

void WebPluginImpl::ShowModalHTMLDialog(const GURL& url, int width, int height,
                                        const std::string& json_arguments,
                                        std::string* json_retval) {
  if (page_delegate_) {
    page_delegate_->ShowModalHTMLDialogForPlugin(
        url, gfx::Size(width, height), json_arguments, json_retval);
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

  if (WebDevToolsAgent* devtools_agent = webframe_->view()->devToolsAgent()) {
    ClientInfo* client_info = GetClientInfoFromLoader(loader);
    if (client_info)
      devtools_agent->didReceiveResponse(client_info->id, response);
  }

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
      ClientInfo* client_info = GetClientInfoFromLoader(loader);
      if (client_info) {
        client_info->pending_failure_notification = true;
      }
    }
  }
}

void WebPluginImpl::didReceiveData(WebURLLoader* loader,
                                   const char *buffer,
                                   int length) {
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

  if (WebDevToolsAgent* devtools_agent = webframe_->view()->devToolsAgent()) {
    ClientInfo* client_info = GetClientInfoFromLoader(loader);
    if (client_info)
      devtools_agent->didReceiveData(client_info->id, length);
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
      if (page_delegate_)
        page_delegate_->DidStopLoadingForPlugin();
    }
    loader->setDefersLoading(true);
    WebPluginResourceClient* resource_client = client_info->client;
    // The ClientInfo can get deleted in the call to DidFinishLoading below.
    // It is not safe to access this structure after that.
    client_info->client = NULL;
    resource_client->DidFinishLoading();

    if (WebDevToolsAgent* devtools_agent = webframe_->view()->devToolsAgent())
      devtools_agent->didFinishLoading(client_info->id);
  }
}

void WebPluginImpl::didFail(WebURLLoader* loader,
                            const WebURLError& error) {
  ClientInfo* client_info = GetClientInfoFromLoader(loader);
  if (client_info && client_info->client) {
    loader->setDefersLoading(true);
    WebPluginResourceClient* resource_client = client_info->client;
    // The ClientInfo can get deleted in the call to DidFail below.
    // It is not safe to access this structure after that.
    client_info->client = NULL;
    resource_client->DidFail();

    if (WebDevToolsAgent* devtools_agent = webframe_->view()->devToolsAgent())
      devtools_agent->didFailLoading(client_info->id, error);
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
  if (!container)
    TearDownPluginInstance(NULL);
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
  RoutingStatus routing_status =
      RouteToFrame(method, is_javascript_url, target, len, buf, is_file_data,
                   notify, notify_data, url);
  if (routing_status == ROUTED)
    return;

  if (is_javascript_url) {
    GURL gurl(url);
    WebString result = container_->executeScriptURL(gurl, popups_allowed);

    // delegate_ could be NULL because executeScript caused the container to
    // be deleted.
    if (delegate_) {
      delegate_->SendJavaScriptStream(
          gurl, result.utf8(), !result.isNull(), notify, notify_data);
    }
  } else {
    GURL complete_url = CompleteURL(url);

    unsigned long resource_id = GetNextResourceId();
    if (!resource_id)
      return;

    WebPluginResourceClient* resource_client = delegate_->CreateResourceClient(
        resource_id, complete_url, notify, notify_data, NULL);
    if (!resource_client)
      return;

    // If the RouteToFrame call returned a failure then inform the result
    // back to the plugin asynchronously.
    if ((routing_status == INVALID_URL) ||
        (routing_status == GENERAL_FAILURE)) {
      resource_client->DidFail();
      return;
    }

    // CreateResourceClient() sends a synchronous IPC message so it's possible
    // that TearDownPluginInstance() may have been called in the nested
    // message loop.  If so, don't start the request.
    if (!delegate_)
      return;

    InitiateHTTPRequest(resource_id, resource_client, method, buf, len,
                        complete_url, NULL, use_plugin_src_as_referrer);
  }
}

unsigned long WebPluginImpl::GetNextResourceId() {
  if (!webframe_)
    return 0;
  WebView* view = webframe_->view();
  if (!view)
    return 0;
  return view->createUniqueIdentifierForRequest();
}

bool WebPluginImpl::InitiateHTTPRequest(unsigned long resource_id,
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
  info.pending_failure_notification = false;

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
  if (WebDevToolsAgent* devtools_agent = webframe_->view()->devToolsAgent()) {
    devtools_agent->identifierForInitialRequest(resource_id, webframe_,
                                                info.request);
    devtools_agent->willSendRequest(resource_id, info.request);
  }

  info.loader.reset(WebKit::webKitClient()->createURLLoader());
  if (!info.loader.get())
    return false;
  info.loader->loadAsynchronously(info.request, this);

  clients_.push_back(info);
  return true;
}

void WebPluginImpl::CancelDocumentLoad() {
  if (webframe_) {
    ignore_response_error_ = true;
    webframe_->stopLoading();
  }
}

void WebPluginImpl::InitiateHTTPRangeRequest(const char* url,
                                             const char* range_info,
                                             intptr_t existing_stream,
                                             bool notify_needed,
                                             intptr_t notify_data) {
  unsigned long resource_id = GetNextResourceId();
  if (!resource_id)
    return;

  GURL complete_url = CompleteURL(url);

  WebPluginResourceClient* resource_client = delegate_->CreateResourceClient(
      resource_id, complete_url, notify_needed, notify_data, existing_stream);
  InitiateHTTPRequest(
      resource_id, resource_client, "GET", NULL, 0, complete_url, range_info,
      true);
}

void WebPluginImpl::SetDeferResourceLoading(unsigned long resource_id,
                                            bool defer) {
  std::vector<ClientInfo>::iterator client_index = clients_.begin();
  while (client_index != clients_.end()) {
    ClientInfo& client_info = *client_index;

    if (client_info.id == resource_id) {
      client_info.loader->setDefersLoading(defer);

      // If we determined that the request had failed via the HTTP headers
      // in the response then we send out a failure notification to the
      // plugin process, as certain plugins don't handle HTTP failure codes
      // correctly.
      if (!defer && client_info.client &&
          client_info.pending_failure_notification) {
        // The ClientInfo and the iterator can become invalid due to the call
        // to DidFail below.
        WebPluginResourceClient* resource_client = client_info.client;
        client_info.loader->cancel();
        clients_.erase(client_index++);
        resource_client->DidFail();

        // Report that resource loading finished.
        WebDevToolsAgent* devtools_agent = webframe_->view()->devToolsAgent();
        if (devtools_agent)
          devtools_agent->didFinishLoading(resource_id);
      }
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

  if (page_delegate_)
    page_delegate_->DidStartLoadingForPlugin();

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
  WebFrame* webframe = webframe_;
  if (!webframe)
    return false;

  WebView* webview = webframe->view();
  if (!webview)
    return false;

  WebPluginContainer* container_widget = container_;

  // Destroy the current plugin instance.
  TearDownPluginInstance(loader);

  container_ = container_widget;
  webframe_ = webframe;

  std::string actual_mime_type;
  WebPluginDelegate* plugin_delegate = page_delegate_->CreatePluginDelegate(
      plugin_url_, mime_type_, &actual_mime_type);

  bool ok = plugin_delegate->Initialize(
      plugin_url_, arg_names_, arg_values_, this, load_manually_);

  if (!ok) {
    container_ = NULL;
    // TODO(iyengar) Should we delete the current plugin instance here?
    return false;
  }

  mime_type_ = actual_mime_type;
  delegate_ = plugin_delegate;

  // Force a geometry update to occur to ensure that the plugin becomes
  // visible.
  container_->reportGeometry();

  // The plugin move sequences accumulated via DidMove are sent to the browser
  // whenever the renderer paints. Force a paint here to ensure that changes
  // to the plugin window are propagated to the browser.
  container_->invalidate();
  return true;
}

void WebPluginImpl::TearDownPluginInstance(
    WebURLLoader* loader_to_ignore) {
  // The container maintains a list of JSObjects which are related to this
  // plugin.  Tell the frame we're gone so that it can invalidate all of
  // those sub JSObjects.
  if (container_)
    container_->clearScriptObjects();

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

    client_index = clients_.erase(client_index);
  }

  // This needs to be called now and not in the destructor since the
  // webframe_ might not be valid anymore.
  webframe_ = NULL;
  method_factory_.RevokeAll();
}

}  // namespace webkit_glue
