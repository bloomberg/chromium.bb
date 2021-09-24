// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/socket/socket_api.h"

#include <memory>
#include <unordered_set>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/containers/span.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/storage_partition.h"
#include "extensions/browser/api/socket/socket.h"
#include "extensions/browser/api/socket/tcp_socket.h"
#include "extensions/browser/api/socket/tls_socket.h"
#include "extensions/browser/api/socket/udp_socket.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/common/permissions/socket_permission.h"
#include "net/base/host_port_pair.h"
#include "net/base/io_buffer.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/base/network_interfaces.h"
#include "net/base/network_isolation_key.h"
#include "net/base/url_util.h"
#include "net/dns/public/resolve_error_info.h"
#include "net/log/net_log_with_source.h"
#include "services/network/public/mojom/network_context.mojom.h"

using extensions::mojom::APIPermissionID;

namespace extensions {

namespace {

const char kAddressKey[] = "address";
const char kPortKey[] = "port";
const char kBytesWrittenKey[] = "bytesWritten";
const char kDataKey[] = "data";
const char kResultCodeKey[] = "resultCode";
const char kSocketIdKey[] = "socketId";

const char kSocketNotFoundError[] = "Socket not found";
const char kDnsLookupFailedError[] = "DNS resolution failed";
const char kPermissionError[] = "App does not have permission";
const char kPortInvalidError[] = "Port must be a value between 0 and 65535.";
const char kNetworkListError[] = "Network lookup failed or unsupported";
const char kTCPSocketBindError[] =
    "TCP socket does not support bind. For TCP server please use listen.";
const char kMulticastSocketTypeError[] = "Only UDP socket supports multicast.";
const char kSecureSocketTypeError[] = "Only TCP sockets are supported for TLS.";
const char kSocketNotConnectedError[] = "Socket not connected";
const char kWildcardAddress[] = "*";
const uint16_t kWildcardPort = 0;

#if BUILDFLAG(IS_CHROMEOS_ASH)
const char kFirewallFailure[] = "Failed to open firewall port";
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

bool IsPortValid(int port) {
  return port >= 0 && port <= 65535;
}

}  // namespace

using content::BrowserThread;
using content::SocketPermissionRequest;

SocketApiFunction::SocketApiFunction() = default;

SocketApiFunction::~SocketApiFunction() = default;

int SocketApiFunction::AddSocket(Socket* socket) {
  return manager_->Add(socket);
}

Socket* SocketApiFunction::GetSocket(int api_resource_id) {
  return manager_->Get(extension_id(), api_resource_id);
}

void SocketApiFunction::ReplaceSocket(int api_resource_id, Socket* socket) {
  manager_->Replace(extension_id(), api_resource_id, socket);
}

std::unordered_set<int>* SocketApiFunction::GetSocketIds() {
  return manager_->GetResourceIds(extension_id());
}

void SocketApiFunction::RemoveSocket(int api_resource_id) {
  manager_->Remove(extension_id(), api_resource_id);
}

std::unique_ptr<SocketResourceManagerInterface>
SocketApiFunction::CreateSocketResourceManager() {
  return std::make_unique<SocketResourceManager<Socket>>();
}

void SocketApiFunction::OpenFirewallHole(const std::string& address,
                                         int socket_id,
                                         Socket* socket) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (!net::HostStringIsLocalhost(address)) {
    net::IPEndPoint local_address;
    if (!socket->GetLocalAddress(&local_address)) {
      NOTREACHED() << "Cannot get address of recently bound socket.";
      Respond(ErrorWithCode(-1, kFirewallFailure));
      return;
    }

    AppFirewallHole::PortType type = socket->GetSocketType() == Socket::TYPE_TCP
                                         ? AppFirewallHole::PortType::TCP
                                         : AppFirewallHole::PortType::UDP;

    AppFirewallHoleManager* manager =
        AppFirewallHoleManager::Get(browser_context());
    std::unique_ptr<AppFirewallHole> hole(
        manager->Open(type, local_address.port(), extension_id()).release());

    if (!hole) {
      Respond(ErrorWithCode(-1, kFirewallFailure));
      return;
    }

    Socket* socket = GetSocket(socket_id);
    if (!socket) {
      Respond(ErrorWithCode(-1, kSocketNotFoundError));
      return;
    }

    socket->set_firewall_hole(std::move(hole));
  }
#endif
}

ExtensionFunction::ResponseAction SocketApiFunction::Run() {
  manager_ = CreateSocketResourceManager();
  manager_->SetBrowserContext(browser_context());
  return Work();
}

ExtensionFunction::ResponseValue SocketApiFunction::ErrorWithCode(
    int error_code,
    const std::string& error) {
  std::vector<base::Value> args;
  args.emplace_back(error_code);
  return ErrorWithArguments(std::move(args), error);
}

SocketExtensionWithDnsLookupFunction::SocketExtensionWithDnsLookupFunction() =
    default;

SocketExtensionWithDnsLookupFunction::~SocketExtensionWithDnsLookupFunction() =
    default;

void SocketExtensionWithDnsLookupFunction::StartDnsLookup(
    const net::HostPortPair& host_port_pair) {
  DCHECK(!receiver_.is_bound());

  browser_context()
      ->GetDefaultStoragePartition()
      ->GetNetworkContext()
      ->CreateHostResolver(
          absl::nullopt,
          pending_host_resolver_.InitWithNewPipeAndPassReceiver());
  DCHECK(pending_host_resolver_);

  host_resolver_.Bind(std::move(pending_host_resolver_));
  url::Origin origin = url::Origin::Create(extension_->url());
  host_resolver_->ResolveHost(host_port_pair,
                              net::NetworkIsolationKey(origin, origin), nullptr,
                              receiver_.BindNewPipeAndPassRemote());
  receiver_.set_disconnect_handler(
      base::BindOnce(&SocketExtensionWithDnsLookupFunction::OnComplete,
                     base::Unretained(this), net::ERR_NAME_NOT_RESOLVED,
                     net::ResolveErrorInfo(net::ERR_FAILED), absl::nullopt));

  // Balanced in OnComplete().
  AddRef();
}

void SocketExtensionWithDnsLookupFunction::OnComplete(
    int result,
    const net::ResolveErrorInfo& resolve_error_info,
    const absl::optional<net::AddressList>& resolved_addresses) {
  host_resolver_.reset();
  receiver_.reset();
  if (result == net::OK) {
    DCHECK(resolved_addresses && !resolved_addresses->empty());
    addresses_ = resolved_addresses.value();
  }
  AfterDnsLookup(result);

  Release();  // Added in StartDnsLookup().
}

SocketCreateFunction::SocketCreateFunction() = default;

SocketCreateFunction::~SocketCreateFunction() = default;

ExtensionFunction::ResponseAction SocketCreateFunction::Work() {
  std::unique_ptr<api::socket::Create::Params> params =
      api::socket::Create::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params.get());

  Socket* socket = nullptr;
  switch (params->type) {
    case extensions::api::socket::SOCKET_TYPE_TCP:
      socket = new TCPSocket(browser_context(), extension_id());
      break;

    case extensions::api::socket::SOCKET_TYPE_UDP: {
      mojo::PendingRemote<network::mojom::UDPSocketListener> listener_remote;
      mojo::PendingReceiver<network::mojom::UDPSocketListener>
          socket_listener_receiver =
              listener_remote.InitWithNewPipeAndPassReceiver();
      mojo::PendingRemote<network::mojom::UDPSocket> udp_socket;
      browser_context()
          ->GetDefaultStoragePartition()
          ->GetNetworkContext()
          ->CreateUDPSocket(udp_socket.InitWithNewPipeAndPassReceiver(),
                            std::move(listener_remote));
      socket =
          new UDPSocket(std::move(udp_socket),
                        std::move(socket_listener_receiver), extension_id());
      break;
    }
    case extensions::api::socket::SOCKET_TYPE_NONE:
      NOTREACHED();
      return RespondNow(NoArguments());
  }

  DCHECK(socket);

  base::Value result(base::Value::Type::DICTIONARY);
  result.SetKey(kSocketIdKey, base::Value(AddSocket(socket)));
  return RespondNow(OneArgument(std::move(result)));
}

ExtensionFunction::ResponseAction SocketDestroyFunction::Work() {
  EXTENSION_FUNCTION_VALIDATE(args().size() >= 1);
  const auto& socket_id_value = args()[0];
  EXTENSION_FUNCTION_VALIDATE(socket_id_value.is_int());
  RemoveSocket(socket_id_value.GetInt());
  return RespondNow(NoArguments());
}

SocketConnectFunction::SocketConnectFunction() = default;

SocketConnectFunction::~SocketConnectFunction() = default;

ExtensionFunction::ResponseAction SocketConnectFunction::Work() {
  EXTENSION_FUNCTION_VALIDATE(args().size() >= 3);
  const auto& socket_id_value = args()[0];
  const auto& hostname_value = args()[1];
  const auto& port_value = args()[2];
  EXTENSION_FUNCTION_VALIDATE(socket_id_value.is_int());
  EXTENSION_FUNCTION_VALIDATE(hostname_value.is_string());
  EXTENSION_FUNCTION_VALIDATE(port_value.is_int());
  socket_id_ = socket_id_value.GetInt();
  hostname_ = hostname_value.GetString();
  int port = port_value.GetInt();

  if (!IsPortValid(port)) {
    return RespondNow(Error(kPortInvalidError));
  }
  port_ = static_cast<uint16_t>(port);

  Socket* socket = GetSocket(socket_id_);
  if (!socket) {
    return RespondNow(ErrorWithCode(-1, kSocketNotFoundError));
  }

  socket->set_hostname(hostname_);

  SocketPermissionRequest::OperationType operation_type;
  switch (socket->GetSocketType()) {
    case Socket::TYPE_TCP:
      operation_type = SocketPermissionRequest::TCP_CONNECT;
      break;
    case Socket::TYPE_UDP:
      operation_type = SocketPermissionRequest::UDP_SEND_TO;
      break;
    default:
      NOTREACHED() << "Unknown socket type.";
      operation_type = SocketPermissionRequest::NONE;
      break;
  }

  SocketPermission::CheckParam param(operation_type, hostname_, port_);
  if (!extension()->permissions_data()->CheckAPIPermissionWithParam(
          APIPermissionID::kSocket, &param)) {
    return RespondNow(ErrorWithCode(-1, kPermissionError));
  }

  StartDnsLookup(net::HostPortPair(hostname_, port_));
  return RespondLater();
}

void SocketConnectFunction::AfterDnsLookup(int lookup_result) {
  if (lookup_result == net::OK) {
    StartConnect();
  } else {
    Respond(ErrorWithCode(lookup_result, kDnsLookupFailedError));
  }
}

void SocketConnectFunction::StartConnect() {
  Socket* socket = GetSocket(socket_id_);
  if (!socket) {
    Respond(ErrorWithCode(-1, kSocketNotFoundError));
    return;
  }

  socket->Connect(addresses_,
                  base::BindOnce(&SocketConnectFunction::OnConnect, this));
}

void SocketConnectFunction::OnConnect(int result) {
  Respond(OneArgument(base::Value(result)));
}

ExtensionFunction::ResponseAction SocketDisconnectFunction::Work() {
  EXTENSION_FUNCTION_VALIDATE(args().size() >= 1);
  const auto& socket_id_value = args()[0];
  EXTENSION_FUNCTION_VALIDATE(socket_id_value.is_int());
  int socket_id = socket_id_value.GetInt();

  Socket* socket = GetSocket(socket_id);
  if (socket) {
    socket->Disconnect(false /* socket_destroying */);
    return RespondNow(OneArgument(base::Value()));
  } else {
    std::vector<base::Value> args;
    args.emplace_back();
    return RespondNow(
        ErrorWithArguments(std::move(args), kSocketNotFoundError));
  }
}

ExtensionFunction::ResponseAction SocketBindFunction::Work() {
  EXTENSION_FUNCTION_VALIDATE(args().size() >= 3);
  const auto& socket_id_value = args()[0];
  const auto& address_value = args()[1];
  const auto& port_value = args()[2];
  EXTENSION_FUNCTION_VALIDATE(socket_id_value.is_int());
  EXTENSION_FUNCTION_VALIDATE(address_value.is_string());
  EXTENSION_FUNCTION_VALIDATE(port_value.is_int());
  socket_id_ = socket_id_value.GetInt();
  address_ = address_value.GetString();
  int port = port_value.GetInt();

  if (!IsPortValid(port)) {
    return RespondNow(Error(kPortInvalidError));
  }
  port_ = static_cast<uint16_t>(port);

  Socket* socket = GetSocket(socket_id_);
  if (!socket) {
    return RespondNow(ErrorWithCode(-1, kSocketNotFoundError));
  }

  if (socket->GetSocketType() == Socket::TYPE_TCP) {
    return RespondNow(ErrorWithCode(-1, kTCPSocketBindError));
  }

  CHECK(socket->GetSocketType() == Socket::TYPE_UDP);
  SocketPermission::CheckParam param(SocketPermissionRequest::UDP_BIND,
                                     address_, port_);
  if (!extension()->permissions_data()->CheckAPIPermissionWithParam(
          APIPermissionID::kSocket, &param)) {
    return RespondNow(ErrorWithCode(-1, kPermissionError));
  }

  socket->Bind(address_, port_,
               base::BindOnce(&SocketBindFunction::OnCompleted, this));
  return RespondLater();
}

void SocketBindFunction::OnCompleted(int net_result) {
  Socket* socket = GetSocket(socket_id_);
  if (!socket) {
    Respond(ErrorWithCode(-1, kSocketNotFoundError));
    return;
  }

  if (net_result != net::OK) {
    Respond(OneArgument(base::Value(net_result)));
    return;
  }

  OpenFirewallHole(address_, socket_id_, socket);
  if (!did_respond()) {
    Respond(OneArgument(base::Value(net_result)));
  }
}

SocketListenFunction::SocketListenFunction() = default;

SocketListenFunction::~SocketListenFunction() = default;

ExtensionFunction::ResponseAction SocketListenFunction::Work() {
  params_ = api::socket::Listen::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params_.get());

  Socket* socket = GetSocket(params_->socket_id);
  if (!socket) {
    return RespondNow(ErrorWithCode(-1, kSocketNotFoundError));
  }

  SocketPermission::CheckParam param(SocketPermissionRequest::TCP_LISTEN,
                                     params_->address, params_->port);
  if (!extension()->permissions_data()->CheckAPIPermissionWithParam(
          APIPermissionID::kSocket, &param)) {
    return RespondNow(ErrorWithCode(-1, kPermissionError));
  }

  socket->Listen(params_->address, params_->port,
                 params_->backlog.get() ? *params_->backlog : 5,
                 base::BindOnce(&SocketListenFunction::OnCompleted, this));
  return RespondLater();
}

void SocketListenFunction::OnCompleted(int result,
                                       const std::string& error_msg) {
  DCHECK_NE(net::ERR_IO_PENDING, result);
  Socket* socket = GetSocket(params_->socket_id);
  if (!socket) {
    Respond(ErrorWithCode(-1, kSocketNotFoundError));
    return;
  }

  if (result != net::OK) {
    Respond(ErrorWithCode(result, error_msg));
    return;
  }

  OpenFirewallHole(params_->address, params_->socket_id, socket);
  if (!did_respond()) {
    Respond(OneArgument(base::Value(result)));
  }
}

SocketAcceptFunction::SocketAcceptFunction() = default;

SocketAcceptFunction::~SocketAcceptFunction() = default;

ExtensionFunction::ResponseAction SocketAcceptFunction::Work() {
  std::unique_ptr<api::socket::Accept::Params> params =
      api::socket::Accept::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params.get());

  Socket* socket = GetSocket(params->socket_id);
  if (socket) {
    socket->Accept(base::BindOnce(&SocketAcceptFunction::OnAccept, this));
    return RespondLater();
  } else {
    api::socket::AcceptInfo info;
    info.result_code = net::ERR_FAILED;
    return RespondNow(ErrorWithArguments(
        api::socket::Accept::Results::Create(info), kSocketNotFoundError));
  }
}

void SocketAcceptFunction::OnAccept(
    int result_code,
    mojo::PendingRemote<network::mojom::TCPConnectedSocket> socket,
    const absl::optional<net::IPEndPoint>& remote_addr,
    mojo::ScopedDataPipeConsumerHandle receive_pipe_handle,
    mojo::ScopedDataPipeProducerHandle send_pipe_handle) {
  base::Value result(base::Value::Type::DICTIONARY);
  result.SetIntKey(kResultCodeKey, result_code);
  if (result_code == net::OK) {
    Socket* client_socket =
        new TCPSocket(std::move(socket), std::move(receive_pipe_handle),
                      std::move(send_pipe_handle), remote_addr, extension_id());
    result.SetIntKey(kSocketIdKey, AddSocket(client_socket));
  }
  Respond(OneArgument(std::move(result)));
}

SocketReadFunction::SocketReadFunction() = default;

SocketReadFunction::~SocketReadFunction() = default;

ExtensionFunction::ResponseAction SocketReadFunction::Work() {
  std::unique_ptr<api::socket::Read::Params> params =
      api::socket::Read::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params.get());

  Socket* socket = GetSocket(params->socket_id);
  if (!socket) {
    api::socket::ReadInfo info;
    info.result_code = -1;
    return RespondNow(ErrorWithArguments(
        api::socket::Read::Results::Create(info), kSocketNotFoundError));
  }

  socket->Read(params->buffer_size.get() ? *params->buffer_size : 4096,
               base::BindOnce(&SocketReadFunction::OnCompleted, this));
  return RespondLater();
}

void SocketReadFunction::OnCompleted(int bytes_read,
                                     scoped_refptr<net::IOBuffer> io_buffer,
                                     bool socket_destroying) {
  base::Value result(base::Value::Type::DICTIONARY);
  result.SetIntKey(kResultCodeKey, bytes_read);
  base::span<const uint8_t> data_span;
  if (bytes_read > 0)
    data_span = base::as_bytes(base::make_span(io_buffer->data(), bytes_read));
  result.SetKey(kDataKey, base::Value(data_span));
  Respond(OneArgument(std::move(result)));
}

SocketWriteFunction::SocketWriteFunction() = default;

SocketWriteFunction::~SocketWriteFunction() = default;

ExtensionFunction::ResponseAction SocketWriteFunction::Work() {
  EXTENSION_FUNCTION_VALIDATE(args().size() >= 2);
  const auto& socket_id_value = args()[0];
  const auto& data_value = args()[1];
  EXTENSION_FUNCTION_VALIDATE(socket_id_value.is_int());
  EXTENSION_FUNCTION_VALIDATE(data_value.is_blob());

  int socket_id = socket_id_value.GetInt();
  size_t io_buffer_size = data_value.GetBlob().size();

  scoped_refptr<net::IOBuffer> io_buffer =
      base::MakeRefCounted<net::IOBuffer>(data_value.GetBlob().size());
  base::ranges::copy(data_value.GetBlob(), io_buffer->data());

  Socket* socket = GetSocket(socket_id);
  if (!socket) {
    api::socket::WriteInfo info;
    info.bytes_written = -1;
    return RespondNow(ErrorWithArguments(
        api::socket::Write::Results::Create(info), kSocketNotFoundError));
  }

  socket->Write(io_buffer, io_buffer_size,
                base::BindOnce(&SocketWriteFunction::OnCompleted, this));
  return RespondLater();
}

void SocketWriteFunction::OnCompleted(int bytes_written) {
  base::Value result(base::Value::Type::DICTIONARY);
  result.SetIntKey(kBytesWrittenKey, bytes_written);
  Respond(OneArgument(std::move(result)));
}

SocketRecvFromFunction::SocketRecvFromFunction() = default;

SocketRecvFromFunction::~SocketRecvFromFunction() = default;

ExtensionFunction::ResponseAction SocketRecvFromFunction::Work() {
  std::unique_ptr<api::socket::RecvFrom::Params> params =
      api::socket::RecvFrom::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params.get());

  Socket* socket = GetSocket(params->socket_id);
  if (!socket || socket->GetSocketType() != Socket::TYPE_UDP) {
    api::socket::RecvFromInfo info;
    info.result_code = -1;
    info.port = 0;
    return RespondNow(ErrorWithArguments(
        api::socket::RecvFrom::Results::Create(info), kSocketNotFoundError));
  }

  socket->RecvFrom(params->buffer_size.get() ? *params->buffer_size : 4096,
                   base::BindOnce(&SocketRecvFromFunction::OnCompleted, this));
  return RespondLater();
}

void SocketRecvFromFunction::OnCompleted(int bytes_read,
                                         scoped_refptr<net::IOBuffer> io_buffer,
                                         bool socket_destroying,
                                         const std::string& address,
                                         uint16_t port) {
  base::Value result(base::Value::Type::DICTIONARY);
  result.SetIntKey(kResultCodeKey, bytes_read);
  base::span<const uint8_t> data_span;
  if (bytes_read > 0)
    data_span = base::as_bytes(base::make_span(io_buffer->data(), bytes_read));
  result.SetKey(kDataKey, base::Value(data_span));
  result.SetStringKey(kAddressKey, address);
  result.SetIntKey(kPortKey, port);
  Respond(OneArgument(std::move(result)));
}

SocketSendToFunction::SocketSendToFunction() = default;

SocketSendToFunction::~SocketSendToFunction() = default;

ExtensionFunction::ResponseAction SocketSendToFunction::Work() {
  EXTENSION_FUNCTION_VALIDATE(args().size() >= 4);
  const auto& socket_id_value = args()[0];
  const auto& data_value = args()[1];
  const auto& hostname_value = args()[2];
  const auto& port_value = args()[3];
  EXTENSION_FUNCTION_VALIDATE(socket_id_value.is_int());
  EXTENSION_FUNCTION_VALIDATE(data_value.is_blob());
  EXTENSION_FUNCTION_VALIDATE(hostname_value.is_string());
  EXTENSION_FUNCTION_VALIDATE(port_value.is_int());

  int port = port_value.GetInt();
  if (!IsPortValid(port)) {
    return RespondNow(Error(kPortInvalidError));
  }
  port_ = static_cast<uint16_t>(port);
  socket_id_ = socket_id_value.GetInt();
  hostname_ = hostname_value.GetString();

  io_buffer_size_ = data_value.GetBlob().size();

  io_buffer_ = base::MakeRefCounted<net::IOBuffer>(data_value.GetBlob().size());
  base::ranges::copy(data_value.GetBlob(), io_buffer_->data());

  Socket* socket = GetSocket(socket_id_);
  if (!socket) {
    return RespondNow(ErrorWithCode(-1, kSocketNotFoundError));
  }

  if (socket->GetSocketType() == Socket::TYPE_UDP) {
    SocketPermission::CheckParam param(
        SocketPermissionRequest::UDP_SEND_TO, hostname_, port_);
    if (!extension()->permissions_data()->CheckAPIPermissionWithParam(
            APIPermissionID::kSocket, &param)) {
      return RespondNow(ErrorWithCode(-1, kPermissionError));
    }
  }

  StartDnsLookup(net::HostPortPair(hostname_, port_));
  return RespondLater();
}

void SocketSendToFunction::AfterDnsLookup(int lookup_result) {
  if (lookup_result == net::OK) {
    StartSendTo();
  } else {
    Respond(ErrorWithCode(lookup_result, kDnsLookupFailedError));
  }
}

void SocketSendToFunction::StartSendTo() {
  Socket* socket = GetSocket(socket_id_);
  if (!socket) {
    Respond(ErrorWithCode(-1, kSocketNotFoundError));
    return;
  }

  socket->SendTo(io_buffer_, io_buffer_size_, addresses_.front(),
                 base::BindOnce(&SocketSendToFunction::OnCompleted, this));
}

void SocketSendToFunction::OnCompleted(int bytes_written) {
  api::socket::WriteInfo info;
  info.bytes_written = bytes_written;
  Respond(ArgumentList(api::socket::SendTo::Results::Create(info)));
}

SocketSetKeepAliveFunction::SocketSetKeepAliveFunction() = default;

SocketSetKeepAliveFunction::~SocketSetKeepAliveFunction() = default;

ExtensionFunction::ResponseAction SocketSetKeepAliveFunction::Work() {
  std::unique_ptr<api::socket::SetKeepAlive::Params> params =
      api::socket::SetKeepAlive::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params.get());

  Socket* socket = GetSocket(params->socket_id);
  if (!socket) {
    return RespondNow(
        ErrorWithArguments(api::socket::SetKeepAlive::Results::Create(false),
                           kSocketNotFoundError));
  }
  int delay = 0;
  if (params->delay.get())
    delay = *params->delay;
  socket->SetKeepAlive(
      params->enable, delay,
      base::BindOnce(&SocketSetKeepAliveFunction::OnCompleted, this));
  return RespondLater();
}

void SocketSetKeepAliveFunction::OnCompleted(bool success) {
  Respond(OneArgument(base::Value(success)));
}

SocketSetNoDelayFunction::SocketSetNoDelayFunction() = default;

SocketSetNoDelayFunction::~SocketSetNoDelayFunction() = default;

ExtensionFunction::ResponseAction SocketSetNoDelayFunction::Work() {
  std::unique_ptr<api::socket::SetNoDelay::Params> params =
      api::socket::SetNoDelay::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params.get());

  Socket* socket = GetSocket(params->socket_id);
  if (!socket) {
    return RespondNow(ErrorWithArguments(
        api::socket::SetNoDelay::Results::Create(false), kSocketNotFoundError));
  }
  socket->SetNoDelay(
      params->no_delay,
      base::BindOnce(&SocketSetNoDelayFunction::OnCompleted, this));
  return RespondLater();
}

void SocketSetNoDelayFunction::OnCompleted(bool success) {
  Respond(OneArgument(base::Value(success)));
}

SocketGetInfoFunction::SocketGetInfoFunction() = default;

SocketGetInfoFunction::~SocketGetInfoFunction() = default;

ExtensionFunction::ResponseAction SocketGetInfoFunction::Work() {
  std::unique_ptr<api::socket::GetInfo::Params> params =
      api::socket::GetInfo::Params::Create(args());

  Socket* socket = GetSocket(params->socket_id);
  if (!socket) {
    return RespondNow(Error(kSocketNotFoundError));
  }

  api::socket::SocketInfo info;
  // This represents what we know about the socket, and does not call through
  // to the system.
  if (socket->GetSocketType() == Socket::TYPE_TCP)
    info.socket_type = extensions::api::socket::SOCKET_TYPE_TCP;
  else
    info.socket_type = extensions::api::socket::SOCKET_TYPE_UDP;
  info.connected = socket->IsConnected();

  // Grab the peer address as known by the OS. This and the call below will
  // always succeed while the socket is connected, even if the socket has
  // been remotely closed by the peer; only reading the socket will reveal
  // that it should be closed locally.
  net::IPEndPoint peerAddress;
  if (socket->GetPeerAddress(&peerAddress)) {
    info.peer_address =
        std::make_unique<std::string>(peerAddress.ToStringWithoutPort());
    info.peer_port = std::make_unique<int>(peerAddress.port());
  }

  // Grab the local address as known by the OS.
  net::IPEndPoint localAddress;
  if (socket->GetLocalAddress(&localAddress)) {
    info.local_address =
        std::make_unique<std::string>(localAddress.ToStringWithoutPort());
    info.local_port = std::make_unique<int>(localAddress.port());
  }

  return RespondNow(ArgumentList(api::socket::GetInfo::Results::Create(info)));
}

ExtensionFunction::ResponseAction SocketGetNetworkListFunction::Run() {
  content::GetNetworkService()->GetNetworkList(
      net::INCLUDE_HOST_SCOPE_VIRTUAL_INTERFACES,
      base::BindOnce(&SocketGetNetworkListFunction::GotNetworkList, this));
  return RespondLater();
}

void SocketGetNetworkListFunction::GotNetworkList(
    const absl::optional<net::NetworkInterfaceList>& interface_list) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!interface_list.has_value()) {
    Respond(Error(kNetworkListError));
    return;
  }

  std::vector<api::socket::NetworkInterface> create_arg;
  create_arg.reserve(interface_list->size());
  for (const net::NetworkInterface& interface : interface_list.value()) {
    api::socket::NetworkInterface info;
    info.name = interface.name;
    info.address = interface.address.ToString();
    info.prefix_length = interface.prefix_length;
    create_arg.push_back(std::move(info));
  }

  Respond(
      ArgumentList(api::socket::GetNetworkList::Results::Create(create_arg)));
}

SocketJoinGroupFunction::SocketJoinGroupFunction() = default;

SocketJoinGroupFunction::~SocketJoinGroupFunction() = default;

ExtensionFunction::ResponseAction SocketJoinGroupFunction::Work() {
  std::unique_ptr<api::socket::JoinGroup::Params> params =
      api::socket::JoinGroup::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params.get());

  Socket* socket = GetSocket(params->socket_id);
  if (!socket) {
    return RespondNow(ErrorWithCode(-1, kSocketNotFoundError));
  }

  if (socket->GetSocketType() != Socket::TYPE_UDP) {
    return RespondNow(ErrorWithCode(-1, kMulticastSocketTypeError));
  }

  SocketPermission::CheckParam param(
      SocketPermissionRequest::UDP_MULTICAST_MEMBERSHIP,
      kWildcardAddress,
      kWildcardPort);

  if (!extension()->permissions_data()->CheckAPIPermissionWithParam(
          APIPermissionID::kSocket, &param)) {
    return RespondNow(ErrorWithCode(-1, kPermissionError));
  }

  static_cast<UDPSocket*>(socket)->JoinGroup(
      params->address,
      base::BindOnce(&SocketJoinGroupFunction::OnCompleted, this));
  return RespondLater();
}

void SocketJoinGroupFunction::OnCompleted(int result) {
  if (result == net::OK) {
    Respond(OneArgument(base::Value(result)));
  } else {
    Respond(ErrorWithCode(result, net::ErrorToString(result)));
  }
}

SocketLeaveGroupFunction::SocketLeaveGroupFunction() = default;

SocketLeaveGroupFunction::~SocketLeaveGroupFunction() = default;

ExtensionFunction::ResponseAction SocketLeaveGroupFunction::Work() {
  std::unique_ptr<api::socket::LeaveGroup::Params> params =
      api::socket::LeaveGroup::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params.get());

  Socket* socket = GetSocket(params->socket_id);

  if (!socket) {
    return RespondNow(ErrorWithCode(-1, kSocketNotFoundError));
  }

  if (socket->GetSocketType() != Socket::TYPE_UDP) {
    return RespondNow(ErrorWithCode(-1, kMulticastSocketTypeError));
  }

  SocketPermission::CheckParam param(
      SocketPermissionRequest::UDP_MULTICAST_MEMBERSHIP,
      kWildcardAddress,
      kWildcardPort);
  if (!extension()->permissions_data()->CheckAPIPermissionWithParam(
          APIPermissionID::kSocket, &param)) {
    return RespondNow(ErrorWithCode(-1, kPermissionError));
  }

  static_cast<UDPSocket*>(socket)->LeaveGroup(
      params->address,
      base::BindOnce(&SocketLeaveGroupFunction::OnCompleted, this));
  return RespondLater();
}

void SocketLeaveGroupFunction::OnCompleted(int result) {
  if (result == net::OK) {
    Respond(OneArgument(base::Value(result)));
  } else {
    Respond(ErrorWithCode(result, net::ErrorToString(result)));
  }
}

SocketSetMulticastTimeToLiveFunction::SocketSetMulticastTimeToLiveFunction() =
    default;

SocketSetMulticastTimeToLiveFunction::~SocketSetMulticastTimeToLiveFunction() =
    default;

ExtensionFunction::ResponseAction SocketSetMulticastTimeToLiveFunction::Work() {
  std::unique_ptr<api::socket::SetMulticastTimeToLive::Params> params =
      api::socket::SetMulticastTimeToLive::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params.get());

  Socket* socket = GetSocket(params->socket_id);
  if (!socket) {
    return RespondNow(ErrorWithCode(-1, kSocketNotFoundError));
  }

  if (socket->GetSocketType() != Socket::TYPE_UDP) {
    return RespondNow(ErrorWithCode(-1, kMulticastSocketTypeError));
  }

  int result =
      static_cast<UDPSocket*>(socket)->SetMulticastTimeToLive(params->ttl);
  if (result == 0) {
    return RespondNow(OneArgument(base::Value(result)));
  } else {
    return RespondNow(ErrorWithCode(result, net::ErrorToString(result)));
  }
}

SocketSetMulticastLoopbackModeFunction::
    SocketSetMulticastLoopbackModeFunction() = default;

SocketSetMulticastLoopbackModeFunction::
    ~SocketSetMulticastLoopbackModeFunction() = default;

ExtensionFunction::ResponseAction
SocketSetMulticastLoopbackModeFunction::Work() {
  std::unique_ptr<api::socket::SetMulticastLoopbackMode::Params> params =
      api::socket::SetMulticastLoopbackMode::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params.get());

  Socket* socket = GetSocket(params->socket_id);
  if (!socket) {
    return RespondNow(ErrorWithCode(-1, kSocketNotFoundError));
  }

  if (socket->GetSocketType() != Socket::TYPE_UDP) {
    return RespondNow(ErrorWithCode(-1, kMulticastSocketTypeError));
  }

  int result = static_cast<UDPSocket*>(socket)->SetMulticastLoopbackMode(
      params->enabled);
  if (result == 0) {
    return RespondNow(OneArgument(base::Value(result)));
  } else {
    return RespondNow(ErrorWithCode(result, net::ErrorToString(result)));
  }
}

SocketGetJoinedGroupsFunction::SocketGetJoinedGroupsFunction() = default;

SocketGetJoinedGroupsFunction::~SocketGetJoinedGroupsFunction() = default;

ExtensionFunction::ResponseAction SocketGetJoinedGroupsFunction::Work() {
  std::unique_ptr<api::socket::GetJoinedGroups::Params> params =
      api::socket::GetJoinedGroups::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params.get());

  Socket* socket = GetSocket(params->socket_id);
  if (!socket) {
    return RespondNow(ErrorWithCode(-1, kSocketNotFoundError));
  }

  if (socket->GetSocketType() != Socket::TYPE_UDP) {
    return RespondNow(ErrorWithCode(-1, kMulticastSocketTypeError));
  }

  SocketPermission::CheckParam param(
      SocketPermissionRequest::UDP_MULTICAST_MEMBERSHIP,
      kWildcardAddress,
      kWildcardPort);
  if (!extension()->permissions_data()->CheckAPIPermissionWithParam(
          APIPermissionID::kSocket, &param)) {
    return RespondNow(ErrorWithCode(-1, kPermissionError));
  }

  base::Value values(base::Value::Type::LIST);
  auto* udp_socket = static_cast<UDPSocket*>(socket);
  for (const std::string& group : udp_socket->GetJoinedGroups()) {
    values.Append(group);
  }
  return RespondNow(OneArgument(std::move(values)));
}

SocketSecureFunction::SocketSecureFunction() = default;

SocketSecureFunction::~SocketSecureFunction() = default;

ExtensionFunction::ResponseAction SocketSecureFunction::Work() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  params_ = api::socket::Secure::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params_.get());

  Socket* socket = GetSocket(params_->socket_id);
  if (!socket) {
    return RespondNow(
        ErrorWithCode(net::ERR_INVALID_ARGUMENT, kSocketNotFoundError));
  }

  // Make sure that the socket is a TCP client socket.
  if (socket->GetSocketType() != Socket::TYPE_TCP) {
    return RespondNow(
        ErrorWithCode(net::ERR_INVALID_ARGUMENT, kSecureSocketTypeError));
  }

  if (!socket->IsConnected()) {
    return RespondNow(
        ErrorWithCode(net::ERR_INVALID_ARGUMENT, kSocketNotConnectedError));
  }

  TCPSocket* tcp_socket = static_cast<TCPSocket*>(socket);
  tcp_socket->UpgradeToTLS(
      params_->options.get(),
      base::BindOnce(&SocketSecureFunction::TlsConnectDone, this));
  return RespondLater();
}

void SocketSecureFunction::TlsConnectDone(
    int result,
    mojo::PendingRemote<network::mojom::TLSClientSocket> tls_socket,
    const net::IPEndPoint& local_addr,
    const net::IPEndPoint& peer_addr,
    mojo::ScopedDataPipeConsumerHandle receive_pipe_handle,
    mojo::ScopedDataPipeProducerHandle send_pipe_handle) {
  if (result != net::OK) {
    RemoveSocket(params_->socket_id);
    Respond(ErrorWithCode(result, net::ErrorToString(result)));
    return;
  }

  auto socket =
      std::make_unique<TLSSocket>(std::move(tls_socket), local_addr, peer_addr,
                                  std::move(receive_pipe_handle),
                                  std::move(send_pipe_handle), extension_id());
  ReplaceSocket(params_->socket_id, socket.release());
  Respond(OneArgument(base::Value(result)));
}

}  // namespace extensions
