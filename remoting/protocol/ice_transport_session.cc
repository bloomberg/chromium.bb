// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/ice_transport_session.h"

#include "remoting/protocol/channel_authenticator.h"
#include "remoting/protocol/channel_multiplexer.h"
#include "remoting/protocol/libjingle_transport_factory.h"
#include "remoting/protocol/pseudotcp_channel_factory.h"
#include "remoting/protocol/secure_channel_factory.h"
#include "remoting/protocol/stream_channel_factory.h"

namespace remoting {
namespace protocol {

// Delay after candidate creation before sending transport-info message to
// accumulate multiple candidates. This is an optimization to reduce number of
// transport-info messages.
const int kTransportInfoSendDelayMs = 20;

// Name of the multiplexed channel.
static const char kMuxChannelName[] = "mux";

IceTransportSession::IceTransportSession(
    LibjingleTransportFactory* libjingle_transport_factory)
    : libjingle_transport_factory_(libjingle_transport_factory) {}

IceTransportSession::~IceTransportSession() {
  channel_multiplexer_.reset();
  DCHECK(channels_.empty());
}

void IceTransportSession::Start(TransportSession::EventHandler* event_handler,
                                Authenticator* authenticator) {
  event_handler_ = event_handler;
  pseudotcp_channel_factory_.reset(new PseudoTcpChannelFactory(this));
  secure_channel_factory_.reset(new SecureChannelFactory(
      pseudotcp_channel_factory_.get(), authenticator));
}

bool IceTransportSession::ProcessTransportInfo(
    buzz::XmlElement* transport_info_xml) {
  IceTransportInfo transport_info;
  if (!transport_info.ParseXml(transport_info_xml))
    return false;

  for (auto it = transport_info.ice_credentials.begin();
       it != transport_info.ice_credentials.end(); ++it) {
    ChannelsMap::iterator channel = channels_.find(it->channel);
    if (channel != channels_.end()) {
      channel->second->SetRemoteCredentials(it->ufrag, it->password);
    } else {
      // Transport info was received before the channel was created.
      // This could happen due to messages being reordered on the wire.
      pending_remote_ice_credentials_.push_back(*it);
    }
  }

  for (auto it = transport_info.candidates.begin();
       it != transport_info.candidates.end(); ++it) {
    ChannelsMap::iterator channel = channels_.find(it->name);
    if (channel != channels_.end()) {
      channel->second->AddRemoteCandidate(it->candidate);
    } else {
      // Transport info was received before the channel was created.
      // This could happen due to messages being reordered on the wire.
      pending_remote_candidates_.push_back(*it);
    }
  }

  return true;
}

DatagramChannelFactory* IceTransportSession::GetDatagramChannelFactory() {
  return this;
}

StreamChannelFactory* IceTransportSession::GetStreamChannelFactory() {
  return secure_channel_factory_.get();
}

StreamChannelFactory* IceTransportSession::GetMultiplexedChannelFactory() {
  if (!channel_multiplexer_.get()) {
    channel_multiplexer_.reset(
        new ChannelMultiplexer(GetStreamChannelFactory(), kMuxChannelName));
  }
  return channel_multiplexer_.get();
}

void IceTransportSession::CreateChannel(
    const std::string& name,
    const ChannelCreatedCallback& callback) {
  DCHECK(!channels_[name]);

  scoped_ptr<Transport> channel =
      libjingle_transport_factory_->CreateTransport();
  channel->Connect(name, this, callback);
  AddPendingRemoteTransportInfo(channel.get());
  channels_[name] = channel.release();
}

void IceTransportSession::CancelChannelCreation(const std::string& name) {
  ChannelsMap::iterator it = channels_.find(name);
  if (it != channels_.end()) {
    DCHECK(!it->second->is_connected());
    delete it->second;
    DCHECK(channels_.find(name) == channels_.end());
  }
}

void IceTransportSession::AddPendingRemoteTransportInfo(Transport* channel) {
  std::list<IceTransportInfo::IceCredentials>::iterator credentials =
      pending_remote_ice_credentials_.begin();
  while (credentials != pending_remote_ice_credentials_.end()) {
    if (credentials->channel == channel->name()) {
      channel->SetRemoteCredentials(credentials->ufrag, credentials->password);
      credentials = pending_remote_ice_credentials_.erase(credentials);
    } else {
      ++credentials;
    }
  }

  std::list<IceTransportInfo::NamedCandidate>::iterator candidate =
      pending_remote_candidates_.begin();
  while (candidate != pending_remote_candidates_.end()) {
    if (candidate->name == channel->name()) {
      channel->AddRemoteCandidate(candidate->candidate);
      candidate = pending_remote_candidates_.erase(candidate);
    } else {
      ++candidate;
    }
  }
}

void IceTransportSession::OnTransportIceCredentials(
    Transport* transport,
    const std::string& ufrag,
    const std::string& password) {
  EnsurePendingTransportInfoMessage();
  pending_transport_info_message_->ice_credentials.push_back(
      IceTransportInfo::IceCredentials(transport->name(), ufrag, password));
}

void IceTransportSession::OnTransportCandidate(
    Transport* transport,
    const cricket::Candidate& candidate) {
  EnsurePendingTransportInfoMessage();
  pending_transport_info_message_->candidates.push_back(
      IceTransportInfo::NamedCandidate(transport->name(), candidate));
}

void IceTransportSession::OnTransportRouteChange(Transport* transport,
                                                 const TransportRoute& route) {
  if (event_handler_)
    event_handler_->OnTransportRouteChange(transport->name(), route);
}

void IceTransportSession::OnTransportFailed(Transport* transport) {
  event_handler_->OnTransportError(CHANNEL_CONNECTION_ERROR);
}

void IceTransportSession::OnTransportDeleted(Transport* transport) {
  ChannelsMap::iterator it = channels_.find(transport->name());
  DCHECK_EQ(it->second, transport);
  channels_.erase(it);
}

void IceTransportSession::EnsurePendingTransportInfoMessage() {
  // |transport_info_timer_| must be running iff
  // |pending_transport_info_message_| exists.
  DCHECK_EQ(pending_transport_info_message_ != nullptr,
            transport_info_timer_.IsRunning());

  if (!pending_transport_info_message_) {
    pending_transport_info_message_.reset(new IceTransportInfo());
    // Delay sending the new candidates in case we get more candidates
    // that we can send in one message.
    transport_info_timer_.Start(
        FROM_HERE, base::TimeDelta::FromMilliseconds(kTransportInfoSendDelayMs),
        this, &IceTransportSession::SendTransportInfo);
  }
}

void IceTransportSession::SendTransportInfo() {
  DCHECK(pending_transport_info_message_);
  event_handler_->OnOutgoingTransportInfo(
      pending_transport_info_message_->ToXml());
  pending_transport_info_message_.reset();
}

}  // namespace protocol
}  // namespace remoting
