// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/libjingle_transport_factory.h"

#include "base/single_thread_task_runner.h"
#include "base/thread_task_runner_handle.h"
#include "base/timer/timer.h"
#include "jingle/glue/channel_socket_adapter.h"
#include "jingle/glue/pseudotcp_adapter.h"
#include "jingle/glue/thread_wrapper.h"
#include "jingle/glue/utils.h"
#include "net/base/net_errors.h"
#include "remoting/base/constants.h"
#include "remoting/jingle_glue/chromium_port_allocator.h"
#include "remoting/jingle_glue/chromium_socket_factory.h"
#include "remoting/jingle_glue/network_settings.h"
#include "remoting/protocol/channel_authenticator.h"
#include "remoting/protocol/transport_config.h"
#include "third_party/libjingle/source/talk/base/network.h"
#include "third_party/libjingle/source/talk/p2p/base/constants.h"
#include "third_party/libjingle/source/talk/p2p/base/p2ptransportchannel.h"
#include "third_party/libjingle/source/talk/p2p/client/basicportallocator.h"
#include "third_party/libjingle/source/talk/p2p/client/httpportallocator.h"

namespace remoting {
namespace protocol {

namespace {

// Value is chosen to balance the extra latency against the reduced
// load due to ACK traffic.
const int kTcpAckDelayMilliseconds = 10;

// Values for the TCP send and receive buffer size. This should be tuned to
// accommodate high latency network but not backlog the decoding pipeline.
const int kTcpReceiveBufferSize = 256 * 1024;
const int kTcpSendBufferSize = kTcpReceiveBufferSize + 30 * 1024;

// Try connecting ICE twice with timeout of 15 seconds for each attempt.
const int kMaxReconnectAttempts = 2;
const int kReconnectDelaySeconds = 15;

class LibjingleStreamTransport : public StreamTransport,
                                 public sigslot::has_slots<> {
 public:
  LibjingleStreamTransport(cricket::PortAllocator* port_allocator,
                           bool incoming_only);
  virtual ~LibjingleStreamTransport();

  // StreamTransport interface.
  virtual void Initialize(
      const std::string& name,
      Transport::EventHandler* event_handler,
      scoped_ptr<ChannelAuthenticator> authenticator) OVERRIDE;
  virtual void Connect(
      const StreamTransport::ConnectedCallback& callback) OVERRIDE;
  virtual void AddRemoteCandidate(const cricket::Candidate& candidate) OVERRIDE;
  virtual const std::string& name() const OVERRIDE;
  virtual bool is_connected() const OVERRIDE;

 private:
  // Signal handlers for cricket::TransportChannel.
  void OnRequestSignaling(cricket::TransportChannelImpl* channel);
  void OnCandidateReady(cricket::TransportChannelImpl* channel,
                        const cricket::Candidate& candidate);
  void OnRouteChange(cricket::TransportChannel* channel,
                     const cricket::Candidate& candidate);
  void OnWritableState(cricket::TransportChannel* channel);

  // Callback for PseudoTcpAdapter::Connect().
  void OnTcpConnected(int result);

  // Callback for Authenticator::SecureAndAuthenticate();
  void OnAuthenticationDone(net::Error error,
                            scoped_ptr<net::StreamSocket> socket);

  // Callback for jingle_glue::TransportChannelSocketAdapter to notify when the
  // socket is destroyed.
  void OnChannelDestroyed();

  // Tries to connect by restarting ICE. Called by |reconnect_timer_|.
  void TryReconnect();

  // Helper methods to call |callback_|.
  void NotifyConnected(scoped_ptr<net::StreamSocket> socket);
  void NotifyConnectFailed();

  cricket::PortAllocator* port_allocator_;
  bool incoming_only_;

  std::string name_;
  EventHandler* event_handler_;
  StreamTransport::ConnectedCallback callback_;
  scoped_ptr<ChannelAuthenticator> authenticator_;
  std::string ice_username_fragment_;
  std::string ice_password_;

  scoped_ptr<cricket::P2PTransportChannel> channel_;
  bool channel_was_writable_;
  int connect_attempts_left_;
  base::RepeatingTimer<LibjingleStreamTransport> reconnect_timer_;

  // We own |socket_| until it is connected.
  scoped_ptr<jingle_glue::PseudoTcpAdapter> socket_;

  DISALLOW_COPY_AND_ASSIGN(LibjingleStreamTransport);
};

LibjingleStreamTransport::LibjingleStreamTransport(
    cricket::PortAllocator* port_allocator,
    bool incoming_only)
    : port_allocator_(port_allocator),
      incoming_only_(incoming_only),
      event_handler_(NULL),
      ice_username_fragment_(
          talk_base::CreateRandomString(cricket::ICE_UFRAG_LENGTH)),
      ice_password_(talk_base::CreateRandomString(cricket::ICE_PWD_LENGTH)),
      channel_was_writable_(false),
      connect_attempts_left_(kMaxReconnectAttempts) {
}

LibjingleStreamTransport::~LibjingleStreamTransport() {
  DCHECK(event_handler_);
  event_handler_->OnTransportDeleted(this);
  // Channel should be already destroyed if we were connected.
  DCHECK(!is_connected() || socket_.get() == NULL);

  if (channel_.get()) {
    base::ThreadTaskRunnerHandle::Get()->DeleteSoon(
        FROM_HERE, channel_.release());
  }
}

void LibjingleStreamTransport::Initialize(
    const std::string& name,
    Transport::EventHandler* event_handler,
    scoped_ptr<ChannelAuthenticator> authenticator) {
  DCHECK(CalledOnValidThread());

  DCHECK(!name.empty());
  DCHECK(event_handler);

  // Can be initialized only once.
  DCHECK(name_.empty());

  name_ = name;
  event_handler_ = event_handler;
  authenticator_ = authenticator.Pass();
}

void LibjingleStreamTransport::Connect(
    const StreamTransport::ConnectedCallback& callback) {
  DCHECK(CalledOnValidThread());

  callback_ = callback;

  DCHECK(!channel_.get());

  // Create P2PTransportChannel, attach signal handlers and connect it.
  // TODO(sergeyu): Specify correct component ID for the channel.
  channel_.reset(new cricket::P2PTransportChannel(
      std::string(), 0, NULL, port_allocator_));
  channel_->SetIceCredentials(ice_username_fragment_, ice_password_);
  channel_->SignalRequestSignaling.connect(
      this, &LibjingleStreamTransport::OnRequestSignaling);
  channel_->SignalCandidateReady.connect(
      this, &LibjingleStreamTransport::OnCandidateReady);
  channel_->SignalRouteChange.connect(
      this, &LibjingleStreamTransport::OnRouteChange);
  channel_->SignalWritableState.connect(
      this, &LibjingleStreamTransport::OnWritableState);
  channel_->set_incoming_only(incoming_only_);

  channel_->Connect();

  --connect_attempts_left_;

  // Start reconnection timer.
  reconnect_timer_.Start(
      FROM_HERE, base::TimeDelta::FromSeconds(kReconnectDelaySeconds),
      this, &LibjingleStreamTransport::TryReconnect);

  // Create net::Socket adapter for the P2PTransportChannel.
  scoped_ptr<jingle_glue::TransportChannelSocketAdapter> channel_adapter(
      new jingle_glue::TransportChannelSocketAdapter(channel_.get()));

  channel_adapter->SetOnDestroyedCallback(base::Bind(
      &LibjingleStreamTransport::OnChannelDestroyed, base::Unretained(this)));

  // Configure and connect PseudoTCP adapter.
  socket_.reset(
      new jingle_glue::PseudoTcpAdapter(channel_adapter.release()));
  socket_->SetSendBufferSize(kTcpSendBufferSize);
  socket_->SetReceiveBufferSize(kTcpReceiveBufferSize);
  socket_->SetNoDelay(true);
  socket_->SetAckDelay(kTcpAckDelayMilliseconds);

  // TODO(sergeyu): This is a hack to improve latency of the video
  // channel. Consider removing it once we have better flow control
  // implemented.
  if (name_ == kVideoChannelName)
    socket_->SetWriteWaitsForSend(true);

  int result = socket_->Connect(
      base::Bind(&LibjingleStreamTransport::OnTcpConnected,
                 base::Unretained(this)));
  if (result != net::ERR_IO_PENDING)
    OnTcpConnected(result);
}

void LibjingleStreamTransport::AddRemoteCandidate(
    const cricket::Candidate& candidate) {
  DCHECK(CalledOnValidThread());
  channel_->OnCandidate(candidate);
}

const std::string& LibjingleStreamTransport::name() const {
  DCHECK(CalledOnValidThread());
  return name_;
}

bool LibjingleStreamTransport::is_connected() const {
  DCHECK(CalledOnValidThread());
  return callback_.is_null();
}

void LibjingleStreamTransport::OnRequestSignaling(
    cricket::TransportChannelImpl* channel) {
  DCHECK(CalledOnValidThread());
  channel_->OnSignalingReady();
}

void LibjingleStreamTransport::OnCandidateReady(
    cricket::TransportChannelImpl* channel,
    const cricket::Candidate& candidate) {
  DCHECK(CalledOnValidThread());
  event_handler_->OnTransportCandidate(this, candidate);
}

void LibjingleStreamTransport::OnRouteChange(
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

void LibjingleStreamTransport::OnWritableState(
    cricket::TransportChannel* channel) {
  DCHECK_EQ(channel, channel_.get());

  event_handler_->OnTransportReady(this, channel->writable());

  if (channel->writable()) {
    channel_was_writable_ = true;
    connect_attempts_left_ = kMaxReconnectAttempts;
    reconnect_timer_.Stop();
  } else if (!channel->writable() && channel_was_writable_) {
    reconnect_timer_.Reset();
    TryReconnect();
  }
}

void LibjingleStreamTransport::OnTcpConnected(int result) {
  DCHECK(CalledOnValidThread());

  if (result != net::OK) {
    NotifyConnectFailed();
    return;
  }

  authenticator_->SecureAndAuthenticate(
      socket_.PassAs<net::StreamSocket>(),
      base::Bind(&LibjingleStreamTransport::OnAuthenticationDone,
                 base::Unretained(this)));
}

void LibjingleStreamTransport::OnAuthenticationDone(
    net::Error error,
    scoped_ptr<net::StreamSocket> socket) {
  if (error != net::OK) {
    NotifyConnectFailed();
    return;
  }

  NotifyConnected(socket.Pass());
}

void LibjingleStreamTransport::OnChannelDestroyed() {
  if (is_connected()) {
    // The connection socket is being deleted, so delete the transport too.
    delete this;
  }
}

void LibjingleStreamTransport::TryReconnect() {
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
  ice_password_ = talk_base::CreateRandomString(cricket::ICE_PWD_LENGTH);
  channel_->SetIceCredentials(ice_username_fragment_, ice_password_);
}

void LibjingleStreamTransport::NotifyConnected(
    scoped_ptr<net::StreamSocket> socket) {
  DCHECK(!is_connected());
  StreamTransport::ConnectedCallback callback = callback_;
  callback_.Reset();
  callback.Run(socket.Pass());
}

void LibjingleStreamTransport::NotifyConnectFailed() {
  DCHECK(!is_connected());

  socket_.reset();

  // This method may be called in response to a libjingle signal, so
  // libjingle objects must be deleted asynchronously.
  if (channel_.get()) {
    base::ThreadTaskRunnerHandle::Get()->DeleteSoon(
        FROM_HERE, channel_.release());
  }

  authenticator_.reset();

  NotifyConnected(scoped_ptr<net::StreamSocket>());
}

}  // namespace

scoped_ptr<LibjingleTransportFactory> LibjingleTransportFactory::Create(
    const NetworkSettings& network_settings,
    const scoped_refptr<net::URLRequestContextGetter>&
        url_request_context_getter) {
  // Use Chrome's network stack to allocate ports for peer-to-peer channels.
  scoped_ptr<ChromiumPortAllocator> port_allocator(
      ChromiumPortAllocator::Create(url_request_context_getter,
          network_settings));

  bool incoming_only = network_settings.nat_traversal_mode ==
      NetworkSettings::NAT_TRAVERSAL_DISABLED;

  // Use libjingle for negotiation of peer-to-peer channels over
  // NativePortAllocator allocated ports.
  scoped_ptr<LibjingleTransportFactory> transport_factory(
      new LibjingleTransportFactory(
          port_allocator.PassAs<cricket::HttpPortAllocatorBase>(),
          incoming_only));
  return transport_factory.Pass();
}

LibjingleTransportFactory::LibjingleTransportFactory(
    scoped_ptr<cricket::HttpPortAllocatorBase> port_allocator,
    bool incoming_only)
    : http_port_allocator_(port_allocator.get()),
      port_allocator_(port_allocator.Pass()),
      incoming_only_(incoming_only) {
  jingle_glue::JingleThreadWrapper::EnsureForCurrentMessageLoop();
}

LibjingleTransportFactory::LibjingleTransportFactory()
    : network_manager_(new talk_base::BasicNetworkManager()),
      socket_factory_(new remoting::ChromiumPacketSocketFactory()),
      http_port_allocator_(NULL),
      port_allocator_(new cricket::BasicPortAllocator(
          network_manager_.get(), socket_factory_.get())),
      incoming_only_(false) {
  jingle_glue::JingleThreadWrapper::EnsureForCurrentMessageLoop();
  port_allocator_->set_flags(
      cricket::PORTALLOCATOR_DISABLE_TCP |
      cricket::PORTALLOCATOR_DISABLE_STUN |
      cricket::PORTALLOCATOR_DISABLE_RELAY |
      cricket::PORTALLOCATOR_ENABLE_SHARED_UFRAG);
}

LibjingleTransportFactory::~LibjingleTransportFactory() {
  // This method may be called in response to a libjingle signal, so
  // libjingle objects must be deleted asynchronously.
  scoped_refptr<base::SingleThreadTaskRunner> task_runner =
      base::ThreadTaskRunnerHandle::Get();
  task_runner->DeleteSoon(FROM_HERE, port_allocator_.release());
  task_runner->DeleteSoon(FROM_HERE, socket_factory_.release());
  task_runner->DeleteSoon(FROM_HERE, network_manager_.release());
}

void LibjingleTransportFactory::SetTransportConfig(
    const TransportConfig& config) {
  if (http_port_allocator_) {
    std::vector<talk_base::SocketAddress> stun_hosts;
    talk_base::SocketAddress stun_address;
    if (stun_address.FromString(config.stun_server)) {
      stun_hosts.push_back(stun_address);
      http_port_allocator_->SetStunHosts(stun_hosts);
    } else {
      LOG(ERROR) << "Failed to parse stun server address: "
                 << config.stun_server;
    }

    std::vector<std::string> relay_hosts;
    relay_hosts.push_back(config.relay_server);
    http_port_allocator_->SetRelayHosts(relay_hosts);
    http_port_allocator_->SetRelayToken(config.relay_token);
  }
}

scoped_ptr<StreamTransport> LibjingleTransportFactory::CreateStreamTransport() {
  return scoped_ptr<StreamTransport>(
      new LibjingleStreamTransport(port_allocator_.get(), incoming_only_));
}

scoped_ptr<DatagramTransport>
LibjingleTransportFactory::CreateDatagramTransport() {
  NOTIMPLEMENTED();
  return scoped_ptr<DatagramTransport>();
}

}  // namespace protocol
}  // namespace remoting
