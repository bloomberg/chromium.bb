// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/proxy_resolving_client_socket.h"

#include <stdint.h>
#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/threading/thread_task_runner_handle.h"
#include "net/base/io_buffer.h"
#include "net/base/ip_address.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/http/http_auth_controller.h"
#include "net/http/http_network_session.h"
#include "net/http/http_transaction_factory.h"
#include "net/http/proxy_client_socket.h"
#include "net/log/net_log_source_type.h"
#include "net/socket/client_socket_handle.h"
#include "net/socket/client_socket_pool_manager.h"
#include "net/traffic_annotation/network_traffic_annotation.h"

namespace network {

ProxyResolvingClientSocket::ProxyResolvingClientSocket(
    net::HttpNetworkSession* network_session,
    const net::SSLConfig& ssl_config,
    const GURL& url)
    : network_session_(network_session),
      ssl_config_(ssl_config),
      proxy_resolve_request_(nullptr),
      url_(url),
      net_log_(net::NetLogWithSource::Make(network_session_->net_log(),
                                           net::NetLogSourceType::SOCKET)),
      weak_factory_(this) {
  // TODO(xunjieli): Handle invalid URLs more gracefully (at mojo API layer
  // or when the request is created).
  DCHECK(url_.is_valid());
}

ProxyResolvingClientSocket::~ProxyResolvingClientSocket() {
  Disconnect();
}

int ProxyResolvingClientSocket::Read(net::IOBuffer* buf,
                                     int buf_len,
                                     const net::CompletionCallback& callback) {
  if (transport_.get() && transport_->socket())
    return transport_->socket()->Read(buf, buf_len, callback);
  return net::ERR_SOCKET_NOT_CONNECTED;
}

int ProxyResolvingClientSocket::Write(
    net::IOBuffer* buf,
    int buf_len,
    const net::CompletionCallback& callback,
    const net::NetworkTrafficAnnotationTag& traffic_annotation) {
  if (transport_.get() && transport_->socket())
    return transport_->socket()->Write(buf, buf_len, callback,
                                       traffic_annotation);
  return net::ERR_SOCKET_NOT_CONNECTED;
}

int ProxyResolvingClientSocket::SetReceiveBufferSize(int32_t size) {
  if (transport_.get() && transport_->socket())
    return transport_->socket()->SetReceiveBufferSize(size);
  return net::ERR_SOCKET_NOT_CONNECTED;
}

int ProxyResolvingClientSocket::SetSendBufferSize(int32_t size) {
  if (transport_.get() && transport_->socket())
    return transport_->socket()->SetSendBufferSize(size);
  return net::ERR_SOCKET_NOT_CONNECTED;
}

int ProxyResolvingClientSocket::Connect(
    const net::CompletionCallback& callback) {
  DCHECK(user_connect_callback_.is_null());

  // First try to resolve the proxy.
  // TODO(xunjieli): Having a null ProxyDelegate is bad. Figure out how to
  // interact with the new interface for proxy delegate.
  // https://crbug.com/793071.
  int net_error = network_session_->proxy_resolution_service()->ResolveProxy(
      url_, "POST", &proxy_info_,
      base::BindRepeating(&ProxyResolvingClientSocket::ConnectToProxy,
                          base::Unretained(this)),
      &proxy_resolve_request_, nullptr /*proxy_delegate*/, net_log_);
  if (net_error != net::ERR_IO_PENDING) {
    // Defer execution of ConnectToProxy instead of calling it
    // directly here for simplicity. From the caller's point of view,
    // the connect always happens asynchronously.
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(&ProxyResolvingClientSocket::ConnectToProxy,
                                  weak_factory_.GetWeakPtr(), net_error));
  }
  user_connect_callback_ = callback;
  return net::ERR_IO_PENDING;
}

void ProxyResolvingClientSocket::Disconnect() {
  CloseTransportSocket();
  if (proxy_resolve_request_) {
    network_session_->proxy_resolution_service()->CancelRequest(
        proxy_resolve_request_);
    proxy_resolve_request_ = nullptr;
  }
  user_connect_callback_.Reset();
}

bool ProxyResolvingClientSocket::IsConnected() const {
  if (!transport_.get() || !transport_->socket())
    return false;
  return transport_->socket()->IsConnected();
}

bool ProxyResolvingClientSocket::IsConnectedAndIdle() const {
  if (!transport_.get() || !transport_->socket())
    return false;
  return transport_->socket()->IsConnectedAndIdle();
}

int ProxyResolvingClientSocket::GetPeerAddress(net::IPEndPoint* address) const {
  if (!transport_.get() || !transport_->socket()) {
    return net::ERR_SOCKET_NOT_CONNECTED;
  }

  if (proxy_info_.is_direct())
    return transport_->socket()->GetPeerAddress(address);

  net::IPAddress ip_address;
  if (!ip_address.AssignFromIPLiteral(url_.HostNoBrackets())) {
    // Do not expose the proxy IP address to the caller.
    return net::ERR_NAME_NOT_RESOLVED;
  }

  *address = net::IPEndPoint(ip_address, url_.EffectiveIntPort());
  return net::OK;
}

int ProxyResolvingClientSocket::GetLocalAddress(
    net::IPEndPoint* address) const {
  if (transport_.get() && transport_->socket())
    return transport_->socket()->GetLocalAddress(address);
  return net::ERR_SOCKET_NOT_CONNECTED;
}

const net::NetLogWithSource& ProxyResolvingClientSocket::NetLog() const {
  if (transport_.get() && transport_->socket())
    return transport_->socket()->NetLog();
  return net_log_;
}

void ProxyResolvingClientSocket::SetSubresourceSpeculation() {
  if (transport_.get() && transport_->socket())
    transport_->socket()->SetSubresourceSpeculation();
}

void ProxyResolvingClientSocket::SetOmniboxSpeculation() {
  if (transport_.get() && transport_->socket())
    transport_->socket()->SetOmniboxSpeculation();
}

bool ProxyResolvingClientSocket::WasEverUsed() const {
  if (transport_.get() && transport_->socket())
    return transport_->socket()->WasEverUsed();
  return false;
}

bool ProxyResolvingClientSocket::WasAlpnNegotiated() const {
  return false;
}

net::NextProto ProxyResolvingClientSocket::GetNegotiatedProtocol() const {
  if (transport_.get() && transport_->socket())
    return transport_->socket()->GetNegotiatedProtocol();
  return net::kProtoUnknown;
}

bool ProxyResolvingClientSocket::GetSSLInfo(net::SSLInfo* ssl_info) {
  return false;
}

void ProxyResolvingClientSocket::GetConnectionAttempts(
    net::ConnectionAttempts* out) const {
  out->clear();
}

int64_t ProxyResolvingClientSocket::GetTotalReceivedBytes() const {
  NOTIMPLEMENTED();
  return 0;
}

void ProxyResolvingClientSocket::ApplySocketTag(const net::SocketTag& tag) {
  NOTIMPLEMENTED();
}

void ProxyResolvingClientSocket::ConnectToProxy(int net_error) {
  proxy_resolve_request_ = nullptr;

  DCHECK_NE(net_error, net::ERR_IO_PENDING);
  if (net_error == net::OK) {
    // Removes unsupported proxies from the list. Currently, this removes
    // just the SCHEME_QUIC proxy, which doesn't yet support tunneling.
    // TODO(xunjieli): Allow QUIC proxy once it supports tunneling.
    proxy_info_.RemoveProxiesWithoutScheme(
        net::ProxyServer::SCHEME_DIRECT | net::ProxyServer::SCHEME_HTTP |
        net::ProxyServer::SCHEME_HTTPS | net::ProxyServer::SCHEME_SOCKS4 |
        net::ProxyServer::SCHEME_SOCKS5);

    if (proxy_info_.is_empty()) {
      // No proxies/direct to choose from. This happens when we don't support
      // any of the proxies in the returned list.
      net_error = net::ERR_NO_SUPPORTED_PROXIES;
    }
  }

  if (net_error != net::OK) {
    CloseTransportSocket();
    base::ResetAndReturn(&user_connect_callback_).Run(net_error);
    return;
  }

  transport_.reset(new net::ClientSocketHandle);
  // Now that the proxy is resolved, issue a socket connect.
  net::HostPortPair host_port_pair = net::HostPortPair::FromURL(url_);
  // Ignore socket limit set by socket pool for this type of socket.
  int request_load_flags = net::LOAD_IGNORE_LIMITS;
  net::RequestPriority request_priority = net::MAXIMUM_PRIORITY;

  net_error = net::InitSocketHandleForRawConnect(
      host_port_pair, network_session_, request_load_flags, request_priority,
      proxy_info_, ssl_config_, ssl_config_, net::PRIVACY_MODE_DISABLED,
      net_log_, transport_.get(),
      base::BindRepeating(&ProxyResolvingClientSocket::ConnectToProxyDone,
                          base::Unretained(this)));
  if (net_error != net::ERR_IO_PENDING) {
    // Since this method is always called asynchronously. it is OK to call
    // ConnectToProxyDone synchronously.
    ConnectToProxyDone(net_error);
  }
}

void ProxyResolvingClientSocket::ConnectToProxyDone(int net_error) {
  if (net_error != net::OK) {
    // If the connection fails, try another proxy.
    net_error = ReconsiderProxyAfterError(net_error);
    // ReconsiderProxyAfterError either returns an error (in which case it is
    // not reconsidering a proxy) or returns ERR_IO_PENDING if it is considering
    // another proxy.
    DCHECK_NE(net_error, net::OK);
    if (net_error == net::ERR_IO_PENDING) {
      // Proxy reconsideration pending. Return.
      return;
    }
    CloseTransportSocket();
  } else {
    network_session_->proxy_resolution_service()->ReportSuccess(proxy_info_,
                                                                nullptr);
  }
  base::ResetAndReturn(&user_connect_callback_).Run(net_error);
}

void ProxyResolvingClientSocket::CloseTransportSocket() {
  if (transport_.get() && transport_->socket())
    transport_->socket()->Disconnect();
  transport_.reset();
}

// TODO(xunjieli): This following method is out of sync with
// HttpStreamFactoryImpl::JobController. The logic should be refactored into a
// common place.
// This method reconsiders the proxy on certain errors. If it does
// reconsider a proxy it always returns ERR_IO_PENDING and posts a call to
// ConnectToProxy with the result of the reconsideration.
int ProxyResolvingClientSocket::ReconsiderProxyAfterError(int error) {
  DCHECK(!proxy_resolve_request_);
  DCHECK_NE(error, net::OK);
  DCHECK_NE(error, net::ERR_IO_PENDING);
  // A failure to resolve the hostname or any error related to establishing a
  // TCP connection could be grounds for trying a new proxy configuration.
  //
  // Why do this when a hostname cannot be resolved?  Some URLs only make sense
  // to proxy servers.  The hostname in those URLs might fail to resolve if we
  // are still using a non-proxy config.  We need to check if a proxy config
  // now exists that corresponds to a proxy server that could load the URL.
  //
  switch (error) {
    case net::ERR_PROXY_CONNECTION_FAILED:
    case net::ERR_NAME_NOT_RESOLVED:
    case net::ERR_INTERNET_DISCONNECTED:
    case net::ERR_ADDRESS_UNREACHABLE:
    case net::ERR_CONNECTION_CLOSED:
    case net::ERR_CONNECTION_RESET:
    case net::ERR_CONNECTION_REFUSED:
    case net::ERR_CONNECTION_ABORTED:
    case net::ERR_TIMED_OUT:
    case net::ERR_TUNNEL_CONNECTION_FAILED:
    case net::ERR_SOCKS_CONNECTION_FAILED:
      break;
    case net::ERR_SOCKS_CONNECTION_HOST_UNREACHABLE:
      // Remap the SOCKS-specific "host unreachable" error to a more
      // generic error code (this way consumers like the link doctor
      // know to substitute their error page).
      //
      // Note that if the host resolving was done by the SOCSK5 proxy, we can't
      // differentiate between a proxy-side "host not found" versus a proxy-side
      // "address unreachable" error, and will report both of these failures as
      // ERR_ADDRESS_UNREACHABLE.
      return net::ERR_ADDRESS_UNREACHABLE;
    case net::ERR_PROXY_AUTH_REQUESTED: {
      net::ProxyClientSocket* proxy_socket =
          static_cast<net::ProxyClientSocket*>(transport_->socket());

      if (proxy_socket->GetAuthController()->HaveAuth()) {
        return proxy_socket->RestartWithAuth(
            base::BindRepeating(&ProxyResolvingClientSocket::ConnectToProxyDone,
                                base::Unretained(this)));
      }
      return error;
    }
    default:
      return error;
  }

  if (proxy_info_.is_https() && ssl_config_.send_client_cert) {
    network_session_->ssl_client_auth_cache()->Remove(
        proxy_info_.proxy_server().host_port_pair());
  }

  // There was nothing left to fall-back to, so fail the transaction
  // with the last connection error we got.
  if (!proxy_info_.Fallback(error, net_log_))
    return error;

  CloseTransportSocket();

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&ProxyResolvingClientSocket::ConnectToProxy,
                                weak_factory_.GetWeakPtr(), net::OK));
  // Since we potentially have another try to go, set the return code code to
  // ERR_IO_PENDING.
  return net::ERR_IO_PENDING;
}

}  // namespace network
