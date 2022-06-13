// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SOCKET_TRANSPORT_CONNECT_JOB_H_
#define NET_SOCKET_TRANSPORT_CONNECT_JOB_H_

#include <memory>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "net/base/host_port_pair.h"
#include "net/base/net_export.h"
#include "net/base/network_isolation_key.h"
#include "net/dns/host_resolver.h"
#include "net/dns/public/resolve_error_info.h"
#include "net/dns/public/secure_dns_policy.h"
#include "net/socket/connect_job.h"
#include "net/socket/connection_attempts.h"
#include "net/socket/socket_tag.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "url/scheme_host_port.h"

namespace net {

class NetLogWithSource;
class SocketTag;

class NET_EXPORT_PRIVATE TransportSocketParams
    : public base::RefCounted<TransportSocketParams> {
 public:
  // Representation of the destination endpoint of the transport
  // socket/connection. Unlike ConnectJobFactory::Endpoint, this does not have a
  // `using_ssl` field for schemeless endpoints because that has no meaning for
  // transport parameters.
  using Endpoint = absl::variant<url::SchemeHostPort, HostPortPair>;

  // |host_resolution_callback| will be invoked after the the hostname is
  // resolved. |network_isolation_key| is passed to the HostResolver to prevent
  // cross-NIK leaks. If |host_resolution_callback| does not return OK, then the
  // connection will be aborted with that value.
  TransportSocketParams(Endpoint destination,
                        NetworkIsolationKey network_isolation_key,
                        SecureDnsPolicy secure_dns_policy,
                        OnHostResolutionCallback host_resolution_callback);

  TransportSocketParams(const TransportSocketParams&) = delete;
  TransportSocketParams& operator=(const TransportSocketParams&) = delete;

  const Endpoint& destination() const { return destination_; }
  const NetworkIsolationKey& network_isolation_key() const {
    return network_isolation_key_;
  }
  SecureDnsPolicy secure_dns_policy() const { return secure_dns_policy_; }
  const OnHostResolutionCallback& host_resolution_callback() const {
    return host_resolution_callback_;
  }

 private:
  friend class base::RefCounted<TransportSocketParams>;
  ~TransportSocketParams();

  const Endpoint destination_;
  const NetworkIsolationKey network_isolation_key_;
  const SecureDnsPolicy secure_dns_policy_;
  const OnHostResolutionCallback host_resolution_callback_;
};

// TransportConnectJob handles the host resolution necessary for socket creation
// and the transport (likely TCP) connect. TransportConnectJob also has fallback
// logic for IPv6 connect() timeouts (which may happen due to networks / routers
// with broken IPv6 support). Those timeouts take 20s, so rather than make the
// user wait 20s for the timeout to fire, we use a fallback timer
// (kIPv6FallbackTimerInMs) and start a connect() to a IPv4 address if the timer
// fires. Then we race the IPv4 connect() against the IPv6 connect() (which has
// a headstart) and return the one that completes first to the socket pool.
class NET_EXPORT_PRIVATE TransportConnectJob : public ConnectJob {
 public:
  // For recording the connection time in the appropriate bucket.
  enum RaceResult {
    RACE_UNKNOWN,
    RACE_IPV4_WINS,
    RACE_IPV4_SOLO,
    RACE_IPV6_WINS,
    RACE_IPV6_SOLO,
  };

  class NET_EXPORT_PRIVATE Factory {
   public:
    Factory() = default;
    virtual ~Factory() = default;

    virtual std::unique_ptr<TransportConnectJob> Create(
        RequestPriority priority,
        const SocketTag& socket_tag,
        const CommonConnectJobParams* common_connect_job_params,
        const scoped_refptr<TransportSocketParams>& params,
        Delegate* delegate,
        const NetLogWithSource* net_log);
  };

  // TransportConnectJobs will time out after this many seconds.  Note this is
  // the total time, including both host resolution and TCP connect() times.
  static const int kTimeoutInSeconds;

  // In cases where both IPv6 and IPv4 addresses were returned from DNS,
  // TransportConnectJobs will start a second connection attempt to just the
  // IPv4 addresses after this many milliseconds. (This is "Happy Eyeballs".)
  static const int kIPv6FallbackTimerInMs;

  // Creates a TransportConnectJob or WebSocketTransportConnectJob, depending on
  // whether or not |common_connect_job_params.web_socket_endpoint_lock_manager|
  // is nullptr.
  // TODO(mmenke): Merge those two ConnectJob classes, and remove this method.
  static std::unique_ptr<ConnectJob> CreateTransportConnectJob(
      scoped_refptr<TransportSocketParams> transport_client_params,
      RequestPriority priority,
      const SocketTag& socket_tag,
      const CommonConnectJobParams* common_connect_job_params,
      ConnectJob::Delegate* delegate,
      const NetLogWithSource* net_log);

  TransportConnectJob(RequestPriority priority,
                      const SocketTag& socket_tag,
                      const CommonConnectJobParams* common_connect_job_params,
                      const scoped_refptr<TransportSocketParams>& params,
                      Delegate* delegate,
                      const NetLogWithSource* net_log);

  TransportConnectJob(const TransportConnectJob&) = delete;
  TransportConnectJob& operator=(const TransportConnectJob&) = delete;

  ~TransportConnectJob() override;

  // ConnectJob methods.
  LoadState GetLoadState() const override;
  bool HasEstablishedConnection() const override;
  ConnectionAttempts GetConnectionAttempts() const override;
  ResolveErrorInfo GetResolveErrorInfo() const override;

  // Rolls |addrlist| forward until the first IPv4 address, if any.
  // WARNING: this method should only be used to implement the prefer-IPv4 hack.
  static void MakeAddressListStartWithIPv4(AddressList* addrlist);

  // Record the histograms Net.DNS_Resolution_And_TCP_Connection_Latency2 and
  // Net.TCP_Connection_Latency and return the connect duration.
  static void HistogramDuration(
      const LoadTimingInfo::ConnectTiming& connect_timing,
      RaceResult race_result);

  static base::TimeDelta ConnectionTimeout();

 private:
  enum State {
    STATE_RESOLVE_HOST,
    STATE_RESOLVE_HOST_COMPLETE,
    STATE_TRANSPORT_CONNECT,
    STATE_TRANSPORT_CONNECT_COMPLETE,
    STATE_NONE,
  };

  void OnIOComplete(int result);
  int DoLoop(int result);

  int DoResolveHost();
  int DoResolveHostComplete(int result);
  int DoTransportConnect();
  int DoTransportConnectComplete(int result);

  // Not part of the state machine.
  void DoIPv6FallbackTransportConnect();
  void DoIPv6FallbackTransportConnectComplete(int result);

  // Begins the host resolution and the TCP connect.  Returns OK on success
  // and ERR_IO_PENDING if it cannot immediately service the request.
  // Otherwise, it returns a net error code.
  int ConnectInternal() override;

  // If host resolution is currently underway, change the priority of the host
  // resolver request.
  void ChangePriorityInternal(RequestPriority priority) override;

  void CopyConnectionAttemptsFromSockets();

  scoped_refptr<TransportSocketParams> params_;
  std::unique_ptr<HostResolver::ResolveHostRequest> request_;

  State next_state_;

  std::unique_ptr<StreamSocket> transport_socket_;

  std::unique_ptr<StreamSocket> fallback_transport_socket_;
  std::unique_ptr<AddressList> fallback_addresses_;
  base::TimeTicks fallback_connect_start_time_;
  base::OneShotTimer fallback_timer_;

  int resolve_result_;
  ResolveErrorInfo resolve_error_info_;

  // Used in the failure case to save connection attempts made on the main and
  // fallback sockets and pass them on in |GetAdditionalErrorState|. (In the
  // success case, connection attempts are passed through the returned socket;
  // attempts are copied from the other socket, if one exists, into it before
  // it is returned.)
  ConnectionAttempts connection_attempts_;
  ConnectionAttempts fallback_connection_attempts_;

  base::WeakPtrFactory<TransportConnectJob> weak_ptr_factory_{this};
};

}  // namespace net

#endif  // NET_SOCKET_TRANSPORT_CONNECT_JOB_H_
