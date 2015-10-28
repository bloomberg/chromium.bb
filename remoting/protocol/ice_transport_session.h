// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_ICE_TRANSPORT_SESSION_H_
#define REMOTING_PROTOCOL_ICE_TRANSPORT_SESSION_H_

#include <list>
#include <map>

#include "base/macros.h"
#include "base/timer/timer.h"
#include "remoting/protocol/datagram_channel_factory.h"
#include "remoting/protocol/jingle_messages.h"
#include "remoting/protocol/transport.h"

namespace remoting {
namespace protocol {

class ChannelMultiplexer;
class LibjingleTransportFactory;
class PseudoTcpChannelFactory;
class SecureChannelFactory;

class IceTransportSession : public TransportSession,
                            public Transport::EventHandler,
                            public DatagramChannelFactory {
 public:
  // |transport_factory| must outlive the session.
  IceTransportSession(LibjingleTransportFactory* libjingle_transport_factory);
  ~IceTransportSession() override;

  // TransportSession interface.
  void Start(TransportSession::EventHandler* event_handler,
             Authenticator* authenticator) override;
  bool ProcessTransportInfo(buzz::XmlElement* transport_info) override;
  DatagramChannelFactory* GetDatagramChannelFactory() override;
  StreamChannelFactory* GetStreamChannelFactory() override;
  StreamChannelFactory* GetMultiplexedChannelFactory() override;

 private:
  typedef std::map<std::string, Transport*> ChannelsMap;

  // DatagramChannelFactory interface.
  void CreateChannel(const std::string& name,
                     const ChannelCreatedCallback& callback) override;
  void CancelChannelCreation(const std::string& name) override;

  // Passes transport info to a new |channel| in case it was received before the
  // channel was created.
  void AddPendingRemoteTransportInfo(Transport* channel);

  // Transport::EventHandler interface.
  void OnTransportIceCredentials(Transport* transport,
                                 const std::string& ufrag,
                                 const std::string& password) override;
  void OnTransportCandidate(Transport* transport,
                            const cricket::Candidate& candidate) override;
  void OnTransportRouteChange(Transport* transport,
                              const TransportRoute& route) override;
  void OnTransportFailed(Transport* transport) override;
  void OnTransportDeleted(Transport* transport) override;

  // Creates empty |pending_transport_info_message_| and schedules timer for
  // SentTransportInfo() to sent the message later.
  void EnsurePendingTransportInfoMessage();

  // Sends transport-info message with candidates from |pending_candidates_|.
  void SendTransportInfo();

  LibjingleTransportFactory* libjingle_transport_factory_;

  TransportSession::EventHandler* event_handler_ = nullptr;

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

  DISALLOW_COPY_AND_ASSIGN(IceTransportSession);
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_ICE_TRANSPORT_SESSION_H_

