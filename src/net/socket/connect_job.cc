// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/connect_job.h"

#include <utility>

#include "base/trace_event/trace_event.h"
#include "net/base/net_errors.h"
#include "net/base/trace_constants.h"
#include "net/http/http_auth_controller.h"
#include "net/log/net_log.h"
#include "net/log/net_log_event_type.h"
#include "net/socket/client_socket_handle.h"
#include "net/socket/stream_socket.h"

namespace net {

CommonConnectJobParams::CommonConnectJobParams(
    ClientSocketFactory* client_socket_factory,
    HostResolver* host_resolver,
    HttpAuthCache* http_auth_cache,
    HttpAuthHandlerFactory* http_auth_handler_factory,
    SpdySessionPool* spdy_session_pool,
    const quic::QuicTransportVersionVector* quic_supported_versions,
    QuicStreamFactory* quic_stream_factory,
    ProxyDelegate* proxy_delegate,
    const HttpUserAgentSettings* http_user_agent_settings,
    const SSLClientSocketContext& ssl_client_socket_context,
    const SSLClientSocketContext& ssl_client_socket_context_privacy_mode,
    SocketPerformanceWatcherFactory* socket_performance_watcher_factory,
    NetworkQualityEstimator* network_quality_estimator,
    NetLog* net_log,
    WebSocketEndpointLockManager* websocket_endpoint_lock_manager)
    : client_socket_factory(client_socket_factory),
      host_resolver(host_resolver),
      http_auth_cache(http_auth_cache),
      http_auth_handler_factory(http_auth_handler_factory),
      spdy_session_pool(spdy_session_pool),
      quic_supported_versions(quic_supported_versions),
      quic_stream_factory(quic_stream_factory),
      proxy_delegate(proxy_delegate),
      http_user_agent_settings(http_user_agent_settings),
      ssl_client_socket_context(ssl_client_socket_context),
      ssl_client_socket_context_privacy_mode(
          ssl_client_socket_context_privacy_mode),
      socket_performance_watcher_factory(socket_performance_watcher_factory),
      network_quality_estimator(network_quality_estimator),
      net_log(net_log),
      websocket_endpoint_lock_manager(websocket_endpoint_lock_manager) {}

CommonConnectJobParams::CommonConnectJobParams(
    const CommonConnectJobParams& other) = default;

CommonConnectJobParams::~CommonConnectJobParams() = default;

CommonConnectJobParams& CommonConnectJobParams::operator=(
    const CommonConnectJobParams& other) = default;

ConnectJob::ConnectJob(RequestPriority priority,
                       const SocketTag& socket_tag,
                       base::TimeDelta timeout_duration,
                       const CommonConnectJobParams* common_connect_job_params,
                       Delegate* delegate,
                       const NetLogWithSource* net_log,
                       NetLogSourceType net_log_source_type,
                       NetLogEventType net_log_connect_event_type)
    : timeout_duration_(timeout_duration),
      priority_(priority),
      socket_tag_(socket_tag),
      common_connect_job_params_(common_connect_job_params),
      delegate_(delegate),
      top_level_job_(net_log == nullptr),
      net_log_(net_log
                   ? *net_log
                   : NetLogWithSource::Make(common_connect_job_params->net_log,
                                            net_log_source_type)),
      net_log_connect_event_type_(net_log_connect_event_type) {
  DCHECK(delegate);
  if (top_level_job_)
    net_log_.BeginEvent(NetLogEventType::CONNECT_JOB);
}

ConnectJob::~ConnectJob() {
  // Log end of Connect event if ConnectJob was still in-progress when
  // destroyed.
  if (delegate_)
    LogConnectCompletion(ERR_ABORTED);
  if (top_level_job_)
    net_log().EndEvent(NetLogEventType::CONNECT_JOB);
}

std::unique_ptr<StreamSocket> ConnectJob::PassSocket() {
  return std::move(socket_);
}

void ConnectJob::ChangePriority(RequestPriority priority) {
  priority_ = priority;
  ChangePriorityInternal(priority);
}

int ConnectJob::Connect() {
  if (!timeout_duration_.is_zero())
    timer_.Start(FROM_HERE, timeout_duration_, this, &ConnectJob::OnTimeout);

  LogConnectStart();

  int rv = ConnectInternal();

  if (rv != ERR_IO_PENDING) {
    LogConnectCompletion(rv);
    delegate_ = nullptr;
  }

  return rv;
}

ConnectionAttempts ConnectJob::GetConnectionAttempts() const {
  // Return empty list by default - used by proxy classes.
  return ConnectionAttempts();
}

std::unique_ptr<StreamSocket> ConnectJob::PassProxySocketOnFailure() {
  return nullptr;
}

bool ConnectJob::IsSSLError() const {
  return false;
}

scoped_refptr<SSLCertRequestInfo> ConnectJob::GetCertRequestInfo() {
  return nullptr;
}

void ConnectJob::SetSocket(std::unique_ptr<StreamSocket> socket) {
  if (socket)
    net_log().AddEvent(NetLogEventType::CONNECT_JOB_SET_SOCKET);
  socket_ = std::move(socket);
}

void ConnectJob::NotifyDelegateOfCompletion(int rv) {
  TRACE_EVENT0(NetTracingCategory(), "ConnectJob::NotifyDelegateOfCompletion");
  // The delegate will own |this|.
  Delegate* delegate = delegate_;
  delegate_ = nullptr;

  LogConnectCompletion(rv);
  delegate->OnConnectJobComplete(rv, this);
}

void ConnectJob::NotifyDelegateOfProxyAuth(
    const HttpResponseInfo& response,
    HttpAuthController* auth_controller,
    base::OnceClosure restart_with_auth_callback) {
  delegate_->OnNeedsProxyAuth(response, auth_controller,
                              std::move(restart_with_auth_callback), this);
}

void ConnectJob::ResetTimer(base::TimeDelta remaining_time) {
  timer_.Stop();
  if (!remaining_time.is_zero())
    timer_.Start(FROM_HERE, remaining_time, this, &ConnectJob::OnTimeout);
}

bool ConnectJob::TimerIsRunning() const {
  return timer_.IsRunning();
}

void ConnectJob::LogConnectStart() {
  connect_timing_.connect_start = base::TimeTicks::Now();
  net_log().BeginEvent(net_log_connect_event_type_);
}

void ConnectJob::LogConnectCompletion(int net_error) {
  connect_timing_.connect_end = base::TimeTicks::Now();
  net_log().EndEventWithNetErrorCode(net_log_connect_event_type_, net_error);
}

void ConnectJob::OnTimeout() {
  // Make sure the socket is NULL before calling into |delegate|.
  SetSocket(nullptr);

  OnTimedOutInternal();

  net_log_.AddEvent(NetLogEventType::CONNECT_JOB_TIMED_OUT);

  NotifyDelegateOfCompletion(ERR_TIMED_OUT);
}

void ConnectJob::OnTimedOutInternal() {}

}  // namespace net
