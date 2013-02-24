// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// An implementation of WebURLLoader in terms of ResourceLoaderBridge.

#include "webkit/glue/weburlloader_impl.h"

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/process_util.h"
#include "base/string_util.h"
#include "base/time.h"
#include "net/base/data_url.h"
#include "net/base/load_flags.h"
#include "net/base/mime_util.h"
#include "net/base/net_errors.h"
#include "net/base/net_util.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_util.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebHTTPHeaderVisitor.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebHTTPLoadInfo.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebURL.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebURLError.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebURLLoaderClient.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebURLLoadTiming.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebURLRequest.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebURLResponse.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebSecurityPolicy.h"
#include "webkit/base/file_path_string_conversions.h"
#include "webkit/glue/ftp_directory_listing_response_delegate.h"
#include "webkit/glue/multipart_response_delegate.h"
#include "webkit/glue/resource_loader_bridge.h"
#include "webkit/glue/resource_request_body.h"
#include "webkit/glue/webkitplatformsupport_impl.h"
#include "webkit/glue/weburlrequest_extradata_impl.h"
#include "webkit/glue/weburlresponse_extradata_impl.h"

using base::Time;
using base::TimeTicks;
using WebKit::WebData;
using WebKit::WebHTTPBody;
using WebKit::WebHTTPHeaderVisitor;
using WebKit::WebHTTPLoadInfo;
using WebKit::WebReferrerPolicy;
using WebKit::WebSecurityPolicy;
using WebKit::WebString;
using WebKit::WebURL;
using WebKit::WebURLError;
using WebKit::WebURLLoadTiming;
using WebKit::WebURLLoader;
using WebKit::WebURLLoaderClient;
using WebKit::WebURLRequest;
using WebKit::WebURLResponse;

namespace webkit_glue {

// Utilities ------------------------------------------------------------------

namespace {

const char kThrottledErrorDescription[] =
    "Request throttled. Visit http://dev.chromium.org/throttling for more "
    "information.";

class HeaderFlattener : public WebHTTPHeaderVisitor {
 public:
  explicit HeaderFlattener(int load_flags)
      : load_flags_(load_flags),
        has_accept_header_(false) {
  }

  virtual void visitHeader(const WebString& name, const WebString& value) {
    // TODO(darin): is UTF-8 really correct here?  It is if the strings are
    // already ASCII (i.e., if they are already escaped properly).
    const std::string& name_utf8 = name.utf8();
    const std::string& value_utf8 = value.utf8();

    // Skip over referrer headers found in the header map because we already
    // pulled it out as a separate parameter.
    if (LowerCaseEqualsASCII(name_utf8, "referer"))
      return;

    // Skip over "Cache-Control: max-age=0" header if the corresponding
    // load flag is already specified. FrameLoader sets both the flag and
    // the extra header -- the extra header is redundant since our network
    // implementation will add the necessary headers based on load flags.
    // See http://code.google.com/p/chromium/issues/detail?id=3434.
    if ((load_flags_ & net::LOAD_VALIDATE_CACHE) &&
        LowerCaseEqualsASCII(name_utf8, "cache-control") &&
        LowerCaseEqualsASCII(value_utf8, "max-age=0"))
      return;

    if (LowerCaseEqualsASCII(name_utf8, "accept"))
      has_accept_header_ = true;

    if (!buffer_.empty())
      buffer_.append("\r\n");
    buffer_.append(name_utf8 + ": " + value_utf8);
  }

  const std::string& GetBuffer() {
    // In some cases, WebKit doesn't add an Accept header, but not having the
    // header confuses some web servers.  See bug 808613.
    if (!has_accept_header_) {
      if (!buffer_.empty())
        buffer_.append("\r\n");
      buffer_.append("Accept: */*");
      has_accept_header_ = true;
    }
    return buffer_;
  }

 private:
  int load_flags_;
  std::string buffer_;
  bool has_accept_header_;
};

// Extracts the information from a data: url.
bool GetInfoFromDataURL(const GURL& url,
                        ResourceResponseInfo* info,
                        std::string* data,
                        int* error_code) {
  std::string mime_type;
  std::string charset;
  if (net::DataURL::Parse(url, &mime_type, &charset, data)) {
    *error_code = net::OK;
    // Assure same time for all time fields of data: URLs.
    Time now = Time::Now();
    info->load_timing.base_time = now;
    info->load_timing.base_ticks = TimeTicks::Now();
    info->request_time = now;
    info->response_time = now;
    info->headers = NULL;
    info->mime_type.swap(mime_type);
    info->charset.swap(charset);
    info->security_info.clear();
    info->content_length = data->length();
    info->encoded_data_length = 0;

    return true;
  }

  *error_code = net::ERR_INVALID_URL;
  return false;
}

typedef ResourceDevToolsInfo::HeadersVector HeadersVector;

void PopulateURLResponse(
    const GURL& url,
    const ResourceResponseInfo& info,
    WebURLResponse* response) {
  response->setURL(url);
  response->setResponseTime(info.response_time.ToDoubleT());
  response->setMIMEType(WebString::fromUTF8(info.mime_type));
  response->setTextEncodingName(WebString::fromUTF8(info.charset));
  response->setExpectedContentLength(info.content_length);
  response->setSecurityInfo(info.security_info);
  response->setAppCacheID(info.appcache_id);
  response->setAppCacheManifestURL(info.appcache_manifest_url);
  response->setWasCached(!info.load_timing.base_time.is_null() &&
      info.response_time < info.load_timing.base_time);
  response->setWasFetchedViaSPDY(info.was_fetched_via_spdy);
  response->setWasNpnNegotiated(info.was_npn_negotiated);
  response->setWasAlternateProtocolAvailable(
      info.was_alternate_protocol_available);
  response->setWasFetchedViaProxy(info.was_fetched_via_proxy);
  response->setRemoteIPAddress(
      WebString::fromUTF8(info.socket_address.host()));
  response->setRemotePort(info.socket_address.port());
  response->setConnectionID(info.connection_id);
  response->setConnectionReused(info.connection_reused);
  response->setDownloadFilePath(
      webkit_base::FilePathToWebString(info.download_file_path));
  response->setExtraData(new WebURLResponseExtraDataImpl(
      info.npn_negotiated_protocol));

  const ResourceLoadTimingInfo& timing_info = info.load_timing;
  if (!timing_info.base_time.is_null()) {
    WebURLLoadTiming timing;
    timing.initialize();
    timing.setRequestTime((timing_info.base_ticks - TimeTicks()).InSecondsF());
    timing.setProxyStart(timing_info.proxy_start);
    timing.setProxyEnd(timing_info.proxy_end);
    timing.setDNSStart(timing_info.dns_start);
    timing.setDNSEnd(timing_info.dns_end);
    timing.setConnectStart(timing_info.connect_start);
    timing.setConnectEnd(timing_info.connect_end);
    timing.setSSLStart(timing_info.ssl_start);
    timing.setSSLEnd(timing_info.ssl_end);
    timing.setSendStart(timing_info.send_start);
    timing.setSendEnd(timing_info.send_end);
    timing.setReceiveHeadersEnd(timing_info.receive_headers_end);
    response->setLoadTiming(timing);
  }

  if (info.devtools_info.get()) {
    WebHTTPLoadInfo load_info;

    load_info.setHTTPStatusCode(info.devtools_info->http_status_code);
    load_info.setHTTPStatusText(WebString::fromUTF8(
        info.devtools_info->http_status_text));
    load_info.setEncodedDataLength(info.encoded_data_length);

    load_info.setRequestHeadersText(WebString::fromUTF8(
        info.devtools_info->request_headers_text));
    load_info.setResponseHeadersText(WebString::fromUTF8(
        info.devtools_info->response_headers_text));
    const HeadersVector& request_headers = info.devtools_info->request_headers;
    for (HeadersVector::const_iterator it = request_headers.begin();
         it != request_headers.end(); ++it) {
      load_info.addRequestHeader(WebString::fromUTF8(it->first),
          WebString::fromUTF8(it->second));
    }
    const HeadersVector& response_headers =
        info.devtools_info->response_headers;
    for (HeadersVector::const_iterator it = response_headers.begin();
         it != response_headers.end(); ++it) {
      load_info.addResponseHeader(WebString::fromUTF8(it->first),
          WebString::fromUTF8(it->second));
    }
    response->setHTTPLoadInfo(load_info);
  }

  const net::HttpResponseHeaders* headers = info.headers;
  if (!headers)
    return;

  WebURLResponse::HTTPVersion version = WebURLResponse::Unknown;
  if (headers->GetHttpVersion() == net::HttpVersion(0, 9))
    version = WebURLResponse::HTTP_0_9;
  else if (headers->GetHttpVersion() == net::HttpVersion(1, 0))
    version = WebURLResponse::HTTP_1_0;
  else if (headers->GetHttpVersion() == net::HttpVersion(1, 1))
    version = WebURLResponse::HTTP_1_1;
  response->setHTTPVersion(version);
  response->setHTTPStatusCode(headers->response_code());
  response->setHTTPStatusText(WebString::fromUTF8(headers->GetStatusText()));

  // TODO(darin): We should leverage HttpResponseHeaders for this, and this
  // should be using the same code as ResourceDispatcherHost.
  // TODO(jungshik): Figure out the actual value of the referrer charset and
  // pass it to GetSuggestedFilename.
  std::string value;
  headers->EnumerateHeader(NULL, "content-disposition", &value);
  response->setSuggestedFileName(
      net::GetSuggestedFilename(url,
                                value,
                                std::string(),  // referrer_charset
                                std::string(),  // suggested_name
                                std::string(),  // mime_type
                                std::string()));  // default_name

  Time time_val;
  if (headers->GetLastModifiedValue(&time_val))
    response->setLastModifiedDate(time_val.ToDoubleT());

  // Build up the header map.
  void* iter = NULL;
  std::string name;
  while (headers->EnumerateHeaderLines(&iter, &name, &value)) {
    response->addHTTPHeaderField(WebString::fromUTF8(name),
                                 WebString::fromUTF8(value));
  }
}

}  // namespace

// WebURLLoaderImpl::Context --------------------------------------------------

// This inner class exists since the WebURLLoader may be deleted while inside a
// call to WebURLLoaderClient.  The bridge requires its Peer to stay alive
// until it receives OnCompletedRequest.
class WebURLLoaderImpl::Context : public base::RefCounted<Context>,
                                  public ResourceLoaderBridge::Peer {
 public:
  explicit Context(WebURLLoaderImpl* loader);

  WebURLLoaderClient* client() const { return client_; }
  void set_client(WebURLLoaderClient* client) { client_ = client; }

  void Cancel();
  void SetDefersLoading(bool value);
  void Start(
      const WebURLRequest& request,
      ResourceLoaderBridge::SyncLoadResponse* sync_load_response,
      WebKitPlatformSupportImpl* platform);

  // ResourceLoaderBridge::Peer methods:
  virtual void OnUploadProgress(uint64 position, uint64 size) OVERRIDE;
  virtual bool OnReceivedRedirect(
      const GURL& new_url,
      const ResourceResponseInfo& info,
      bool* has_new_first_party_for_cookies,
      GURL* new_first_party_for_cookies) OVERRIDE;
  virtual void OnReceivedResponse(const ResourceResponseInfo& info) OVERRIDE;
  virtual void OnDownloadedData(int len) OVERRIDE;
  virtual void OnReceivedData(const char* data,
                              int data_length,
                              int encoded_data_length) OVERRIDE;
  virtual void OnReceivedCachedMetadata(const char* data, int len) OVERRIDE;
  virtual void OnCompletedRequest(
      int error_code,
      bool was_ignored_by_handler,
      const std::string& security_info,
      const base::TimeTicks& completion_time) OVERRIDE;

 private:
  friend class base::RefCounted<Context>;
  virtual ~Context() {}

  // We can optimize the handling of data URLs in most cases.
  bool CanHandleDataURL(const GURL& url) const;
  void HandleDataURL();

  WebURLLoaderImpl* loader_;
  WebURLRequest request_;
  WebURLLoaderClient* client_;
  WebReferrerPolicy referrer_policy_;
  scoped_ptr<ResourceLoaderBridge> bridge_;
  scoped_ptr<FtpDirectoryListingResponseDelegate> ftp_listing_delegate_;
  scoped_ptr<MultipartResponseDelegate> multipart_delegate_;
  scoped_ptr<ResourceLoaderBridge> completed_bridge_;
};

WebURLLoaderImpl::Context::Context(WebURLLoaderImpl* loader)
    : loader_(loader),
      client_(NULL),
      referrer_policy_(WebKit::WebReferrerPolicyDefault) {
}

void WebURLLoaderImpl::Context::Cancel() {
  // The bridge will still send OnCompletedRequest, which will Release() us, so
  // we don't do that here.
  if (bridge_.get())
    bridge_->Cancel();

  // Ensure that we do not notify the multipart delegate anymore as it has
  // its own pointer to the client.
  if (multipart_delegate_.get())
    multipart_delegate_->Cancel();

  // Do not make any further calls to the client.
  client_ = NULL;
  loader_ = NULL;
}

void WebURLLoaderImpl::Context::SetDefersLoading(bool value) {
  if (bridge_.get())
    bridge_->SetDefersLoading(value);
}

void WebURLLoaderImpl::Context::Start(
    const WebURLRequest& request,
    ResourceLoaderBridge::SyncLoadResponse* sync_load_response,
    WebKitPlatformSupportImpl* platform) {
  DCHECK(!bridge_.get());

  request_ = request;  // Save the request.

  GURL url = request.url();
  if (url.SchemeIs("data") && CanHandleDataURL(url)) {
    if (sync_load_response) {
      // This is a sync load. Do the work now.
      sync_load_response->url = url;
      std::string data;
      GetInfoFromDataURL(sync_load_response->url, sync_load_response,
                         &sync_load_response->data,
                         &sync_load_response->error_code);
    } else {
      AddRef();  // Balanced in OnCompletedRequest
      MessageLoop::current()->PostTask(FROM_HERE,
          base::Bind(&Context::HandleDataURL, this));
    }
    return;
  }

  GURL referrer_url(
      request.httpHeaderField(WebString::fromUTF8("Referer")).utf8());
  const std::string& method = request.httpMethod().utf8();

  int load_flags = net::LOAD_NORMAL;
  switch (request.cachePolicy()) {
    case WebURLRequest::ReloadIgnoringCacheData:
      // Required by LayoutTests/http/tests/misc/refresh-headers.php
      load_flags |= net::LOAD_VALIDATE_CACHE;
      break;
    case WebURLRequest::ReturnCacheDataElseLoad:
      load_flags |= net::LOAD_PREFERRING_CACHE;
      break;
    case WebURLRequest::ReturnCacheDataDontLoad:
      load_flags |= net::LOAD_ONLY_FROM_CACHE;
      break;
    case WebURLRequest::UseProtocolCachePolicy:
      break;
  }

  if (request.reportUploadProgress())
    load_flags |= net::LOAD_ENABLE_UPLOAD_PROGRESS;
  if (request.reportLoadTiming())
    load_flags |= net::LOAD_ENABLE_LOAD_TIMING;
  if (request.reportRawHeaders())
    load_flags |= net::LOAD_REPORT_RAW_HEADERS;

  if (!request.allowCookies() || !request.allowStoredCredentials()) {
    load_flags |= net::LOAD_DO_NOT_SAVE_COOKIES;
    load_flags |= net::LOAD_DO_NOT_SEND_COOKIES;
  }

  if (!request.allowStoredCredentials())
    load_flags |= net::LOAD_DO_NOT_SEND_AUTH_DATA;

  HeaderFlattener flattener(load_flags);
  request.visitHTTPHeaderFields(&flattener);

  // TODO(brettw) this should take parameter encoding into account when
  // creating the GURLs.

  ResourceLoaderBridge::RequestInfo request_info;
  request_info.method = method;
  request_info.url = url;
  request_info.first_party_for_cookies = request.firstPartyForCookies();
  request_info.referrer = referrer_url;
  request_info.headers = flattener.GetBuffer();
  request_info.load_flags = load_flags;
  // requestor_pid only needs to be non-zero if the request originates outside
  // the render process, so we can use requestorProcessID even for requests
  // from in-process plugins.
  request_info.requestor_pid = request.requestorProcessID();
  request_info.request_type =
      ResourceType::FromTargetType(request.targetType());
  request_info.priority = request.priority();
  request_info.appcache_host_id = request.appCacheHostID();
  request_info.routing_id = request.requestorID();
  request_info.download_to_file = request.downloadToFile();
  request_info.has_user_gesture = request.hasUserGesture();
  request_info.extra_data = request.extraData();
  if (request.extraData()) {
    referrer_policy_ = static_cast<WebURLRequestExtraDataImpl*>(
        request.extraData())->referrer_policy();
    request_info.referrer_policy = referrer_policy_;
  }
  bridge_.reset(platform->CreateResourceLoader(request_info));

  if (!request.httpBody().isNull()) {
    // GET and HEAD requests shouldn't have http bodies.
    DCHECK(method != "GET" && method != "HEAD");
    const WebHTTPBody& httpBody = request.httpBody();
    size_t i = 0;
    WebHTTPBody::Element element;
    scoped_refptr<ResourceRequestBody> request_body = new ResourceRequestBody;
    while (httpBody.elementAt(i++, element)) {
      switch (element.type) {
        case WebHTTPBody::Element::TypeData:
          if (!element.data.isEmpty()) {
            // WebKit sometimes gives up empty data to append. These aren't
            // necessary so we just optimize those out here.
            request_body->AppendBytes(
                element.data.data(), static_cast<int>(element.data.size()));
          }
          break;
        case WebHTTPBody::Element::TypeFile:
          if (element.fileLength == -1) {
            request_body->AppendFileRange(
                webkit_base::WebStringToFilePath(element.filePath),
                0, kuint64max, base::Time());
          } else {
            request_body->AppendFileRange(
                webkit_base::WebStringToFilePath(element.filePath),
                static_cast<uint64>(element.fileStart),
                static_cast<uint64>(element.fileLength),
                base::Time::FromDoubleT(element.modificationTime));
          }
          break;
        case WebHTTPBody::Element::TypeURL: {
          GURL url = GURL(element.url);
          DCHECK(url.SchemeIsFileSystem());
          request_body->AppendFileSystemFileRange(
              url,
              static_cast<uint64>(element.fileStart),
              static_cast<uint64>(element.fileLength),
              base::Time::FromDoubleT(element.modificationTime));
          break;
        }
        case WebHTTPBody::Element::TypeBlob:
          request_body->AppendBlob(GURL(element.blobURL));
          break;
        default:
          NOTREACHED();
      }
    }
    request_body->set_identifier(request.httpBody().identifier());
    bridge_->SetRequestBody(request_body);
  }

  if (sync_load_response) {
    bridge_->SyncLoad(sync_load_response);
    return;
  }

  if (bridge_->Start(this)) {
    AddRef();  // Balanced in OnCompletedRequest
  } else {
    bridge_.reset();
  }
}

void WebURLLoaderImpl::Context::OnUploadProgress(uint64 position, uint64 size) {
  if (client_)
    client_->didSendData(loader_, position, size);
}

bool WebURLLoaderImpl::Context::OnReceivedRedirect(
    const GURL& new_url,
    const ResourceResponseInfo& info,
    bool* has_new_first_party_for_cookies,
    GURL* new_first_party_for_cookies) {
  if (!client_)
    return false;

  WebURLResponse response;
  response.initialize();
  PopulateURLResponse(request_.url(), info, &response);

  // TODO(darin): We lack sufficient information to construct the actual
  // request that resulted from the redirect.
  WebURLRequest new_request(new_url);
  new_request.setFirstPartyForCookies(request_.firstPartyForCookies());
  new_request.setDownloadToFile(request_.downloadToFile());

  WebString referrer_string = WebString::fromUTF8("Referer");
  WebString referrer = WebSecurityPolicy::generateReferrerHeader(
      referrer_policy_,
      new_url,
      request_.httpHeaderField(referrer_string));
  if (!referrer.isEmpty())
    new_request.setHTTPHeaderField(referrer_string, referrer);

  if (response.httpStatusCode() == 307)
    new_request.setHTTPMethod(request_.httpMethod());

  client_->willSendRequest(loader_, new_request, response);
  request_ = new_request;
  *has_new_first_party_for_cookies = true;
  *new_first_party_for_cookies = request_.firstPartyForCookies();

  // Only follow the redirect if WebKit left the URL unmodified.
  if (new_url == GURL(new_request.url()))
    return true;

  // We assume that WebKit only changes the URL to suppress a redirect, and we
  // assume that it does so by setting it to be invalid.
  DCHECK(!new_request.url().isValid());
  return false;
}

void WebURLLoaderImpl::Context::OnReceivedResponse(
    const ResourceResponseInfo& info) {
  if (!client_)
    return;

  WebURLResponse response;
  response.initialize();
  PopulateURLResponse(request_.url(), info, &response);

  bool show_raw_listing = (GURL(request_.url()).query() == "raw");

  if (info.mime_type == "text/vnd.chromium.ftp-dir") {
    if (show_raw_listing) {
      // Set the MIME type to plain text to prevent any active content.
      response.setMIMEType("text/plain");
    } else {
      // We're going to produce a parsed listing in HTML.
      response.setMIMEType("text/html");
    }
  }

  client_->didReceiveResponse(loader_, response);

  // We may have been cancelled after didReceiveResponse, which would leave us
  // without a client and therefore without much need to do further handling.
  if (!client_)
    return;

  DCHECK(!ftp_listing_delegate_.get());
  DCHECK(!multipart_delegate_.get());
  if (info.headers && info.mime_type == "multipart/x-mixed-replace") {
    std::string content_type;
    info.headers->EnumerateHeader(NULL, "content-type", &content_type);

    std::string mime_type;
    std::string charset;
    bool had_charset = false;
    std::string boundary;
    net::HttpUtil::ParseContentType(content_type, &mime_type, &charset,
                                    &had_charset, &boundary);
    TrimString(boundary, " \"", &boundary);

    // If there's no boundary, just handle the request normally.  In the gecko
    // code, nsMultiMixedConv::OnStartRequest throws an exception.
    if (!boundary.empty()) {
      multipart_delegate_.reset(
          new MultipartResponseDelegate(client_, loader_, response, boundary));
    }
  } else if (info.mime_type == "text/vnd.chromium.ftp-dir" &&
             !show_raw_listing) {
    ftp_listing_delegate_.reset(
        new FtpDirectoryListingResponseDelegate(client_, loader_, response));
  }
}

void WebURLLoaderImpl::Context::OnDownloadedData(int len) {
  if (client_)
    client_->didDownloadData(loader_, len);
}

void WebURLLoaderImpl::Context::OnReceivedData(const char* data,
                                               int data_length,
                                               int encoded_data_length) {
  if (!client_)
    return;

  if (ftp_listing_delegate_.get()) {
    // The FTP listing delegate will make the appropriate calls to
    // client_->didReceiveData and client_->didReceiveResponse.
    ftp_listing_delegate_->OnReceivedData(data, data_length);
  } else if (multipart_delegate_.get()) {
    // The multipart delegate will make the appropriate calls to
    // client_->didReceiveData and client_->didReceiveResponse.
    multipart_delegate_->OnReceivedData(data, data_length, encoded_data_length);
  } else {
    client_->didReceiveData(loader_, data, data_length, encoded_data_length);
  }
}

void WebURLLoaderImpl::Context::OnReceivedCachedMetadata(
    const char* data, int len) {
  if (client_)
    client_->didReceiveCachedMetadata(loader_, data, len);
}

void WebURLLoaderImpl::Context::OnCompletedRequest(
    int error_code,
    bool was_ignored_by_handler,
    const std::string& security_info,
    const base::TimeTicks& completion_time) {
  if (ftp_listing_delegate_.get()) {
    ftp_listing_delegate_->OnCompletedRequest();
    ftp_listing_delegate_.reset(NULL);
  } else if (multipart_delegate_.get()) {
    multipart_delegate_->OnCompletedRequest();
    multipart_delegate_.reset(NULL);
  }

  // Prevent any further IPC to the browser now that we're complete, but
  // don't delete it to keep any downloaded temp files alive.
  DCHECK(!completed_bridge_.get());
  completed_bridge_.swap(bridge_);

  if (client_) {
    if (error_code != net::OK) {
      WebURLError error;
      if (error_code == net::ERR_ABORTED) {
        error.isCancellation = true;
      } else if (error_code == net::ERR_TEMPORARILY_THROTTLED) {
        error.localizedDescription = WebString::fromUTF8(
            kThrottledErrorDescription);
      }
      error.domain = WebString::fromUTF8(net::kErrorDomain);
      error.reason = error_code;
      error.unreachableURL = request_.url();
      client_->didFail(loader_, error);
    } else {
      client_->didFinishLoading(
          loader_, (completion_time - TimeTicks()).InSecondsF());
    }
  }

  // We are done with the bridge now, and so we need to release the reference
  // to ourselves that we took on behalf of the bridge.  This may cause our
  // destruction.
  Release();
}

bool WebURLLoaderImpl::Context::CanHandleDataURL(const GURL& url) const {
  DCHECK(url.SchemeIs("data"));

  // Optimize for the case where we can handle a data URL locally.  We must
  // skip this for data URLs targetted at frames since those could trigger a
  // download.
  //
  // NOTE: We special case MIME types we can render both for performance
  // reasons as well as to support unit tests, which do not have an underlying
  // ResourceLoaderBridge implementation.

#if defined(OS_ANDROID)
  // For compatibility reasons on Android we need to expose top-level data://
  // to the browser.
  if (request_.targetType() == WebURLRequest::TargetIsMainFrame)
    return false;
#endif

  if (request_.targetType() != WebURLRequest::TargetIsMainFrame &&
      request_.targetType() != WebURLRequest::TargetIsSubframe)
    return true;

  std::string mime_type, unused_charset;
  if (net::DataURL::Parse(url, &mime_type, &unused_charset, NULL) &&
      net::IsSupportedMimeType(mime_type))
    return true;

  return false;
}

void WebURLLoaderImpl::Context::HandleDataURL() {
  ResourceResponseInfo info;
  int error_code;
  std::string data;

  if (GetInfoFromDataURL(request_.url(), &info, &data, &error_code)) {
    OnReceivedResponse(info);
    if (!data.empty())
      OnReceivedData(data.data(), data.size(), 0);
  }

  OnCompletedRequest(error_code, false, info.security_info,
                     base::TimeTicks::Now());
}

// WebURLLoaderImpl -----------------------------------------------------------

WebURLLoaderImpl::WebURLLoaderImpl(WebKitPlatformSupportImpl* platform)
    : ALLOW_THIS_IN_INITIALIZER_LIST(context_(new Context(this))),
      platform_(platform) {
}

WebURLLoaderImpl::~WebURLLoaderImpl() {
  cancel();
}

void WebURLLoaderImpl::loadSynchronously(const WebURLRequest& request,
                                         WebURLResponse& response,
                                         WebURLError& error,
                                         WebData& data) {
  ResourceLoaderBridge::SyncLoadResponse sync_load_response;
  context_->Start(request, &sync_load_response, platform_);

  const GURL& final_url = sync_load_response.url;

  // TODO(tc): For file loads, we may want to include a more descriptive
  // status code or status text.
  int error_code = sync_load_response.error_code;
  if (error_code != net::OK) {
    response.setURL(final_url);
    error.domain = WebString::fromUTF8(net::kErrorDomain);
    error.reason = error_code;
    error.unreachableURL = final_url;
    return;
  }

  PopulateURLResponse(final_url, sync_load_response, &response);

  data.assign(sync_load_response.data.data(),
              sync_load_response.data.size());
}

void WebURLLoaderImpl::loadAsynchronously(const WebURLRequest& request,
                                          WebURLLoaderClient* client) {
  DCHECK(!context_->client());

  context_->set_client(client);
  context_->Start(request, NULL, platform_);
}

void WebURLLoaderImpl::cancel() {
  context_->Cancel();
}

void WebURLLoaderImpl::setDefersLoading(bool value) {
  context_->SetDefersLoading(value);
}

}  // namespace webkit_glue
