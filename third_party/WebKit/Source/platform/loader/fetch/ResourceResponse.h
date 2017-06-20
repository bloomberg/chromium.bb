/*
 * Copyright (C) 2006, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef ResourceResponse_h
#define ResourceResponse_h

#include "platform/PlatformExport.h"
#include "platform/blob/BlobData.h"
#include "platform/loader/fetch/ResourceLoadInfo.h"
#include "platform/loader/fetch/ResourceLoadTiming.h"
#include "platform/network/HTTPHeaderMap.h"
#include "platform/network/HTTPParsers.h"
#include "platform/weborigin/KURL.h"
#include "platform/wtf/RefCounted.h"
#include "platform/wtf/RefPtr.h"
#include "platform/wtf/Vector.h"
#include "platform/wtf/text/CString.h"
#include "public/platform/WebURLResponse.h"
#include "public/platform/modules/serviceworker/WebServiceWorkerResponseType.h"

namespace blink {

struct CrossThreadResourceResponseData;

class PLATFORM_EXPORT ResourceResponse final {
  DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();

 public:
  enum HTTPVersion : uint8_t {
    kHTTPVersionUnknown,
    kHTTPVersion_0_9,
    kHTTPVersion_1_0,
    kHTTPVersion_1_1,
    kHTTPVersion_2_0
  };
  enum SecurityStyle : uint8_t {
    kSecurityStyleUnknown,
    kSecurityStyleUnauthenticated,
    kSecurityStyleAuthenticationBroken,
    kSecurityStyleWarning,
    kSecurityStyleAuthenticated
  };

  class PLATFORM_EXPORT SignedCertificateTimestamp final {
   public:
    SignedCertificateTimestamp(String status,
                               String origin,
                               String log_description,
                               String log_id,
                               int64_t timestamp,
                               String hash_algorithm,
                               String signature_algorithm,
                               String signature_data)
        : status_(status),
          origin_(origin),
          log_description_(log_description),
          log_id_(log_id),
          timestamp_(timestamp),
          hash_algorithm_(hash_algorithm),
          signature_algorithm_(signature_algorithm),
          signature_data_(signature_data) {}
    explicit SignedCertificateTimestamp(
        const struct blink::WebURLResponse::SignedCertificateTimestamp&);
    SignedCertificateTimestamp IsolatedCopy() const;

    String status_;
    String origin_;
    String log_description_;
    String log_id_;
    int64_t timestamp_;
    String hash_algorithm_;
    String signature_algorithm_;
    String signature_data_;
  };

  using SignedCertificateTimestampList =
      WTF::Vector<SignedCertificateTimestamp>;

  struct SecurityDetails {
    DISALLOW_NEW();
    SecurityDetails() : valid_from(0), valid_to(0) {}
    // All strings are human-readable values.
    String protocol;
    // keyExchange is the empty string if not applicable for the connection's
    // protocol.
    String key_exchange;
    // keyExchangeGroup is the empty string if not applicable for the
    // connection's key exchange.
    String key_exchange_group;
    String cipher;
    // mac is the empty string when the connection cipher suite does not
    // have a separate MAC value (i.e. if the cipher suite is AEAD).
    String mac;
    String subject_name;
    Vector<String> san_list;
    String issuer;
    time_t valid_from;
    time_t valid_to;
    // DER-encoded X509Certificate certificate chain.
    Vector<AtomicString> certificate;
    SignedCertificateTimestampList sct_list;
  };

  class ExtraData : public RefCounted<ExtraData> {
   public:
    virtual ~ExtraData() {}
  };

  explicit ResourceResponse(CrossThreadResourceResponseData*);

  // Gets a copy of the data suitable for passing to another thread.
  std::unique_ptr<CrossThreadResourceResponseData> CopyData() const;

  ResourceResponse();
  ResourceResponse(const KURL&,
                   const AtomicString& mime_type,
                   long long expected_length,
                   const AtomicString& text_encoding_name);
  ResourceResponse(const ResourceResponse&);
  ResourceResponse& operator=(const ResourceResponse&);

  bool IsNull() const { return is_null_; }
  bool IsHTTP() const;

  // The URL of the resource. Note that if a service worker responded to the
  // request for this resource, it may have fetched an entirely different URL
  // and responded with that resource. wasFetchedViaServiceWorker() and
  // originalURLViaServiceWorker() can be used to determine whether and how a
  // service worker responded to the request. Example service worker code:
  //
  // onfetch = (event => {
  //   if (event.request.url == 'https://abc.com')
  //     event.respondWith(fetch('https://def.com'));
  // });
  //
  // If this service worker responds to an "https://abc.com" request, then for
  // the resulting ResourceResponse, url() is "https://abc.com",
  // wasFetchedViaServiceWorker() is true, and originalURLViaServiceWorker() is
  // "https://def.com".
  const KURL& Url() const;
  void SetURL(const KURL&);

  const AtomicString& MimeType() const;
  void SetMimeType(const AtomicString&);

  long long ExpectedContentLength() const;
  void SetExpectedContentLength(long long);

  const AtomicString& TextEncodingName() const;
  void SetTextEncodingName(const AtomicString&);

  int HttpStatusCode() const;
  void SetHTTPStatusCode(int);

  const AtomicString& HttpStatusText() const;
  void SetHTTPStatusText(const AtomicString&);

  const AtomicString& HttpHeaderField(const AtomicString& name) const;
  void SetHTTPHeaderField(const AtomicString& name, const AtomicString& value);
  void AddHTTPHeaderField(const AtomicString& name, const AtomicString& value);
  void ClearHTTPHeaderField(const AtomicString& name);
  const HTTPHeaderMap& HttpHeaderFields() const;

  bool IsMultipart() const { return MimeType() == "multipart/x-mixed-replace"; }

  bool IsAttachment() const;

  AtomicString HttpContentType() const;

  // These functions return parsed values of the corresponding response headers.
  // NaN means that the header was not present or had invalid value.
  bool CacheControlContainsNoCache() const;
  bool CacheControlContainsNoStore() const;
  bool CacheControlContainsMustRevalidate() const;
  bool HasCacheValidatorFields() const;
  double CacheControlMaxAge() const;
  double Date() const;
  double Age() const;
  double Expires() const;
  double LastModified() const;

  unsigned ConnectionID() const;
  void SetConnectionID(unsigned);

  bool ConnectionReused() const;
  void SetConnectionReused(bool);

  bool WasCached() const;
  void SetWasCached(bool);

  ResourceLoadTiming* GetResourceLoadTiming() const;
  void SetResourceLoadTiming(RefPtr<ResourceLoadTiming>);

  RefPtr<ResourceLoadInfo> GetResourceLoadInfo() const;
  void SetResourceLoadInfo(RefPtr<ResourceLoadInfo>);

  HTTPVersion HttpVersion() const { return http_version_; }
  void SetHTTPVersion(HTTPVersion version) { http_version_ = version; }

  bool HasMajorCertificateErrors() const {
    return has_major_certificate_errors_;
  }
  void SetHasMajorCertificateErrors(bool has_major_certificate_errors) {
    has_major_certificate_errors_ = has_major_certificate_errors;
  }

  SecurityStyle GetSecurityStyle() const { return security_style_; }
  void SetSecurityStyle(SecurityStyle security_style) {
    security_style_ = security_style;
  }

  const SecurityDetails* GetSecurityDetails() const {
    return &security_details_;
  }
  void SetSecurityDetails(const String& protocol,
                          const String& key_exchange,
                          const String& key_exchange_group,
                          const String& cipher,
                          const String& mac,
                          const String& subject_name,
                          const Vector<String>& san_list,
                          const String& issuer,
                          time_t valid_from,
                          time_t valid_to,
                          const Vector<AtomicString>& certificate,
                          const SignedCertificateTimestampList& sct_list);

  long long AppCacheID() const { return app_cache_id_; }
  void SetAppCacheID(long long id) { app_cache_id_ = id; }

  const KURL& AppCacheManifestURL() const { return app_cache_manifest_url_; }
  void SetAppCacheManifestURL(const KURL& url) {
    app_cache_manifest_url_ = url;
  }

  bool WasFetchedViaSPDY() const { return was_fetched_via_spdy_; }
  void SetWasFetchedViaSPDY(bool value) { was_fetched_via_spdy_ = value; }

  // See ServiceWorkerResponseInfo::was_fetched_via_service_worker.
  bool WasFetchedViaServiceWorker() const {
    return was_fetched_via_service_worker_;
  }
  void SetWasFetchedViaServiceWorker(bool value) {
    was_fetched_via_service_worker_ = value;
  }

  bool WasFetchedViaForeignFetch() const {
    return was_fetched_via_foreign_fetch_;
  }
  void SetWasFetchedViaForeignFetch(bool value) {
    was_fetched_via_foreign_fetch_ = value;
  }

  // See ServiceWorkerResponseInfo::was_fallback_required.
  bool WasFallbackRequiredByServiceWorker() const {
    return was_fallback_required_by_service_worker_;
  }
  void SetWasFallbackRequiredByServiceWorker(bool value) {
    was_fallback_required_by_service_worker_ = value;
  }

  WebServiceWorkerResponseType ServiceWorkerResponseType() const {
    return service_worker_response_type_;
  }
  void SetServiceWorkerResponseType(WebServiceWorkerResponseType value) {
    service_worker_response_type_ = value;
  }

  // See ServiceWorkerResponseInfo::url_list_via_service_worker.
  const Vector<KURL>& UrlListViaServiceWorker() const {
    return url_list_via_service_worker_;
  }
  void SetURLListViaServiceWorker(const Vector<KURL>& url_list) {
    url_list_via_service_worker_ = url_list;
  }

  // Returns the last URL of urlListViaServiceWorker if exists. Otherwise
  // returns an empty URL.
  KURL OriginalURLViaServiceWorker() const;

  const Vector<char>& MultipartBoundary() const { return multipart_boundary_; }
  void SetMultipartBoundary(const char* bytes, size_t size) {
    multipart_boundary_.clear();
    multipart_boundary_.Append(bytes, size);
  }

  const String& CacheStorageCacheName() const {
    return cache_storage_cache_name_;
  }
  void SetCacheStorageCacheName(const String& cache_storage_cache_name) {
    cache_storage_cache_name_ = cache_storage_cache_name;
  }

  const Vector<String>& CorsExposedHeaderNames() const {
    return cors_exposed_header_names_;
  }
  void SetCorsExposedHeaderNames(const Vector<String>& header_names) {
    cors_exposed_header_names_ = header_names;
  }

  bool DidServiceWorkerNavigationPreload() const {
    return did_service_worker_navigation_preload_;
  }
  void SetDidServiceWorkerNavigationPreload(bool value) {
    did_service_worker_navigation_preload_ = value;
  }

  int64_t ResponseTime() const { return response_time_; }
  void SetResponseTime(int64_t response_time) {
    response_time_ = response_time;
  }

  const AtomicString& RemoteIPAddress() const { return remote_ip_address_; }
  void SetRemoteIPAddress(const AtomicString& value) {
    remote_ip_address_ = value;
  }

  unsigned short RemotePort() const { return remote_port_; }
  void SetRemotePort(unsigned short value) { remote_port_ = value; }

  long long EncodedDataLength() const { return encoded_data_length_; }
  void SetEncodedDataLength(long long value);

  long long EncodedBodyLength() const { return encoded_body_length_; }
  void SetEncodedBodyLength(long long value);

  long long DecodedBodyLength() const { return decoded_body_length_; }
  void SetDecodedBodyLength(long long value);

  const String& DownloadedFilePath() const { return downloaded_file_path_; }
  void SetDownloadedFilePath(const String&);

  // Extra data associated with this response.
  ExtraData* GetExtraData() const { return extra_data_.Get(); }
  void SetExtraData(RefPtr<ExtraData> extra_data) {
    extra_data_ = std::move(extra_data);
  }

  unsigned MemoryUsage() const {
    // average size, mostly due to URL and Header Map strings
    return 1280;
  }

  // PlzNavigate: Even if there is redirections, only one
  // ResourceResponse is built: the final response.
  // The redirect response chain can be accessed by this function.
  const Vector<ResourceResponse>& RedirectResponses() const {
    return redirect_responses_;
  }
  void AppendRedirectResponse(const ResourceResponse&);

  // This method doesn't compare the all members.
  static bool Compare(const ResourceResponse&, const ResourceResponse&);

 private:
  void UpdateHeaderParsedState(const AtomicString& name);

  KURL url_;
  AtomicString mime_type_;
  long long expected_content_length_;
  AtomicString text_encoding_name_;
  unsigned connection_id_;
  int http_status_code_;
  AtomicString http_status_text_;
  HTTPHeaderMap http_header_fields_;

  // Remote IP address of the socket which fetched this resource.
  AtomicString remote_ip_address_;

  // Remote port number of the socket which fetched this resource.
  unsigned short remote_port_;

  bool was_cached_ : 1;
  bool connection_reused_ : 1;
  bool is_null_ : 1;
  mutable bool have_parsed_age_header_ : 1;
  mutable bool have_parsed_date_header_ : 1;
  mutable bool have_parsed_expires_header_ : 1;
  mutable bool have_parsed_last_modified_header_ : 1;

  // True if the resource was retrieved by the embedder in spite of
  // certificate errors.
  bool has_major_certificate_errors_ : 1;

  // Was the resource fetched over SPDY.  See http://dev.chromium.org/spdy
  bool was_fetched_via_spdy_ : 1;

  // Was the resource fetched over an explicit proxy (HTTP, SOCKS, etc).
  bool was_fetched_via_proxy_ : 1;

  // Was the resource fetched over a ServiceWorker.
  bool was_fetched_via_service_worker_ : 1;

  // Was the resource fetched using a foreign fetch service worker.
  bool was_fetched_via_foreign_fetch_ : 1;

  // Was the fallback request with skip service worker flag required.
  bool was_fallback_required_by_service_worker_ : 1;

  // True if service worker navigation preload was performed due to
  // the request for this resource.
  bool did_service_worker_navigation_preload_ : 1;

  // The type of the response which was fetched by the ServiceWorker.
  WebServiceWorkerResponseType service_worker_response_type_;

  // HTTP version used in the response, if known.
  HTTPVersion http_version_;

  // The security style of the resource.
  // This only contains a valid value when the DevTools Network domain is
  // enabled. (Otherwise, it contains a default value of Unknown.)
  SecurityStyle security_style_;

  // Security details of this request's connection.
  // If m_securityStyle is Unknown or Unauthenticated, this does not contain
  // valid data.
  SecurityDetails security_details_;

  RefPtr<ResourceLoadTiming> resource_load_timing_;
  RefPtr<ResourceLoadInfo> resource_load_info_;

  mutable CacheControlHeader cache_control_header_;

  mutable double age_;
  mutable double date_;
  mutable double expires_;
  mutable double last_modified_;

  // The id of the appcache this response was retrieved from, or zero if
  // the response was not retrieved from an appcache.
  long long app_cache_id_;

  // The manifest url of the appcache this response was retrieved from, if any.
  // Note: only valid for main resource responses.
  KURL app_cache_manifest_url_;

  // The multipart boundary of this response.
  Vector<char> multipart_boundary_;

  // The URL list of the response which was fetched by the ServiceWorker.
  // This is empty if the response was created inside the ServiceWorker.
  Vector<KURL> url_list_via_service_worker_;

  // The cache name of the CacheStorage from where the response is served via
  // the ServiceWorker. Null if the response isn't from the CacheStorage.
  String cache_storage_cache_name_;

  // The headers that should be exposed according to CORS. Only guaranteed
  // to be set if the response was fetched by a ServiceWorker.
  Vector<String> cors_exposed_header_names_;

  // The time at which the response headers were received.  For cached
  // responses, this time could be "far" in the past.
  int64_t response_time_;

  // Size of the response in bytes prior to decompression.
  long long encoded_data_length_;

  // Size of the response body in bytes prior to decompression.
  long long encoded_body_length_;

  // Sizes of the response body in bytes after any content-encoding is
  // removed.
  long long decoded_body_length_;

  // The downloaded file path if the load streamed to a file.
  String downloaded_file_path_;

  // The handle to the downloaded file to ensure the underlying file will not
  // be deleted.
  RefPtr<BlobDataHandle> downloaded_file_handle_;

  // ExtraData associated with the response.
  RefPtr<ExtraData> extra_data_;

  // PlzNavigate: the redirect responses are transmitted
  // inside the final response.
  Vector<ResourceResponse> redirect_responses_;
};

inline bool operator==(const ResourceResponse& a, const ResourceResponse& b) {
  return ResourceResponse::Compare(a, b);
}
inline bool operator!=(const ResourceResponse& a, const ResourceResponse& b) {
  return !(a == b);
}

struct CrossThreadResourceResponseData {
  WTF_MAKE_NONCOPYABLE(CrossThreadResourceResponseData);
  USING_FAST_MALLOC(CrossThreadResourceResponseData);

 public:
  CrossThreadResourceResponseData() {}
  KURL url_;
  String mime_type_;
  long long expected_content_length_;
  String text_encoding_name_;
  int http_status_code_;
  String http_status_text_;
  std::unique_ptr<CrossThreadHTTPHeaderMapData> http_headers_;
  RefPtr<ResourceLoadTiming> resource_load_timing_;
  bool has_major_certificate_errors_;
  ResourceResponse::SecurityStyle security_style_;
  ResourceResponse::SecurityDetails security_details_;
  // This is |certificate| from SecurityDetails since that structure should
  // use an AtomicString but this temporary structure is sent across threads.
  Vector<String> certificate_;
  ResourceResponse::HTTPVersion http_version_;
  long long app_cache_id_;
  KURL app_cache_manifest_url_;
  Vector<char> multipart_boundary_;
  bool was_fetched_via_spdy_;
  bool was_fetched_via_proxy_;
  bool was_fetched_via_service_worker_;
  bool was_fetched_via_foreign_fetch_;
  bool was_fallback_required_by_service_worker_;
  WebServiceWorkerResponseType service_worker_response_type_;
  Vector<KURL> url_list_via_service_worker_;
  String cache_storage_cache_name_;
  bool did_service_worker_navigation_preload_;
  int64_t response_time_;
  String remote_ip_address_;
  unsigned short remote_port_;
  long long encoded_data_length_;
  long long encoded_body_length_;
  long long decoded_body_length_;
  String downloaded_file_path_;
  RefPtr<BlobDataHandle> downloaded_file_handle_;
};

}  // namespace blink

#endif  // ResourceResponse_h
