// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef SERVICES_NETWORK_PROXY_RESOLVING_CLIENT_SOCKET_H_
#define SERVICES_NETWORK_PROXY_RESOLVING_CLIENT_SOCKET_H_

#include <stdint.h>

#include <memory>

#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "net/base/completion_callback.h"
#include "net/base/host_port_pair.h"
#include "net/base/net_errors.h"
#include "net/log/net_log_with_source.h"
#include "net/proxy/proxy_info.h"
#include "net/proxy/proxy_service.h"
#include "net/socket/next_proto.h"
#include "net/socket/stream_socket.h"
#include "net/ssl/ssl_config_service.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "url/gurl.h"

namespace net {
class ClientSocketFactory;
class ClientSocketHandle;
class HttpNetworkSession;
class URLRequestContextGetter;
}  // namespace net

namespace network {

// This class represents a net::StreamSocket implementation that does proxy
// resolution for the provided url before establishing a connection. If there is
// a proxy configured, a connection will be established to the proxy.
//
// TODO(xunjieli): https://crbug.com/721401. This class should be private (i.e.
// moved out of services/network/public/cpp). The functionalities will be
// exposed only through a mojo interface.
class ProxyResolvingClientSocket : public net::StreamSocket {
 public:
  // Constructs a new ProxyResolvingClientSocket. |socket_factory| is the
  // ClientSocketFactory that will be used by the underlying HttpNetworkSession.
  // If |socket_factory| is nullptr, the default socket factory
  // (net::ClientSocketFactory::GetDefaultFactory()) will be used. |url|'s host
  // and port specify where a connection will be established to. The full URL
  // will be only used for proxy resolution. Caller doesn't need to explicitly
  // sanitize the url, any sensitive data (like embedded usernames and
  // passwords), and local data (i.e. reference fragment) will be sanitized by
  // net::ProxyService::ResolveProxyHelper() before the url is disclosed to the
  // proxy.
  ProxyResolvingClientSocket(
      net::ClientSocketFactory* socket_factory,
      const scoped_refptr<net::URLRequestContextGetter>& request_context_getter,
      const net::SSLConfig& ssl_config,
      const GURL& url);
  ~ProxyResolvingClientSocket() override;

  // net::StreamSocket implementation.
  int Read(net::IOBuffer* buf,
           int buf_len,
           const net::CompletionCallback& callback) override;
  int Write(
      net::IOBuffer* buf,
      int buf_len,
      const net::CompletionCallback& callback,
      const net::NetworkTrafficAnnotationTag& traffic_annotation) override;
  int SetReceiveBufferSize(int32_t size) override;
  int SetSendBufferSize(int32_t size) override;
  int Connect(const net::CompletionCallback& callback) override;
  void Disconnect() override;
  bool IsConnected() const override;
  bool IsConnectedAndIdle() const override;
  int GetPeerAddress(net::IPEndPoint* address) const override;
  int GetLocalAddress(net::IPEndPoint* address) const override;
  const net::NetLogWithSource& NetLog() const override;
  void SetSubresourceSpeculation() override;
  void SetOmniboxSpeculation() override;
  bool WasEverUsed() const override;
  bool WasAlpnNegotiated() const override;
  net::NextProto GetNegotiatedProtocol() const override;
  bool GetSSLInfo(net::SSLInfo* ssl_info) override;
  void GetConnectionAttempts(net::ConnectionAttempts* out) const override;
  void ClearConnectionAttempts() override {}
  void AddConnectionAttempts(const net::ConnectionAttempts& attempts) override {
  }
  int64_t GetTotalReceivedBytes() const override;
  void ApplySocketTag(const net::SocketTag& tag) override;

 private:
  FRIEND_TEST_ALL_PREFIXES(ProxyResolvingClientSocketTest, ConnectToProxy);
  FRIEND_TEST_ALL_PREFIXES(ProxyResolvingClientSocketTest, ReadWriteErrors);

  void ConnectToProxy(int net_error);
  void ConnectToProxyDone(int net_error);

  void CloseTransportSocket();
  int ReconsiderProxyAfterError(int error);

  std::unique_ptr<net::HttpNetworkSession> network_session_;

  // The transport socket.
  std::unique_ptr<net::ClientSocketHandle> transport_;

  const net::SSLConfig ssl_config_;
  net::ProxyService::Request* proxy_resolve_request_;
  net::ProxyInfo proxy_info_;
  const GURL url_;
  net::NetLogWithSource net_log_;

  // The callback passed to Connect().
  net::CompletionCallback user_connect_callback_;

  base::WeakPtrFactory<ProxyResolvingClientSocket> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ProxyResolvingClientSocket);
};

}  // namespace network

#endif  // SERVICES_NETWORK_PROXY_RESOLVING_CLIENT_SOCKET_H_
