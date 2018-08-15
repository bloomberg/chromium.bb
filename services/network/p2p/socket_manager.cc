// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/p2p/socket_manager.h"

#include <stddef.h>

#include <algorithm>

#include "base/bind.h"
#include "base/task/post_task.h"
#include "base/threading/thread_task_runner_handle.h"
#include "net/base/address_list.h"
#include "net/base/net_errors.h"
#include "net/base/network_interfaces.h"
#include "net/base/sys_addrinfo.h"
#include "net/dns/host_resolver.h"
#include "net/log/net_log_source.h"
#include "net/log/net_log_with_source.h"
#include "net/socket/client_socket_factory.h"
#include "net/socket/datagram_client_socket.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"
#include "services/network/p2p/socket.h"
#include "services/network/proxy_resolving_client_socket_factory.h"
#include "services/network/public/cpp/p2p_param_traits.h"

namespace network {

namespace {

// Used by GetDefaultLocalAddress as the target to connect to for getting the
// default local address. They are public DNS servers on the internet.
const uint8_t kPublicIPv4Host[] = {8, 8, 8, 8};
const uint8_t kPublicIPv6Host[] = {
    0x20, 0x01, 0x48, 0x60, 0x48, 0x60, 0, 0, 0, 0, 0, 0, 0, 0, 0x88, 0x88};
const int kPublicPort = 53;  // DNS port.

// Experimentation shows that creating too many sockets creates odd problems
// because of resource exhaustion in the Unix sockets domain.
// Trouble has been seen on Linux at 3479 sockets in test, so leave a margin.
const int kMaxSimultaneousSockets = 3000;

}  // namespace

class P2PSocketManager::DnsRequest {
 public:
  typedef base::Callback<void(const net::IPAddressList&)> DoneCallback;

  explicit DnsRequest(net::HostResolver* host_resolver)
      : resolver_(host_resolver) {}

  void Resolve(const std::string& host_name,
               const DoneCallback& done_callback) {
    DCHECK(!done_callback.is_null());

    host_name_ = host_name;
    done_callback_ = done_callback;

    // Return an error if it's an empty string.
    if (host_name_.empty()) {
      net::IPAddressList address_list;
      done_callback_.Run(address_list);
      return;
    }

    // Add period at the end to make sure that we only resolve
    // fully-qualified names.
    if (host_name_.back() != '.')
      host_name_ += '.';

    net::HostResolver::RequestInfo info(net::HostPortPair(host_name_, 0));
    int result =
        resolver_->Resolve(info, net::DEFAULT_PRIORITY, &addresses_,
                           base::BindOnce(&P2PSocketManager::DnsRequest::OnDone,
                                          base::Unretained(this)),
                           &request_, net::NetLogWithSource());
    if (result != net::ERR_IO_PENDING)
      OnDone(result);
  }

 private:
  void OnDone(int result) {
    net::IPAddressList list;
    if (result != net::OK) {
      LOG(ERROR) << "Failed to resolve address for " << host_name_
                 << ", errorcode: " << result;
      done_callback_.Run(list);
      return;
    }

    DCHECK(!addresses_.empty());
    for (net::AddressList::iterator iter = addresses_.begin();
         iter != addresses_.end(); ++iter) {
      list.push_back(iter->address());
    }
    done_callback_.Run(list);
  }

  net::AddressList addresses_;

  std::string host_name_;
  net::HostResolver* resolver_;
  std::unique_ptr<net::HostResolver::Request> request_;

  DoneCallback done_callback_;
};

P2PSocketManager::P2PSocketManager(
    mojom::P2PTrustedSocketManagerClientPtr trusted_socket_manager_client,
    mojom::P2PTrustedSocketManagerRequest trusted_socket_manager_request,
    mojom::P2PSocketManagerRequest socket_manager_request,
    DeleteCallback delete_callback,
    net::URLRequestContext* url_request_context)
    : delete_callback_(std::move(delete_callback)),
      url_request_context_(url_request_context),
      network_list_task_runner_(base::CreateSequencedTaskRunnerWithTraits(
          {base::MayBlock(), base::TaskPriority::USER_VISIBLE})),
      trusted_socket_manager_client_(std::move(trusted_socket_manager_client)),
      trusted_socket_manager_binding_(
          this,
          std::move(trusted_socket_manager_request)),
      socket_manager_binding_(this, std::move(socket_manager_request)),
      weak_factory_(this) {
  trusted_socket_manager_binding_.set_connection_error_handler(
      base::Bind(&P2PSocketManager::OnConnectionError, base::Unretained(this)));
  socket_manager_binding_.set_connection_error_handler(
      base::Bind(&P2PSocketManager::OnConnectionError, base::Unretained(this)));
}

P2PSocketManager::~P2PSocketManager() {
  sockets_.clear();
  dns_requests_.clear();

  if (network_notification_client_)
    net::NetworkChangeNotifier::RemoveNetworkChangeObserver(this);

  proxy_resolving_socket_factory_.reset();
}

void P2PSocketManager::OnNetworkChanged(
    net::NetworkChangeNotifier::ConnectionType type) {
  // NetworkChangeNotifier always emits CONNECTION_NONE notification whenever
  // network configuration changes. All other notifications can be ignored.
  if (type != net::NetworkChangeNotifier::CONNECTION_NONE)
    return;

  // Notify the renderer about changes to list of network interfaces.
  network_list_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&P2PSocketManager::DoGetNetworkList,
                                weak_factory_.GetWeakPtr(),
                                base::ThreadTaskRunnerHandle::Get()));
}

void P2PSocketManager::AddAcceptedConnection(
    std::unique_ptr<P2PSocket> accepted_connection) {
  sockets_[accepted_connection.get()] = std::move(accepted_connection);
}

void P2PSocketManager::DestroySocket(P2PSocket* socket) {
  auto iter = sockets_.find(socket);
  DCHECK(iter != sockets_.end());
  sockets_.erase(iter);
}

void P2PSocketManager::DumpPacket(const int8_t* packet_header,
                                  size_t header_length,
                                  size_t packet_length,
                                  bool incoming) {
  std::vector<uint8_t> header_buffer(header_length);
  memcpy(&header_buffer[0], packet_header, header_length);
  trusted_socket_manager_client_->DumpPacket(header_buffer, packet_length,
                                             incoming);
}

void P2PSocketManager::DoGetNetworkList(
    const base::WeakPtr<P2PSocketManager>& socket_manager,
    scoped_refptr<base::SingleThreadTaskRunner> main_task_runner) {
  net::NetworkInterfaceList list;
  if (!net::GetNetworkList(&list, net::EXCLUDE_HOST_SCOPE_VIRTUAL_INTERFACES)) {
    LOG(ERROR) << "GetNetworkList failed.";
    return;
  }
  net::IPAddress default_ipv4_local_address = GetDefaultLocalAddress(AF_INET);
  net::IPAddress default_ipv6_local_address = GetDefaultLocalAddress(AF_INET6);
  main_task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(&P2PSocketManager::SendNetworkList, socket_manager, list,
                     default_ipv4_local_address, default_ipv6_local_address));
}

void P2PSocketManager::SendNetworkList(
    const net::NetworkInterfaceList& list,
    const net::IPAddress& default_ipv4_local_address,
    const net::IPAddress& default_ipv6_local_address) {
  network_notification_client_->NetworkListChanged(
      list, default_ipv4_local_address, default_ipv6_local_address);
}

void P2PSocketManager::StartNetworkNotifications(
    mojom::P2PNetworkNotificationClientPtr client) {
  DCHECK(!network_notification_client_);
  network_notification_client_ = std::move(client);
  network_notification_client_.set_connection_error_handler(base::BindOnce(
      &P2PSocketManager::NetworkNotificationClientConnectionError,
      base::Unretained(this)));

  net::NetworkChangeNotifier::AddNetworkChangeObserver(this);

  network_list_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&P2PSocketManager::DoGetNetworkList,
                                weak_factory_.GetWeakPtr(),
                                base::ThreadTaskRunnerHandle::Get()));
}

void P2PSocketManager::GetHostAddress(
    const std::string& host_name,
    mojom::P2PSocketManager::GetHostAddressCallback callback) {
  std::unique_ptr<DnsRequest> request =
      std::make_unique<DnsRequest>(url_request_context_->host_resolver());
  DnsRequest* request_ptr = request.get();
  dns_requests_.insert(std::move(request));
  request_ptr->Resolve(
      host_name,
      base::Bind(&P2PSocketManager::OnAddressResolved, base::Unretained(this),
                 request_ptr, base::Passed(&callback)));
}

void P2PSocketManager::CreateSocket(P2PSocketType type,
                                    const net::IPEndPoint& local_address,
                                    const P2PPortRange& port_range,
                                    const P2PHostAndIPEndPoint& remote_address,
                                    mojom::P2PSocketClientPtr client,
                                    mojom::P2PSocketRequest request) {
  if (port_range.min_port > port_range.max_port ||
      (port_range.min_port == 0 && port_range.max_port != 0)) {
    trusted_socket_manager_client_->InvalidSocketPortRangeRequested();
    return;
  }

  if (!proxy_resolving_socket_factory_) {
    proxy_resolving_socket_factory_ =
        std::make_unique<ProxyResolvingClientSocketFactory>(
            url_request_context_);
  }
  if (sockets_.size() > kMaxSimultaneousSockets) {
    LOG(ERROR) << "Too many sockets created";
    return;
  }
  std::unique_ptr<P2PSocket> socket(
      P2PSocket::Create(this, std::move(client), std::move(request), type,
                        url_request_context_->net_log(),
                        proxy_resolving_socket_factory_.get(), &throttler_));

  if (!socket)
    return;

  if (socket->Init(local_address, port_range.min_port, port_range.max_port,
                   remote_address)) {
    if (dump_incoming_rtp_packet_ || dump_outgoing_rtp_packet_) {
      socket->StartRtpDump(dump_incoming_rtp_packet_,
                           dump_outgoing_rtp_packet_);
    }
    sockets_[socket.get()] = std::move(socket);
  }
}

void P2PSocketManager::StartRtpDump(bool incoming, bool outgoing) {
  if ((!dump_incoming_rtp_packet_ && incoming) ||
      (!dump_outgoing_rtp_packet_ && outgoing)) {
    if (incoming)
      dump_incoming_rtp_packet_ = true;

    if (outgoing)
      dump_outgoing_rtp_packet_ = true;

    for (auto& it : sockets_)
      it.first->StartRtpDump(incoming, outgoing);
  }
}

void P2PSocketManager::StopRtpDump(bool incoming, bool outgoing) {
  if ((dump_incoming_rtp_packet_ && incoming) ||
      (dump_outgoing_rtp_packet_ && outgoing)) {
    if (incoming)
      dump_incoming_rtp_packet_ = false;

    if (outgoing)
      dump_outgoing_rtp_packet_ = false;

    for (auto& it : sockets_)
      it.first->StopRtpDump(incoming, outgoing);
  }
}

void P2PSocketManager::NetworkNotificationClientConnectionError() {
  net::NetworkChangeNotifier::RemoveNetworkChangeObserver(this);
}

net::IPAddress P2PSocketManager::GetDefaultLocalAddress(int family) {
  DCHECK(family == AF_INET || family == AF_INET6);

  // Creation and connection of a UDP socket might be janky.
  // DCHECK(network_list_task_runner_->RunsTasksInCurrentSequence());

  auto socket =
      net::ClientSocketFactory::GetDefaultFactory()->CreateDatagramClientSocket(
          net::DatagramSocket::DEFAULT_BIND, nullptr, net::NetLogSource());

  net::IPAddress ip_address;
  if (family == AF_INET) {
    ip_address = net::IPAddress(kPublicIPv4Host);
  } else {
    ip_address = net::IPAddress(kPublicIPv6Host);
  }

  if (socket->Connect(net::IPEndPoint(ip_address, kPublicPort)) != net::OK) {
    return net::IPAddress();
  }

  net::IPEndPoint local_address;
  if (socket->GetLocalAddress(&local_address) != net::OK)
    return net::IPAddress();

  return local_address.address();
}

void P2PSocketManager::OnAddressResolved(
    DnsRequest* request,
    mojom::P2PSocketManager::GetHostAddressCallback callback,
    const net::IPAddressList& addresses) {
  std::move(callback).Run(addresses);

  dns_requests_.erase(
      std::find_if(dns_requests_.begin(), dns_requests_.end(),
                   [request](const std::unique_ptr<DnsRequest>& ptr) {
                     return ptr.get() == request;
                   }));
}

void P2PSocketManager::OnConnectionError() {
  std::move(delete_callback_).Run(this);
}

}  // namespace network
