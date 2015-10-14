// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/libjingle_transport_factory.h"

#include <algorithm>

#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/single_thread_task_runner.h"
#include "base/thread_task_runner_handle.h"
#include "base/timer/timer.h"
#include "jingle/glue/utils.h"
#include "net/base/net_errors.h"
#include "remoting/protocol/channel_socket_adapter.h"
#include "remoting/protocol/network_settings.h"
#include "remoting/signaling/jingle_info_request.h"
#include "third_party/webrtc/base/network.h"
#include "third_party/webrtc/p2p/base/constants.h"
#include "third_party/webrtc/p2p/base/p2ptransportchannel.h"
#include "third_party/webrtc/p2p/base/port.h"
#include "third_party/webrtc/p2p/client/basicportallocator.h"
#include "third_party/webrtc/p2p/client/httpportallocator.h"

namespace remoting {
namespace protocol {

namespace {

// Try connecting ICE twice with timeout of 15 seconds for each attempt.
const int kMaxReconnectAttempts = 2;
const int kReconnectDelaySeconds = 15;

// Get fresh STUN/Relay configuration every hour.
const int kJingleInfoUpdatePeriodSeconds = 3600;

// Utility function to map a cricket::Candidate string type to a
// TransportRoute::RouteType enum value.
TransportRoute::RouteType CandidateTypeToTransportRouteType(
    const std::string& candidate_type) {
  if (candidate_type == "local") {
    return TransportRoute::DIRECT;
  } else if (candidate_type == "stun" || candidate_type == "prflx") {
    return TransportRoute::STUN;
  } else if (candidate_type == "relay") {
    return TransportRoute::RELAY;
  } else {
    LOG(FATAL) << "Unknown candidate type: " << candidate_type;
    return TransportRoute::DIRECT;
  }
}

class LibjingleTransport
    : public Transport,
      public base::SupportsWeakPtr<LibjingleTransport>,
      public sigslot::has_slots<> {
 public:
  LibjingleTransport(cricket::PortAllocator* port_allocator,
                     const NetworkSettings& network_settings,
                     TransportRole role);
  ~LibjingleTransport() override;

  // Called by JingleTransportFactory when it has fresh Jingle info.
  void OnCanStart();

  // Transport interface.
  void Connect(const std::string& name,
               Transport::EventHandler* event_handler,
               const Transport::ConnectedCallback& callback) override;
  void SetRemoteCredentials(const std::string& ufrag,
                            const std::string& password) override;
  void AddRemoteCandidate(const cricket::Candidate& candidate) override;
  const std::string& name() const override;
  bool is_connected() const override;

 private:
  void DoStart();
  void NotifyConnected();

  // Signal handlers for cricket::TransportChannel.
  void OnCandidateGathered(cricket::TransportChannelImpl* channel,
                           const cricket::Candidate& candidate);
  void OnRouteChange(cricket::TransportChannel* channel,
                     const cricket::Candidate& candidate);
  void OnReceivingState(cricket::TransportChannel* channel);
  void OnWritableState(cricket::TransportChannel* channel);

  // Callback for TransportChannelSocketAdapter to notify when the socket is
  // destroyed.
  void OnChannelDestroyed();

  void NotifyRouteChanged();

  // Tries to connect by restarting ICE. Called by |reconnect_timer_|.
  void TryReconnect();

  cricket::PortAllocator* port_allocator_;
  NetworkSettings network_settings_;
  TransportRole role_;

  std::string name_;
  EventHandler* event_handler_;
  Transport::ConnectedCallback callback_;
  std::string ice_username_fragment_;

  bool can_start_;

  std::string remote_ice_username_fragment_;
  std::string remote_ice_password_;
  std::list<cricket::Candidate> pending_candidates_;
  scoped_ptr<cricket::P2PTransportChannel> channel_;
  int connect_attempts_left_;
  base::RepeatingTimer reconnect_timer_;

  base::WeakPtrFactory<LibjingleTransport> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(LibjingleTransport);
};

LibjingleTransport::LibjingleTransport(cricket::PortAllocator* port_allocator,
                                       const NetworkSettings& network_settings,
                                       TransportRole role)
    : port_allocator_(port_allocator),
      network_settings_(network_settings),
      role_(role),
      event_handler_(nullptr),
      ice_username_fragment_(
          rtc::CreateRandomString(cricket::ICE_UFRAG_LENGTH)),
      can_start_(false),
      connect_attempts_left_(kMaxReconnectAttempts),
      weak_factory_(this) {
  DCHECK(!ice_username_fragment_.empty());
}

LibjingleTransport::~LibjingleTransport() {
  DCHECK(event_handler_);

  event_handler_->OnTransportDeleted(this);

  if (channel_.get()) {
    base::ThreadTaskRunnerHandle::Get()->DeleteSoon(
        FROM_HERE, channel_.release());
  }
}

void LibjingleTransport::OnCanStart() {
  DCHECK(CalledOnValidThread());

  DCHECK(!can_start_);
  can_start_ = true;

  // If Connect() has been called then start connection.
  if (!callback_.is_null())
    DoStart();

  // Pass pending ICE credentials and candidates to the channel.
  if (!remote_ice_username_fragment_.empty()) {
    channel_->SetRemoteIceCredentials(remote_ice_username_fragment_,
                                      remote_ice_password_);
  }

  while (!pending_candidates_.empty()) {
    channel_->AddRemoteCandidate(pending_candidates_.front());
    pending_candidates_.pop_front();
  }
}

void LibjingleTransport::Connect(
    const std::string& name,
    Transport::EventHandler* event_handler,
    const Transport::ConnectedCallback& callback) {
  DCHECK(CalledOnValidThread());
  DCHECK(!name.empty());
  DCHECK(event_handler);
  DCHECK(!callback.is_null());

  DCHECK(name_.empty());
  name_ = name;
  event_handler_ = event_handler;
  callback_ = callback;

  if (can_start_)
    DoStart();
}

void LibjingleTransport::DoStart() {
  DCHECK(!channel_.get());

  // Create P2PTransportChannel, attach signal handlers and connect it.
  // TODO(sergeyu): Specify correct component ID for the channel.
  channel_.reset(new cricket::P2PTransportChannel(
      std::string(), 0, nullptr, port_allocator_));
  std::string ice_password = rtc::CreateRandomString(cricket::ICE_PWD_LENGTH);
  channel_->SetIceProtocolType(cricket::ICEPROTO_RFC5245);
  channel_->SetIceRole((role_ == TransportRole::CLIENT)
                           ? cricket::ICEROLE_CONTROLLING
                           : cricket::ICEROLE_CONTROLLED);
  event_handler_->OnTransportIceCredentials(this, ice_username_fragment_,
                                            ice_password);
  channel_->SetIceCredentials(ice_username_fragment_, ice_password);
  channel_->SignalCandidateGathered.connect(
      this, &LibjingleTransport::OnCandidateGathered);
  channel_->SignalRouteChange.connect(
      this, &LibjingleTransport::OnRouteChange);
  channel_->SignalReceivingState.connect(
      this, &LibjingleTransport::OnReceivingState);
  channel_->SignalWritableState.connect(
      this, &LibjingleTransport::OnWritableState);
  channel_->set_incoming_only(
      !(network_settings_.flags & NetworkSettings::NAT_TRAVERSAL_OUTGOING));

  channel_->Connect();
  channel_->MaybeStartGathering();

  --connect_attempts_left_;

  // Start reconnection timer.
  reconnect_timer_.Start(
      FROM_HERE, base::TimeDelta::FromSeconds(kReconnectDelaySeconds),
      this, &LibjingleTransport::TryReconnect);
}

void LibjingleTransport::NotifyConnected() {
  // Create P2PDatagramSocket adapter for the P2PTransportChannel.
  scoped_ptr<TransportChannelSocketAdapter> socket(
      new TransportChannelSocketAdapter(channel_.get()));
  socket->SetOnDestroyedCallback(base::Bind(
      &LibjingleTransport::OnChannelDestroyed, base::Unretained(this)));
  base::ResetAndReturn(&callback_).Run(socket.Pass());
}

void LibjingleTransport::SetRemoteCredentials(const std::string& ufrag,
                                              const std::string& password) {
  DCHECK(CalledOnValidThread());

  remote_ice_username_fragment_ = ufrag;
  remote_ice_password_ = password;

  if (channel_)
    channel_->SetRemoteIceCredentials(ufrag, password);
}

void LibjingleTransport::AddRemoteCandidate(
    const cricket::Candidate& candidate) {
  DCHECK(CalledOnValidThread());

  // To enforce the no-relay setting, it's not enough to not produce relay
  // candidates. It's also necessary to discard remote relay candidates.
  bool relay_allowed = (network_settings_.flags &
                        NetworkSettings::NAT_TRAVERSAL_RELAY) != 0;
  if (!relay_allowed && candidate.type() == cricket::RELAY_PORT_TYPE)
    return;

  if (channel_) {
    channel_->AddRemoteCandidate(candidate);
  } else {
    pending_candidates_.push_back(candidate);
  }
}

const std::string& LibjingleTransport::name() const {
  DCHECK(CalledOnValidThread());
  return name_;
}

bool LibjingleTransport::is_connected() const {
  DCHECK(CalledOnValidThread());
  return callback_.is_null();
}

void LibjingleTransport::OnCandidateGathered(
    cricket::TransportChannelImpl* channel,
    const cricket::Candidate& candidate) {
  DCHECK(CalledOnValidThread());
  event_handler_->OnTransportCandidate(this, candidate);
}

void LibjingleTransport::OnRouteChange(
    cricket::TransportChannel* channel,
    const cricket::Candidate& candidate) {
  // Ignore notifications if the channel is not writable.
  if (channel_->writable())
    NotifyRouteChanged();
}

void LibjingleTransport::OnReceivingState(cricket::TransportChannel* channel) {
  DCHECK_EQ(channel, static_cast<cricket::TransportChannel*>(channel_.get()));

  if (channel->receiving() && !callback_.is_null())
    NotifyConnected();
}

void LibjingleTransport::OnWritableState(cricket::TransportChannel* channel) {
  DCHECK_EQ(channel, static_cast<cricket::TransportChannel*>(channel_.get()));

  if (channel->writable()) {
    connect_attempts_left_ = kMaxReconnectAttempts;
    reconnect_timer_.Stop();

    // Route change notifications are ignored when the |channel_| is not
    // writable. Notify the event handler about the current route once the
    // channel is writable.
    NotifyRouteChanged();
  } else {
    reconnect_timer_.Reset();
    TryReconnect();
  }
}

void LibjingleTransport::OnChannelDestroyed() {
  // The connection socket is being deleted, so delete the transport too.
  delete this;
}

void LibjingleTransport::NotifyRouteChanged() {
  TransportRoute route;

  DCHECK(channel_->best_connection());
  const cricket::Connection* connection = channel_->best_connection();

  // A connection has both a local and a remote candidate. For our purposes, the
  // route type is determined by the most indirect candidate type. For example:
  // it's possible for the local candidate be a "relay" type, while the remote
  // candidate is "local". In this case, we still want to report a RELAY route
  // type.
  static_assert(TransportRoute::DIRECT < TransportRoute::STUN &&
                TransportRoute::STUN < TransportRoute::RELAY,
                "Route type enum values are ordered by 'indirectness'");
  route.type = std::max(
      CandidateTypeToTransportRouteType(connection->local_candidate().type()),
      CandidateTypeToTransportRouteType(connection->remote_candidate().type()));

  if (!jingle_glue::SocketAddressToIPEndPoint(
          connection->remote_candidate().address(), &route.remote_address)) {
    LOG(FATAL) << "Failed to convert peer IP address.";
  }

  const cricket::Candidate& local_candidate =
      channel_->best_connection()->local_candidate();
  if (!jingle_glue::SocketAddressToIPEndPoint(
          local_candidate.address(), &route.local_address)) {
    LOG(FATAL) << "Failed to convert local IP address.";
  }

  event_handler_->OnTransportRouteChange(this, route);
}

void LibjingleTransport::TryReconnect() {
  DCHECK(!channel_->writable());

  if (connect_attempts_left_ <= 0) {
    reconnect_timer_.Stop();

    // Notify the caller that ICE connection has failed - normally that will
    // terminate Jingle connection (i.e. the transport will be destroyed).
    event_handler_->OnTransportFailed(this);
    return;
  }
  --connect_attempts_left_;

  // Restart ICE by resetting ICE password.
  std::string ice_password = rtc::CreateRandomString(cricket::ICE_PWD_LENGTH);
  event_handler_->OnTransportIceCredentials(this, ice_username_fragment_,
                                            ice_password);
  channel_->SetIceCredentials(ice_username_fragment_, ice_password);
}

}  // namespace

LibjingleTransportFactory::LibjingleTransportFactory(
    SignalStrategy* signal_strategy,
    scoped_ptr<cricket::HttpPortAllocatorBase> port_allocator,
    const NetworkSettings& network_settings,
    TransportRole role)
    : signal_strategy_(signal_strategy),
      port_allocator_(port_allocator.Pass()),
      network_settings_(network_settings),
      role_(role) {
}

LibjingleTransportFactory::~LibjingleTransportFactory() {
  // This method may be called in response to a libjingle signal, so
  // libjingle objects must be deleted asynchronously.
  scoped_refptr<base::SingleThreadTaskRunner> task_runner =
      base::ThreadTaskRunnerHandle::Get();
  task_runner->DeleteSoon(FROM_HERE, port_allocator_.release());
}

void LibjingleTransportFactory::PrepareTokens() {
  EnsureFreshJingleInfo();
}

scoped_ptr<Transport> LibjingleTransportFactory::CreateTransport() {
  scoped_ptr<LibjingleTransport> result(
      new LibjingleTransport(port_allocator_.get(), network_settings_, role_));

  EnsureFreshJingleInfo();

  // If there is a pending |jingle_info_request_| delay starting the new
  // transport until the request is finished.
  if (jingle_info_request_) {
    on_jingle_info_callbacks_.push_back(
        base::Bind(&LibjingleTransport::OnCanStart,
                   result->AsWeakPtr()));
  } else {
    result->OnCanStart();
  }

  return result.Pass();
}

void LibjingleTransportFactory::EnsureFreshJingleInfo() {
  uint32 stun_or_relay_flags = NetworkSettings::NAT_TRAVERSAL_STUN |
      NetworkSettings::NAT_TRAVERSAL_RELAY;
  if (!(network_settings_.flags & stun_or_relay_flags) ||
      jingle_info_request_) {
    return;
  }

  if (last_jingle_info_update_time_.is_null() ||
      base::TimeTicks::Now() - last_jingle_info_update_time_ >
          base::TimeDelta::FromSeconds(kJingleInfoUpdatePeriodSeconds)) {
    jingle_info_request_.reset(new JingleInfoRequest(signal_strategy_));
    jingle_info_request_->Send(base::Bind(
        &LibjingleTransportFactory::OnJingleInfo, base::Unretained(this)));
  }
}

void LibjingleTransportFactory::OnJingleInfo(
    const std::string& relay_token,
    const std::vector<std::string>& relay_hosts,
    const std::vector<rtc::SocketAddress>& stun_hosts) {
  if (!relay_token.empty() && !relay_hosts.empty()) {
    port_allocator_->SetRelayHosts(relay_hosts);
    port_allocator_->SetRelayToken(relay_token);
  }
  if (!stun_hosts.empty()) {
    port_allocator_->SetStunHosts(stun_hosts);
  }

  jingle_info_request_.reset();
  if ((!relay_token.empty() && !relay_hosts.empty()) || !stun_hosts.empty())
    last_jingle_info_update_time_ = base::TimeTicks::Now();

  while (!on_jingle_info_callbacks_.empty()) {
    on_jingle_info_callbacks_.begin()->Run();
    on_jingle_info_callbacks_.pop_front();
  }
}

}  // namespace protocol
}  // namespace remoting
