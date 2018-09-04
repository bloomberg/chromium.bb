// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/socket_factory.h"

#include <string>
#include <utility>

#include "base/optional.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/bindings/type_converter.h"
#include "net/base/completion_once_callback.h"
#include "net/base/net_errors.h"
#include "net/cert/cert_verifier.h"
#include "net/cert/ct_policy_enforcer.h"
#include "net/cert/multi_log_ct_verifier.h"
#include "net/log/net_log.h"
#include "net/socket/client_socket_factory.h"
#include "net/socket/client_socket_handle.h"
#include "net/ssl/ssl_config.h"
#include "net/ssl/ssl_config_service.h"
#include "net/url_request/url_request_context.h"
#include "services/network/ssl_config_type_converter.h"
#include "services/network/tls_client_socket.h"
#include "services/network/udp_socket.h"

namespace network {
namespace {
// Cert verifier which blindly accepts all certificates, regardless of validity.
class FakeCertVerifier : public net::CertVerifier {
 public:
  FakeCertVerifier() {}
  ~FakeCertVerifier() override {}

  int Verify(const RequestParams& params,
             net::CertVerifyResult* verify_result,
             net::CompletionOnceCallback,
             std::unique_ptr<Request>*,
             const net::NetLogWithSource&) override {
    verify_result->Reset();
    verify_result->verified_cert = params.certificate();
    return net::OK;
  }
  void SetConfig(const Config& config) override {}
};
}  // namespace

SocketFactory::SocketFactory(net::NetLog* net_log,
                             net::URLRequestContext* url_request_context)
    : net_log_(net_log),
      ssl_client_socket_context_(
          url_request_context->cert_verifier(),
          nullptr, /* TODO(rkn): ChannelIDService is not thread safe. */
          url_request_context->transport_security_state(),
          url_request_context->cert_transparency_verifier(),
          url_request_context->ct_policy_enforcer(),
          std::string() /* TODO(rsleevi): Ensure a proper unique shard. */),
      client_socket_factory_(nullptr),
      ssl_config_service_(url_request_context->ssl_config_service()) {
  if (url_request_context->GetNetworkSessionContext()) {
    client_socket_factory_ =
        url_request_context->GetNetworkSessionContext()->client_socket_factory;
  }
  if (!client_socket_factory_)
    client_socket_factory_ = net::ClientSocketFactory::GetDefaultFactory();
}

SocketFactory::~SocketFactory() {}

void SocketFactory::CreateUDPSocket(mojom::UDPSocketRequest request,
                                    mojom::UDPSocketReceiverPtr receiver) {
  udp_socket_bindings_.AddBinding(
      std::make_unique<UDPSocket>(std::move(receiver), net_log_),
      std::move(request));
}

void SocketFactory::CreateTCPServerSocket(
    const net::IPEndPoint& local_addr,
    int backlog,
    const net::NetworkTrafficAnnotationTag& traffic_annotation,
    mojom::TCPServerSocketRequest request,
    mojom::NetworkContext::CreateTCPServerSocketCallback callback) {
  auto socket =
      std::make_unique<TCPServerSocket>(this, net_log_, traffic_annotation);
  net::IPEndPoint local_addr_out;
  int result = socket->Listen(local_addr, backlog, &local_addr_out);
  if (result != net::OK) {
    std::move(callback).Run(result, base::nullopt);
    return;
  }
  tcp_server_socket_bindings_.AddBinding(std::move(socket), std::move(request));
  std::move(callback).Run(result, local_addr_out);
}

void SocketFactory::CreateTCPConnectedSocket(
    const base::Optional<net::IPEndPoint>& local_addr,
    const net::AddressList& remote_addr_list,
    const net::NetworkTrafficAnnotationTag& traffic_annotation,
    mojom::TCPConnectedSocketRequest request,
    mojom::SocketObserverPtr observer,
    mojom::NetworkContext::CreateTCPConnectedSocketCallback callback) {
  auto socket = std::make_unique<TCPConnectedSocket>(
      std::move(observer), net_log_, this, client_socket_factory_,
      traffic_annotation);
  TCPConnectedSocket* socket_raw = socket.get();
  tcp_connected_socket_bindings_.AddBinding(std::move(socket),
                                            std::move(request));
  socket_raw->Connect(local_addr, remote_addr_list, std::move(callback));
}

void SocketFactory::CreateTCPBoundSocket(
    const net::IPEndPoint& local_addr,
    const net::NetworkTrafficAnnotationTag& traffic_annotation,
    mojom::TCPBoundSocketRequest request,
    mojom::NetworkContext::CreateTCPBoundSocketCallback callback) {
  auto socket =
      std::make_unique<TCPBoundSocket>(this, net_log_, traffic_annotation);
  net::IPEndPoint local_addr_out;
  int result = socket->Bind(local_addr, &local_addr_out);
  if (result != net::OK) {
    std::move(callback).Run(result, base::nullopt);
    return;
  }
  socket->set_id(tcp_bound_socket_bindings_.AddBinding(std::move(socket),
                                                       std::move(request)));
  std::move(callback).Run(result, local_addr_out);
}

void SocketFactory::DestroyBoundSocket(mojo::BindingId bound_socket_id) {
  tcp_bound_socket_bindings_.RemoveBinding(bound_socket_id);
}

void SocketFactory::OnBoundSocketListening(
    mojo::BindingId bound_socket_id,
    std::unique_ptr<TCPServerSocket> server_socket,
    mojom::TCPServerSocketRequest server_socket_request) {
  tcp_server_socket_bindings_.AddBinding(std::move(server_socket),
                                         std::move(server_socket_request));
  tcp_bound_socket_bindings_.RemoveBinding(bound_socket_id);
}

void SocketFactory::OnBoundSocketConnected(
    mojo::BindingId bound_socket_id,
    std::unique_ptr<TCPConnectedSocket> connected_socket,
    mojom::TCPConnectedSocketRequest connected_socket_request) {
  tcp_connected_socket_bindings_.AddBinding(
      std::move(connected_socket), std::move(connected_socket_request));
  tcp_bound_socket_bindings_.RemoveBinding(bound_socket_id);
}

void SocketFactory::CreateTLSClientSocket(
    const net::HostPortPair& host_port_pair,
    mojom::TLSClientSocketOptionsPtr socket_options,
    mojom::TLSClientSocketRequest request,
    std::unique_ptr<net::ClientSocketHandle> tcp_socket,
    mojom::SocketObserverPtr observer,
    const net::NetworkTrafficAnnotationTag& traffic_annotation,
    mojom::TCPConnectedSocket::UpgradeToTLSCallback callback) {
  auto socket = std::make_unique<TLSClientSocket>(
      std::move(request), std::move(observer),
      static_cast<net::NetworkTrafficAnnotationTag>(traffic_annotation));
  TLSClientSocket* socket_raw = socket.get();
  tls_socket_bindings_.AddBinding(std::move(socket), std::move(request));

  net::SSLConfig ssl_config;
  ssl_config_service_->GetSSLConfig(&ssl_config);
  net::SSLClientSocketContext& ssl_client_socket_context =
      ssl_client_socket_context_;

  bool send_ssl_info = false;
  if (socket_options) {
    ssl_config.version_min =
        mojo::MojoSSLVersionToNetSSLVersion(socket_options->version_min);
    ssl_config.version_max =
        mojo::MojoSSLVersionToNetSSLVersion(socket_options->version_max);

    if (socket_options->skip_cert_verification) {
      if (!no_verification_cert_verifier_) {
        no_verification_cert_verifier_ = base::WrapUnique(new FakeCertVerifier);
        no_verification_transport_security_state_.reset(
            new net::TransportSecurityState);
        no_verification_cert_transparency_verifier_.reset(
            new net::MultiLogCTVerifier());
        no_verification_ct_policy_enforcer_.reset(
            new net::DefaultCTPolicyEnforcer());
        no_verification_ssl_client_socket_context_.cert_verifier =
            no_verification_cert_verifier_.get();
        no_verification_ssl_client_socket_context_.transport_security_state =
            no_verification_transport_security_state_.get();
        no_verification_ssl_client_socket_context_.cert_transparency_verifier =
            no_verification_cert_transparency_verifier_.get();
        no_verification_ssl_client_socket_context_.ct_policy_enforcer =
            no_verification_ct_policy_enforcer_.get();
      }
      ssl_client_socket_context = no_verification_ssl_client_socket_context_;
      send_ssl_info = true;
    }
  }
  socket_raw->Connect(host_port_pair, ssl_config, std::move(tcp_socket),
                      ssl_client_socket_context, client_socket_factory_,
                      std::move(callback), send_ssl_info);
}

void SocketFactory::OnAccept(std::unique_ptr<TCPConnectedSocket> socket,
                             mojom::TCPConnectedSocketRequest request) {
  tcp_connected_socket_bindings_.AddBinding(std::move(socket),
                                            std::move(request));
}

}  // namespace network
