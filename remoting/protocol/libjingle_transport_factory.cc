// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/libjingle_transport_factory.h"

#include "base/callback.h"
#include "base/single_thread_task_runner.h"
#include "base/thread_task_runner_handle.h"
#include "base/timer/timer.h"
#include "jingle/glue/channel_socket_adapter.h"
#include "jingle/glue/utils.h"
#include "net/base/net_errors.h"
#include "remoting/protocol/network_settings.h"
#include "remoting/signaling/jingle_info_request.h"
#include "third_party/libjingle/source/talk/p2p/base/constants.h"
#include "third_party/libjingle/source/talk/p2p/base/p2ptransportchannel.h"
#include "third_party/libjingle/source/talk/p2p/base/port.h"
#include "third_party/libjingle/source/talk/p2p/client/basicportallocator.h"
#include "third_party/libjingle/source/talk/p2p/client/httpportallocator.h"
#include "third_party/webrtc/base/network.h"

namespace remoting {
namespace protocol {

namespace {

// Try connecting ICE twice with timeout of 15 seconds for each attempt.
const int kMaxReconnectAttempts = 2;
const int kReconnectDelaySeconds = 15;

// Get fresh STUN/Relay configuration every hour.
const int kJingleInfoUpdatePeriodSeconds = 3600;

class LibjingleTransport
    : public Transport,
      public base::SupportsWeakPtr<LibjingleTransport>,
      public sigslot::has_slots<> {
 public:
  LibjingleTransport(cricket::PortAllocator* port_allocator,
                           const NetworkSettings& network_settings);
  virtual ~LibjingleTransport();

  // Called by JingleTransportFactory when it has fresh Jingle info.
  void OnCanStart();

  // Transport interface.
  virtual void Connect(
      const std::string& name,
      Transport::EventHandler* event_handler,
      const Transport::ConnectedCallback& callback) OVERRIDE;
  virtual void AddRemoteCandidate(const cricket::Candidate& candidate) OVERRIDE;
  virtual const std::string& name() const OVERRIDE;
  virtual bool is_connected() const OVERRIDE;

 private:
  void DoStart();
  void NotifyConnected();

  // Signal handlers for cricket::TransportChannel.
  void OnRequestSignaling(cricket::TransportChannelImpl* channel);
  void OnCandidateReady(cricket::TransportChannelImpl* channel,
                        const cricket::Candidate& candidate);
  void OnRouteChange(cricket::TransportChannel* channel,
                     const cricket::Candidate& candidate);
  void OnWritableState(cricket::TransportChannel* channel);

  // Callback for jingle_glue::TransportChannelSocketAdapter to notify when the
  // socket is destroyed.
  void OnChannelDestroyed();

  // Tries to connect by restarting ICE. Called by |reconnect_timer_|.
  void TryReconnect();

  cricket::PortAllocator* port_allocator_;
  NetworkSettings network_settings_;

  std::string name_;
  EventHandler* event_handler_;
  Transport::ConnectedCallback callback_;
  std::string ice_username_fragment_;
  std::string ice_password_;

  bool can_start_;

  std::list<cricket::Candidate> pending_candidates_;
  scoped_ptr<cricket::P2PTransportChannel> channel_;
  bool channel_was_writable_;
  int connect_attempts_left_;
  base::RepeatingTimer<LibjingleTransport> reconnect_timer_;

  base::WeakPtrFactory<LibjingleTransport> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(LibjingleTransport);
};

LibjingleTransport::LibjingleTransport(cricket::PortAllocator* port_allocator,
                                       const NetworkSettings& network_settings)
    : port_allocator_(port_allocator),
      network_settings_(network_settings),
      event_handler_(NULL),
      ice_username_fragment_(
          rtc::CreateRandomString(cricket::ICE_UFRAG_LENGTH)),
      ice_password_(rtc::CreateRandomString(cricket::ICE_PWD_LENGTH)),
      can_start_(false),
      channel_was_writable_(false),
      connect_attempts_left_(kMaxReconnectAttempts),
      weak_factory_(this) {
  DCHECK(!ice_username_fragment_.empty());
  DCHECK(!ice_password_.empty());
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

  while (!pending_candidates_.empty()) {
    channel_->OnCandidate(pending_candidates_.front());
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
      std::string(), 0, NULL, port_allocator_));
  channel_->SetIceProtocolType(cricket::ICEPROTO_GOOGLE);
  channel_->SetIceCredentials(ice_username_fragment_, ice_password_);
  channel_->SignalRequestSignaling.connect(
      this, &LibjingleTransport::OnRequestSignaling);
  channel_->SignalCandidateReady.connect(
      this, &LibjingleTransport::OnCandidateReady);
  channel_->SignalRouteChange.connect(
      this, &LibjingleTransport::OnRouteChange);
  channel_->SignalWritableState.connect(
      this, &LibjingleTransport::OnWritableState);
  channel_->set_incoming_only(
      !(network_settings_.flags & NetworkSettings::NAT_TRAVERSAL_OUTGOING));

  channel_->Connect();

  --connect_attempts_left_;

  // Start reconnection timer.
  reconnect_timer_.Start(
      FROM_HERE, base::TimeDelta::FromSeconds(kReconnectDelaySeconds),
      this, &LibjingleTransport::TryReconnect);
}

void LibjingleTransport::NotifyConnected() {
  // Create net::Socket adapter for the P2PTransportChannel.
  scoped_ptr<jingle_glue::TransportChannelSocketAdapter> socket(
      new jingle_glue::TransportChannelSocketAdapter(channel_.get()));
  socket->SetOnDestroyedCallback(base::Bind(
      &LibjingleTransport::OnChannelDestroyed, base::Unretained(this)));

  Transport::ConnectedCallback callback = callback_;
  callback_.Reset();
  callback.Run(socket.PassAs<net::Socket>());
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
    channel_->OnCandidate(candidate);
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

void LibjingleTransport::OnRequestSignaling(
    cricket::TransportChannelImpl* channel) {
  DCHECK(CalledOnValidThread());
  channel_->OnSignalingReady();
}

void LibjingleTransport::OnCandidateReady(
    cricket::TransportChannelImpl* channel,
    const cricket::Candidate& candidate) {
  DCHECK(CalledOnValidThread());
  event_handler_->OnTransportCandidate(this, candidate);
}

void LibjingleTransport::OnRouteChange(
    cricket::TransportChannel* channel,
    const cricket::Candidate& candidate) {
  TransportRoute route;

  if (candidate.type() == "local") {
    route.type = TransportRoute::DIRECT;
  } else if (candidate.type() == "stun") {
    route.type = TransportRoute::STUN;
  } else if (candidate.type() == "relay") {
    route.type = TransportRoute::RELAY;
  } else {
    LOG(FATAL) << "Unknown candidate type: " << candidate.type();
  }

  if (!jingle_glue::SocketAddressToIPEndPoint(
          candidate.address(), &route.remote_address)) {
    LOG(FATAL) << "Failed to convert peer IP address.";
  }

  DCHECK(channel_->best_connection());
  const cricket::Candidate& local_candidate =
      channel_->best_connection()->local_candidate();
  if (!jingle_glue::SocketAddressToIPEndPoint(
          local_candidate.address(), &route.local_address)) {
    LOG(FATAL) << "Failed to convert local IP address.";
  }

  event_handler_->OnTransportRouteChange(this, route);
}

void LibjingleTransport::OnWritableState(
    cricket::TransportChannel* channel) {
  DCHECK_EQ(channel, channel_.get());

  if (channel->writable()) {
    if (!channel_was_writable_) {
      channel_was_writable_ = true;
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE,
          base::Bind(&LibjingleTransport::NotifyConnected,
                     weak_factory_.GetWeakPtr()));
    }
    connect_attempts_left_ = kMaxReconnectAttempts;
    reconnect_timer_.Stop();
  } else if (!channel->writable() && channel_was_writable_) {
    reconnect_timer_.Reset();
    TryReconnect();
  }
}

void LibjingleTransport::OnChannelDestroyed() {
  if (is_connected()) {
    // The connection socket is being deleted, so delete the transport too.
    delete this;
  }
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
  ice_password_ = rtc::CreateRandomString(cricket::ICE_PWD_LENGTH);
  channel_->SetIceCredentials(ice_username_fragment_, ice_password_);
}

}  // namespace

LibjingleTransportFactory::LibjingleTransportFactory(
    SignalStrategy* signal_strategy,
    scoped_ptr<cricket::HttpPortAllocatorBase> port_allocator,
    const NetworkSettings& network_settings)
    : signal_strategy_(signal_strategy),
      port_allocator_(port_allocator.Pass()),
      network_settings_(network_settings) {
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
      new LibjingleTransport(port_allocator_.get(), network_settings_));

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

  return result.PassAs<Transport>();
}

void LibjingleTransportFactory::EnsureFreshJingleInfo() {
  uint32 stun_or_relay_flags = NetworkSettings::NAT_TRAVERSAL_STUN |
      NetworkSettings::NAT_TRAVERSAL_RELAY;
  if (!(network_settings_.flags & stun_or_relay_flags) ||
      jingle_info_request_) {
    return;
  }

  if (base::TimeTicks::Now() - last_jingle_info_update_time_ >
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
