// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// The intent of this file is to provide a type-neutral abstraction between
// Chrome and WebKit for resource loading. This pure-virtual interface is
// implemented by the embedder.
//
// One of these objects will be created by WebKit for each request. WebKit
// will own the pointer to the bridge, and will delete it when the request is
// no longer needed.
//
// In turn, the bridge's owner on the WebKit end will implement the Peer
// interface, which we will use to communicate notifications back.

#ifndef WEBKIT_GLUE_RESOURCE_LOADER_BRIDGE_H_
#define WEBKIT_GLUE_RESOURCE_LOADER_BRIDGE_H_

#include <utility>
#include <vector>

#include "build/build_config.h"
#if defined(OS_POSIX)
#include "base/file_descriptor_posix.h"
#endif
#include "base/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/platform_file.h"
#include "base/time.h"
#include "base/values.h"
#include "googleurl/src/gurl.h"
#include "net/base/host_port_pair.h"
#include "net/url_request/url_request_status.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebReferrerPolicy.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebURLRequest.h"
#include "webkit/glue/resource_type.h"
#include "webkit/glue/webkit_glue_export.h"

namespace net {
class HttpResponseHeaders;
}

namespace webkit_glue {
class ResourceRequestBody;

// Structure containing timing information for the request. It addresses
// http://groups.google.com/group/http-archive-specification/web/har-1-1-spec
// and http://dev.w3.org/2006/webapi/WebTiming/ needs.
//
// All the values for starts and ends are given in milliseconds and are
// offsets with respect to the given base time.
struct ResourceLoadTimingInfo {
  WEBKIT_GLUE_EXPORT ResourceLoadTimingInfo();
  WEBKIT_GLUE_EXPORT ~ResourceLoadTimingInfo();

  // All the values in this struct are given as offsets in ticks wrt
  // this base tick count.
  base::TimeTicks base_ticks;

  // The value of Time::Now() when base_ticks was set.
  base::Time base_time;

  // The time that proxy processing started. For requests with no proxy phase,
  // this time is -1.
  int32 proxy_start;

  // The time that proxy processing ended. For reused sockets this time
  // is -1.
  int32 proxy_end;

  // The time that DNS lookup started. For reused sockets this time is -1.
  int32 dns_start;

  // The time that DNS lookup ended. For reused sockets this time is -1.
  int32 dns_end;

  // The time that establishing connection started. Connect time includes
  // DNS, blocking, TCP, TCP retries and SSL time.
  int32 connect_start;

  // The time that establishing connection ended. Connect time includes
  // DNS, blocking, TCP, TCP retries and SSL time.
  int32 connect_end;

  // The time at which SSL handshake started. For non-HTTPS requests this
  // is 0.
  int32 ssl_start;

  // The time at which SSL handshake ended. For non-HTTPS requests this is 0.
  int32 ssl_end;

  // The time that HTTP request started. For non-HTTP requests this is 0.
  int32 send_start;

  // The time that HTTP request ended. For non-HTTP requests this is 0.
  int32 send_end;

  // The time at which receiving HTTP headers started. For non-HTTP requests
  // this is 0.
  int32 receive_headers_start;

  // The time at which receiving HTTP headers ended. For non-HTTP requests
  // this is 0.
  int32 receive_headers_end;
};

struct ResourceDevToolsInfo : base::RefCounted<ResourceDevToolsInfo> {
  typedef std::vector<std::pair<std::string, std::string> >
      HeadersVector;

  WEBKIT_GLUE_EXPORT ResourceDevToolsInfo();

  int32 http_status_code;
  std::string http_status_text;
  HeadersVector request_headers;
  HeadersVector response_headers;
  std::string request_headers_text;
  std::string response_headers_text;

 private:
  friend class base::RefCounted<ResourceDevToolsInfo>;
  WEBKIT_GLUE_EXPORT ~ResourceDevToolsInfo();
};

struct ResourceResponseInfo {
  WEBKIT_GLUE_EXPORT ResourceResponseInfo();
  WEBKIT_GLUE_EXPORT ~ResourceResponseInfo();

  // The time at which the request was made that resulted in this response.
  // For cached responses, this time could be "far" in the past.
  base::Time request_time;

  // The time at which the response headers were received.  For cached
  // responses, this time could be "far" in the past.
  base::Time response_time;

  // The response headers or NULL if the URL type does not support headers.
  scoped_refptr<net::HttpResponseHeaders> headers;

  // The mime type of the response.  This may be a derived value.
  std::string mime_type;

  // The character encoding of the response or none if not applicable to the
  // response's mime type.  This may be a derived value.
  std::string charset;

  // An opaque string carrying security information pertaining to this
  // response.  This may include information about the SSL connection used.
  std::string security_info;

  // Content length if available. -1 if not available
  int64 content_length;

  // Length of the encoded data transferred over the network. In case there is
  // no data, contains -1.
  int64 encoded_data_length;

  // The appcache this response was loaded from, or kNoCacheId.
  int64 appcache_id;

  // The manifest url of the appcache this response was loaded from.
  // Note: this value is only populated for main resource requests.
  GURL appcache_manifest_url;

  // Connection identifier from the underlying network stack. In case there
  // is no associated connection, contains 0.
  uint32 connection_id;

  // Determines whether physical connection reused.
  bool connection_reused;

  // Detailed timing information used by the WebTiming, HAR and Developer
  // Tools.
  ResourceLoadTimingInfo load_timing;

  // Actual request and response headers, as obtained from the network stack.
  // Only present if request had LOAD_REPORT_RAW_HEADERS in load_flags, and
  // requesting renderer had CanReadRowCookies permission.
  scoped_refptr<ResourceDevToolsInfo> devtools_info;

  // The path to a file that will contain the response body.  It may only
  // contain a portion of the response body at the time that the ResponseInfo
  // becomes available.
  base::FilePath download_file_path;

  // True if the response was delivered using SPDY.
  bool was_fetched_via_spdy;

  // True if the response was delivered after NPN is negotiated.
  bool was_npn_negotiated;

  // True if response could use alternate protocol. However, browser will
  // ignore the alternate protocol when spdy is not enabled on browser side.
  bool was_alternate_protocol_available;

  // True if the response was fetched via an explicit proxy (as opposed to a
  // transparent proxy). The proxy could be any type of proxy, HTTP or SOCKS.
  // Note: we cannot tell if a transparent proxy may have been involved.
  bool was_fetched_via_proxy;

  // NPN protocol negotiated with the server.
  std::string npn_negotiated_protocol;

  // Remote address of the socket which fetched this resource.
  net::HostPortPair socket_address;
};

class ResourceLoaderBridge {
 public:
  // Structure used when calling
  // WebKitPlatformSupportImpl::CreateResourceLoader().
  struct WEBKIT_GLUE_EXPORT RequestInfo {
    RequestInfo();
    ~RequestInfo();

    // HTTP-style method name (e.g., "GET" or "POST").
    std::string method;

    // Absolute URL encoded in ASCII per the rules of RFC-2396.
    GURL url;

    // URL of the document in the top-level window, which may be checked by the
    // third-party cookie blocking policy.
    GURL first_party_for_cookies;

    // Optional parameter, a URL with similar constraints in how it must be
    // encoded as the url member.
    GURL referrer;

    // The referrer policy that applies to the referrer.
    WebKit::WebReferrerPolicy referrer_policy;

    // For HTTP(S) requests, the headers parameter can be a \r\n-delimited and
    // \r\n-terminated list of MIME headers.  They should be ASCII-encoded using
    // the standard MIME header encoding rules.  The headers parameter can also
    // be null if no extra request headers need to be set.
    std::string headers;

    // Composed of the values defined in url_request_load_flags.h.
    int load_flags;

    // Process id of the process making the request.
    int requestor_pid;

    // Indicates if the current request is the main frame load, a sub-frame
    // load, or a sub objects load.
    ResourceType::Type request_type;

    // Indicates the priority of this request, as determined by WebKit.
    WebKit::WebURLRequest::Priority priority;

    // Used for plugin to browser requests.
    uint32 request_context;

    // Identifies what appcache host this request is associated with.
    int appcache_host_id;

    // Used to associated the bridge with a frame's network context.
    int routing_id;

    // If true, then the response body will be downloaded to a file and the
    // path to that file will be provided in ResponseInfo::download_file_path.
    bool download_to_file;

    // True if the request was user initiated.
    bool has_user_gesture;

    // Extra data associated with this request.  We do not own this pointer.
    WebKit::WebURLRequest::ExtraData* extra_data;

   private:
    DISALLOW_COPY_AND_ASSIGN(RequestInfo);
  };

  // See the SyncLoad method declared below.  (The name of this struct is not
  // suffixed with "Info" because it also contains the response data.)
  struct SyncLoadResponse : ResourceResponseInfo {
    SyncLoadResponse();
    ~SyncLoadResponse();

    // The response error code.
    int error_code;

    // The final URL of the response.  This may differ from the request URL in
    // the case of a server redirect.
    GURL url;

    // The response data.
    std::string data;
  };

  // Generated by the bridge. This is implemented by our custom resource loader
  // within webkit. The Peer and it's bridge should have identical lifetimes
  // as they represent each end of a communication channel.
  //
  // These callbacks mirror net::URLRequest::Delegate and the order and
  // conditions in which they will be called are identical. See url_request.h
  // for more information.
  class Peer {
   public:
    // Called as upload progress is made.
    // note: only for requests with LOAD_ENABLE_UPLOAD_PROGRESS set
    virtual void OnUploadProgress(uint64 position, uint64 size) = 0;

    // Called when a redirect occurs.  The implementation may return false to
    // suppress the redirect.  The given ResponseInfo provides complete
    // information about the redirect, and new_url is the URL that will be
    // loaded if this method returns true.  If this method returns true, the
    // output parameter *has_new_first_party_for_cookies indicates whether the
    // output parameter *new_first_party_for_cookies contains the new URL that
    // should be consulted for the third-party cookie blocking policy.
    virtual bool OnReceivedRedirect(const GURL& new_url,
                                    const ResourceResponseInfo& info,
                                    bool* has_new_first_party_for_cookies,
                                    GURL* new_first_party_for_cookies) = 0;

    // Called when response headers are available (after all redirects have
    // been followed).
    virtual void OnReceivedResponse(const ResourceResponseInfo& info) = 0;

    // Called when a chunk of response data is downloaded.  This method may be
    // called multiple times or not at all if an error occurs.  This method is
    // only called if RequestInfo::download_to_file was set to true, and in
    // that case, OnReceivedData will not be called.
    virtual void OnDownloadedData(int len) = 0;

    // Called when a chunk of response data is available. This method may
    // be called multiple times or not at all if an error occurs.
    // The encoded_data_length is the length of the encoded data transferred
    // over the network, which could be different from data length (e.g. for
    // gzipped content), or -1 if if unknown.
    virtual void OnReceivedData(const char* data,
                                int data_length,
                                int encoded_data_length) = 0;

    // Called when metadata generated by the renderer is retrieved from the
    // cache. This method may be called zero or one times.
    virtual void OnReceivedCachedMetadata(const char* data, int len) { }

    // Called when the response is complete.  This method signals completion of
    // the resource load.
    virtual void OnCompletedRequest(
        int error_code,
        bool was_ignored_by_handler,
        const std::string& security_info,
        const base::TimeTicks& completion_time) = 0;

   protected:
    virtual ~Peer() {}
  };

  // use WebKitPlatformSupportImpl::CreateResourceLoader() for construction, but
  // anybody can delete at any time, INCLUDING during processing of callbacks.
  WEBKIT_GLUE_EXPORT virtual ~ResourceLoaderBridge();

  // Call this method before calling Start() to set the request body.
  // May only be used with HTTP(S) POST requests.
  virtual void SetRequestBody(ResourceRequestBody* request_body) = 0;

  // Call this method to initiate the request.  If this method succeeds, then
  // the peer's methods will be called asynchronously to report various events.
  virtual bool Start(Peer* peer) = 0;

  // Call this method to cancel a request that is in progress.  This method
  // causes the request to immediately transition into the 'done' state. The
  // OnCompletedRequest method will be called asynchronously; this assumes
  // the peer is still valid.
  virtual void Cancel() = 0;

  // Call this method to suspend or resume a load that is in progress.  This
  // method may only be called after a successful call to the Start method.
  virtual void SetDefersLoading(bool value) = 0;

  // Call this method to load the resource synchronously (i.e., in one shot).
  // This is an alternative to the Start method.  Be warned that this method
  // will block the calling thread until the resource is fully downloaded or an
  // error occurs.  It could block the calling thread for a long time, so only
  // use this if you really need it!  There is also no way for the caller to
  // interrupt this method.  Errors are reported via the status field of the
  // response parameter.
  virtual void SyncLoad(SyncLoadResponse* response) = 0;

 protected:
  // Construction must go through
  // WebKitPlatformSupportImpl::CreateResourceLoader()
  // For HTTP(S) POST requests, the AppendDataToUpload and AppendFileToUpload
  // methods may be called to construct the body of the request.
  WEBKIT_GLUE_EXPORT ResourceLoaderBridge();

 private:
  DISALLOW_COPY_AND_ASSIGN(ResourceLoaderBridge);
};

}  // namespace webkit_glue

#endif  // WEBKIT_GLUE_RESOURCE_LOADER_BRIDGE_H_
