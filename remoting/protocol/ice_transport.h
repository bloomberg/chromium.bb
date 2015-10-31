// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_ICE_TRANSPORT_H_
#define REMOTING_PROTOCOL_ICE_TRANSPORT_H_

#include <list>
#include <map>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "remoting/protocol/datagram_channel_factory.h"
#include "remoting/protocol/ice_transport_channel.h"
#include "remoting/protocol/jingle_messages.h"
#include "remoting/protocol/transport.h"

namespace remoting {
namespace protocol {

class ChannelMultiplexer;
class PseudoTcpChannelFactory;
class SecureChannelFactory;

class IceTransport : public Transport,
                     public IceTransportChannel::Delegate,
                     public DatagramChannelFactory {
 public:
  // |port_allocator| must outlive the session.
  IceTransport(cricket::PortAllocator* port_allocator,
               const NetworkSettings& network_settings,
               TransportRole role);
  ~IceTransport() override;

  // Returns a closure that must be called before transport channels start
  // connecting .
  base::Closure GetCanStartClosure();

  // Transport interface.
  void Start(EventHandler* event_handler,
             Authenticator* authenticator) override;
  bool ProcessTransportInfo(buzz::XmlElement* transport_info) override;
  DatagramChannelFactory* GetDatagramChannelFactory() override;
  StreamChannelFactory* GetStreamChannelFactory() override;
  StreamChannelFactory* GetMultiplexedChannelFactory() override;

 private:
  typedef std::map<std::string, IceTransportChannel*> ChannelsMap;

  void OnCanStart();

  // DatagramChannelFactory interface.
  void CreateChannel(const std::string& name,
                     const ChannelCreatedCallback& callback) override;
  void CancelChannelCreation(const std::string& name) override;

  // Passes transport info to a new |channel| in case it was received before the
  // channel was created.
  void AddPendingRemoteTransportInfo(IceTransportChannel* channel);

  // IceTransportChannel::Delegate interface.
  void OnTransportIceCredentials(IceTransportChannel* transport,
                                 const std::string& ufrag,
                                 const std::string& password) override;
  void OnTransportCandidate(IceTransportChannel* transport,
                            const cricket::Candidate& candidate) override;
  void OnTransportRouteChange(IceTransportChannel* transport,
                              const TransportRoute& route) override;
  void OnTransportFailed(IceTransportChannel* transport) override;
  void OnTransportDeleted(IceTransportChannel* transport) override;

  // Creates empty |pending_transport_info_message_| and schedules timer for
  // SentTransportInfo() to sent the message later.
  void EnsurePendingTransportInfoMessage();

  // Sends transport-info message with candidates from |pending_candidates_|.
  void SendTransportInfo();

  cricket::PortAllocator* port_allocator_;
  NetworkSettings network_settings_;
  TransportRole role_;

  bool can_start_ = false;

  Transport::EventHandler* event_handler_ = nullptr;

  ChannelsMap channels_;
  scoped_ptr<PseudoTcpChannelFactory> pseudotcp_channel_factory_;
  scoped_ptr<SecureChannelFactory> secure_channel_factory_;
  scoped_ptr<ChannelMultiplexer> channel_multiplexer_;

  // Pending remote transport info received before the local channels were
  // created.
  std::list<IceTransportInfo::IceCredentials> pending_remote_ice_credentials_;
  std::list<IceTransportInfo::NamedCandidate> pending_remote_candidates_;

  scoped_ptr<IceTransportInfo> pending_transport_info_message_;
  base::OneShotTimer transport_info_timer_;

  base::WeakPtrFactory<IceTransport> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(IceTransport);
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_ICE_TRANSPORT_H_

