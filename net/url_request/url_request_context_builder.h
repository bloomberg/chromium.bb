// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This class is useful for building a simple URLRequestContext. Most creators
// of new URLRequestContexts should use this helper class to construct it. Call
// any configuration params, and when done, invoke Build() to construct the
// URLRequestContext. This URLRequestContext will own all its own storage.
//
// URLRequestContextBuilder and its associated params classes are initially
// populated with "sane" default values. Read through the comments to figure out
// what these are.

#ifndef NET_URL_REQUEST_URL_REQUEST_CONTEXT_BUILDER_H_
#define NET_URL_REQUEST_URL_REQUEST_CONTEXT_BUILDER_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "build/build_config.h"
#include "net/base/net_export.h"
#include "net/base/network_delegate.h"
#include "net/dns/host_resolver.h"
#include "net/http/http_network_session.h"
#include "net/proxy/proxy_config_service.h"
#include "net/proxy/proxy_service.h"
#include "net/quic/quic_protocol.h"
#include "net/socket/next_proto.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace net {

class ChannelIDService;
class CookieStore;
class FtpTransactionFactory;
class HostMappingRules;
class HttpAuthHandlerFactory;
class HttpServerProperties;
class ProxyConfigService;
class URLRequestContext;
class URLRequestInterceptor;

class NET_EXPORT URLRequestContextBuilder {
 public:
  struct NET_EXPORT HttpCacheParams {
    enum Type {
      IN_MEMORY,
      DISK,
    };

    HttpCacheParams();
    ~HttpCacheParams();

    // The type of HTTP cache. Default is IN_MEMORY.
    Type type;

    // The max size of the cache in bytes. Default is algorithmically determined
    // based off available disk space.
    int max_size;

    // The cache path (when type is DISK).
    base::FilePath path;
  };

  struct NET_EXPORT HttpNetworkSessionParams {
    HttpNetworkSessionParams();
    ~HttpNetworkSessionParams();

    // These fields mirror those in HttpNetworkSession::Params;
    bool ignore_certificate_errors;
    HostMappingRules* host_mapping_rules;
    uint16 testing_fixed_http_port;
    uint16 testing_fixed_https_port;
    NextProtoVector next_protos;
    std::string trusted_spdy_proxy;
    bool use_alternative_services;
    bool enable_quic;
    QuicTagVector quic_connection_options;
  };

  URLRequestContextBuilder();
  ~URLRequestContextBuilder();

  // Extracts the component pointers required to construct an HttpNetworkSession
  // and copies them into the Params used to create the session. This function
  // should be used to ensure that a context and its associated
  // HttpNetworkSession are consistent.
  static void SetHttpNetworkSessionComponents(
      const URLRequestContext* context,
      HttpNetworkSession::Params* params);

  // These functions are mutually exclusive.  The ProxyConfigService, if
  // set, will be used to construct a ProxyService.
  void set_proxy_config_service(
      scoped_ptr<ProxyConfigService> proxy_config_service) {
    proxy_config_service_ = proxy_config_service.Pass();
  }
  void set_proxy_service(scoped_ptr<ProxyService> proxy_service) {
    proxy_service_ = proxy_service.Pass();
  }

  // Call these functions to specify hard-coded Accept-Language
  // or User-Agent header values for all requests that don't
  // have the headers already set.
  void set_accept_language(const std::string& accept_language) {
    accept_language_ = accept_language;
  }
  void set_user_agent(const std::string& user_agent) {
    user_agent_ = user_agent;
  }

  // Control support for data:// requests. By default it's disabled.
  void set_data_enabled(bool enable) {
    data_enabled_ = enable;
  }

#if !defined(DISABLE_FILE_SUPPORT)
  // Control support for file:// requests. By default it's disabled.
  void set_file_enabled(bool enable) {
    file_enabled_ = enable;
  }
#endif

#if !defined(DISABLE_FTP_SUPPORT)
  // Control support for ftp:// requests. By default it's disabled.
  void set_ftp_enabled(bool enable) {
    ftp_enabled_ = enable;
  }
#endif

  // Unlike the other setters, the builder does not take ownership of the
  // NetLog.
  // TODO(mmenke):  Probably makes sense to get rid of this, and have consumers
  // set their own NetLog::Observers instead.
  void set_net_log(NetLog* net_log) { net_log_ = net_log; }

  // By default host_resolver is constructed with CreateDefaultResolver.
  void set_host_resolver(scoped_ptr<HostResolver> host_resolver) {
    host_resolver_ = host_resolver.Pass();
  }

  // Uses BasicNetworkDelegate by default. Note that calling Build will unset
  // any custom delegate in builder, so this must be called each time before
  // Build is called.
  void set_network_delegate(scoped_ptr<NetworkDelegate> delegate) {
    network_delegate_ = delegate.Pass();
  }

  // Adds additional auth handler factories to be used in addition to what is
  // provided in the default |HttpAuthHandlerRegistryFactory|. The auth |scheme|
  // and |factory| are provided. The builder takes ownership of the factory and
  // Build() must be called after this method.
  void add_http_auth_handler_factory(const std::string& scheme,
                                     HttpAuthHandlerFactory* factory) {
    extra_http_auth_handlers_.push_back(SchemeFactory(scheme, factory));
  }

  // By default HttpCache is enabled with a default constructed HttpCacheParams.
  void EnableHttpCache(const HttpCacheParams& params);
  void DisableHttpCache();

  // Override default HttpNetworkSession::Params settings.
  void set_http_network_session_params(
      const HttpNetworkSessionParams& http_network_session_params) {
    http_network_session_params_ = http_network_session_params;
  }

  void set_transport_security_persister_path(
      const base::FilePath& transport_security_persister_path) {
    transport_security_persister_path_ = transport_security_persister_path;
  }

  // Adjust |http_network_session_params_.next_protos| to enable SPDY and QUIC.
  void SetSpdyAndQuicEnabled(bool spdy_enabled,
                             bool quic_enabled);

  void set_quic_connection_options(
      const QuicTagVector& quic_connection_options) {
    http_network_session_params_.quic_connection_options =
        quic_connection_options;
  }

  void set_throttling_enabled(bool throttling_enabled) {
    throttling_enabled_ = throttling_enabled;
  }

  void set_backoff_enabled(bool backoff_enabled) {
    backoff_enabled_ = backoff_enabled;
  }

  void SetCertVerifier(scoped_ptr<CertVerifier> cert_verifier);

  void SetInterceptors(
      ScopedVector<URLRequestInterceptor> url_request_interceptors);

  // Override the default in-memory cookie store and channel id service.
  // |cookie_store| must not be NULL. |channel_id_service| may be NULL to
  // disable channel id for this context.
  // Note that a persistent cookie store should not be used with an in-memory
  // channel id service, and one cookie store should not be shared between
  // multiple channel-id stores (or used both with and without a channel id
  // store).
  void SetCookieAndChannelIdStores(
      const scoped_refptr<CookieStore>& cookie_store,
      scoped_ptr<ChannelIDService> channel_id_service);

  // Sets the task runner used to perform file operations. If not set, one will
  // be created.
  void SetFileTaskRunner(
      const scoped_refptr<base::SingleThreadTaskRunner>& task_runner);

  // Note that if SDCH is enabled without a policy object observing
  // the SDCH manager and handling at least Get-Dictionary events, the
  // result will be "Content-Encoding: sdch" advertisements, but no
  // dictionaries fetches and no specific dictionaries advertised.
  // SdchOwner in net/sdch/sdch_owner.h is a simple policy object.
  void set_sdch_enabled(bool enable) { sdch_enabled_ = enable; }

  // Sets a specific HttpServerProperties for use in the
  // URLRequestContext rather than creating a default HttpServerPropertiesImpl.
  void SetHttpServerProperties(
      scoped_ptr<HttpServerProperties> http_server_properties);

  scoped_ptr<URLRequestContext> Build();

 private:
  struct NET_EXPORT SchemeFactory {
    SchemeFactory(const std::string& scheme, HttpAuthHandlerFactory* factory);
    ~SchemeFactory();

    std::string scheme;
    HttpAuthHandlerFactory* factory;
  };

  std::string accept_language_;
  std::string user_agent_;
  // Include support for data:// requests.
  bool data_enabled_;
#if !defined(DISABLE_FILE_SUPPORT)
  // Include support for file:// requests.
  bool file_enabled_;
#endif
#if !defined(DISABLE_FTP_SUPPORT)
  // Include support for ftp:// requests.
  bool ftp_enabled_;
#endif
  bool http_cache_enabled_;
  bool throttling_enabled_;
  bool backoff_enabled_;
  bool sdch_enabled_;

  scoped_refptr<base::SingleThreadTaskRunner> file_task_runner_;
  HttpCacheParams http_cache_params_;
  HttpNetworkSessionParams http_network_session_params_;
  base::FilePath transport_security_persister_path_;
  NetLog* net_log_;
  scoped_ptr<HostResolver> host_resolver_;
  scoped_ptr<ChannelIDService> channel_id_service_;
  scoped_ptr<ProxyConfigService> proxy_config_service_;
  scoped_ptr<ProxyService> proxy_service_;
  scoped_ptr<NetworkDelegate> network_delegate_;
  scoped_refptr<CookieStore> cookie_store_;
  scoped_ptr<FtpTransactionFactory> ftp_transaction_factory_;
  std::vector<SchemeFactory> extra_http_auth_handlers_;
  scoped_ptr<CertVerifier> cert_verifier_;
  ScopedVector<URLRequestInterceptor> url_request_interceptors_;
  scoped_ptr<HttpServerProperties> http_server_properties_;

  DISALLOW_COPY_AND_ASSIGN(URLRequestContextBuilder);
};

}  // namespace net

#endif  // NET_URL_REQUEST_URL_REQUEST_CONTEXT_BUILDER_H_
