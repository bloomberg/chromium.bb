// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/client_socket_pool.h"

#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "net/http/http_proxy_connect_job.h"
#include "net/log/net_log_event_type.h"
#include "net/log/net_log_with_source.h"
#include "net/socket/socks_connect_job.h"
#include "net/socket/ssl_connect_job.h"
#include "net/socket/stream_socket.h"
#include "net/socket/transport_connect_job.h"
#include "net/socket/websocket_transport_connect_job.h"

namespace net {

namespace {

// The maximum duration, in seconds, to keep used idle persistent sockets alive.
int64_t g_used_idle_socket_timeout_s = 300;  // 5 minutes

// TODO(mmenke): Once the socket pool arguments are no longer needed, remove
// this method and use TransportConnectJob::CreateTransportConnectJob()
// directly.
std::unique_ptr<ConnectJob> CreateTransportConnectJob(
    scoped_refptr<TransportSocketParams> transport_socket_params,
    RequestPriority priority,
    const SocketTag& socket_tag,
    const CommonConnectJobParams* common_connect_job_params,
    ConnectJob::Delegate* delegate) {
  return TransportConnectJob::CreateTransportConnectJob(
      std::move(transport_socket_params), priority, socket_tag,
      common_connect_job_params, delegate, nullptr /* net_log */);
}

std::unique_ptr<ConnectJob> CreateSOCKSConnectJob(
    scoped_refptr<SOCKSSocketParams> socks_socket_params,
    RequestPriority priority,
    const SocketTag& socket_tag,
    const CommonConnectJobParams* common_connect_job_params,
    ConnectJob::Delegate* delegate) {
  return std::make_unique<SOCKSConnectJob>(
      priority, socket_tag, common_connect_job_params,
      std::move(socks_socket_params), delegate, nullptr /* net_log */);
}

std::unique_ptr<ConnectJob> CreateSSLConnectJob(
    scoped_refptr<SSLSocketParams> ssl_socket_params,
    RequestPriority priority,
    const SocketTag& socket_tag,
    const CommonConnectJobParams* common_connect_job_params,
    ConnectJob::Delegate* delegate) {
  return std::make_unique<SSLConnectJob>(
      priority, socket_tag, common_connect_job_params,
      std::move(ssl_socket_params), delegate, nullptr /* net_log */);
}

std::unique_ptr<ConnectJob> CreateHttpProxyConnectJob(
    scoped_refptr<HttpProxySocketParams> http_proxy_socket_params,
    RequestPriority priority,
    const SocketTag& socket_tag,
    const CommonConnectJobParams* common_connect_job_params,
    ConnectJob::Delegate* delegate) {
  return std::make_unique<HttpProxyConnectJob>(
      priority, socket_tag, common_connect_job_params,
      std::move(http_proxy_socket_params), delegate, nullptr /* net_log */);
}

}  // namespace

ClientSocketPool::SocketParams::SocketParams(
    const CreateConnectJobCallback& create_connect_job_callback)
    : create_connect_job_callback_(create_connect_job_callback) {}

scoped_refptr<ClientSocketPool::SocketParams>
ClientSocketPool::SocketParams::CreateFromTransportSocketParams(
    scoped_refptr<TransportSocketParams> transport_client_params) {
  CreateConnectJobCallback callback = base::BindRepeating(
      &CreateTransportConnectJob, std::move(transport_client_params));
  return base::MakeRefCounted<SocketParams>(callback);
}

scoped_refptr<ClientSocketPool::SocketParams>
ClientSocketPool::SocketParams::CreateFromSOCKSSocketParams(
    scoped_refptr<SOCKSSocketParams> socks_socket_params) {
  CreateConnectJobCallback callback = base::BindRepeating(
      &CreateSOCKSConnectJob, std::move(socks_socket_params));
  return base::MakeRefCounted<SocketParams>(callback);
}

scoped_refptr<ClientSocketPool::SocketParams>
ClientSocketPool::SocketParams::CreateFromSSLSocketParams(
    scoped_refptr<SSLSocketParams> ssl_socket_params) {
  CreateConnectJobCallback callback =
      base::BindRepeating(&CreateSSLConnectJob, std::move(ssl_socket_params));
  return base::MakeRefCounted<SocketParams>(callback);
}

scoped_refptr<ClientSocketPool::SocketParams>
ClientSocketPool::SocketParams::CreateFromHttpProxySocketParams(
    scoped_refptr<HttpProxySocketParams> http_proxy_socket_params) {
  CreateConnectJobCallback callback = base::BindRepeating(
      &CreateHttpProxyConnectJob, std::move(http_proxy_socket_params));
  return base::MakeRefCounted<SocketParams>(callback);
}

ClientSocketPool::SocketParams::~SocketParams() = default;

ClientSocketPool::GroupId::GroupId()
    : socket_type_(SocketType::kHttp), privacy_mode_(false) {}

ClientSocketPool::GroupId::GroupId(const HostPortPair& destination,
                                   SocketType socket_type,
                                   bool privacy_mode)
    : destination_(destination),
      socket_type_(socket_type),
      privacy_mode_(privacy_mode) {}

ClientSocketPool::GroupId::GroupId(const GroupId& group_id) = default;

ClientSocketPool::GroupId::~GroupId() = default;

ClientSocketPool::GroupId& ClientSocketPool::GroupId::operator=(
    const GroupId& group_id) = default;

ClientSocketPool::GroupId& ClientSocketPool::GroupId::operator=(
    GroupId&& group_id) = default;

std::string ClientSocketPool::GroupId::ToString() const {
  std::string result = destination_.ToString();
  switch (socket_type_) {
    case ClientSocketPool::SocketType::kHttp:
      break;

    case ClientSocketPool::SocketType::kSsl:
      result = "ssl/" + result;
      break;

    case ClientSocketPool::SocketType::kSslVersionInterferenceProbe:
      result = "version-interference-probe/ssl/" + result;
      break;

    case ClientSocketPool::SocketType::kFtp:
      result = "ftp/" + result;
      break;
  }
  if (privacy_mode_)
    result = "pm/" + result;
  return result;
}

ClientSocketPool::~ClientSocketPool() = default;

// static
base::TimeDelta ClientSocketPool::used_idle_socket_timeout() {
  return base::TimeDelta::FromSeconds(g_used_idle_socket_timeout_s);
}

// static
void ClientSocketPool::set_used_idle_socket_timeout(base::TimeDelta timeout) {
  DCHECK_GT(timeout.InSeconds(), 0);
  g_used_idle_socket_timeout_s = timeout.InSeconds();
}

ClientSocketPool::ClientSocketPool() = default;

void ClientSocketPool::NetLogTcpClientSocketPoolRequestedSocket(
    const NetLogWithSource& net_log,
    const GroupId& group_id) {
  if (net_log.IsCapturing()) {
    // TODO(eroman): Split out the host and port parameters.
    net_log.AddEvent(NetLogEventType::TCP_CLIENT_SOCKET_POOL_REQUESTED_SOCKET,
                     base::BindRepeating(&NetLogGroupIdCallback,
                                         base::Unretained(&group_id)));
  }
}

std::unique_ptr<base::Value> ClientSocketPool::NetLogGroupIdCallback(
    const ClientSocketPool::GroupId* group_id,
    NetLogCaptureMode /* capture_mode */) {
  std::unique_ptr<base::DictionaryValue> event_params(
      new base::DictionaryValue());
  event_params->SetString("group_id", group_id->ToString());
  return event_params;
}

}  // namespace net
