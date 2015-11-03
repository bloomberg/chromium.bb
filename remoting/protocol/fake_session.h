// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_FAKE_SESSION_H_
#define REMOTING_PROTOCOL_FAKE_SESSION_H_

#include <map>
#include <string>
#include <vector>

#include "base/memory/scoped_ptr.h"
#include "remoting/protocol/fake_stream_socket.h"
#include "remoting/protocol/session.h"
#include "remoting/protocol/transport.h"

namespace remoting {
namespace protocol {

extern const char kTestJid[];

class FakeTransport : public Transport {
 public:
  FakeTransport();
  ~FakeTransport() override;

  // Transport interface.
  void Start(EventHandler* event_handler,
             Authenticator* authenticator) override;
  bool ProcessTransportInfo(buzz::XmlElement* transport_info) override;
  DatagramChannelFactory* GetDatagramChannelFactory() override;
  FakeStreamChannelFactory* GetStreamChannelFactory() override;
  FakeStreamChannelFactory* GetMultiplexedChannelFactory() override;

 private:
  FakeStreamChannelFactory channel_factory_;
};

// FakeSession is a dummy protocol::Session that uses FakeStreamSocket for all
// channels.
class FakeSession : public Session {
 public:
  FakeSession();
  ~FakeSession() override;

  EventHandler* event_handler() { return event_handler_; }

  void set_error(ErrorCode error) { error_ = error; }

  bool is_closed() const { return closed_; }

  // Session interface.
  void SetEventHandler(EventHandler* event_handler) override;
  ErrorCode error() override;
  const std::string& jid() override;
  const SessionConfig& config() override;
  FakeTransport* GetTransport() override;
  FakeStreamChannelFactory* GetQuicChannelFactory() override;
  void Close(ErrorCode error) override;

 public:
  EventHandler* event_handler_;
  scoped_ptr<SessionConfig> config_;

  std::string jid_;

  FakeTransport transport_;

  ErrorCode error_;
  bool closed_;

  DISALLOW_COPY_AND_ASSIGN(FakeSession);
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_FAKE_SESSION_H_
