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

namespace remoting {
namespace protocol {

extern const char kTestJid[];

// FakeSession is a dummy protocol::Session that uses FakeStreamSocket for all
// channels.
class FakeSession : public Session {
 public:
  FakeSession();
  virtual ~FakeSession();

  EventHandler* event_handler() { return event_handler_; }

  void set_error(ErrorCode error) { error_ = error; }

  bool is_closed() const { return closed_; }

  FakeStreamChannelFactory& fake_channel_factory() { return channel_factory_; }

  // Session interface.
  virtual void SetEventHandler(EventHandler* event_handler) OVERRIDE;
  virtual ErrorCode error() OVERRIDE;
  virtual const std::string& jid() OVERRIDE;
  virtual const CandidateSessionConfig* candidate_config() OVERRIDE;
  virtual const SessionConfig& config() OVERRIDE;
  virtual void set_config(const SessionConfig& config) OVERRIDE;
  virtual StreamChannelFactory* GetTransportChannelFactory() OVERRIDE;
  virtual StreamChannelFactory* GetMultiplexedChannelFactory() OVERRIDE;
  virtual void Close() OVERRIDE;

 public:
  EventHandler* event_handler_;
  scoped_ptr<const CandidateSessionConfig> candidate_config_;
  SessionConfig config_;

  FakeStreamChannelFactory channel_factory_;

  std::string jid_;

  ErrorCode error_;
  bool closed_;

  DISALLOW_COPY_AND_ASSIGN(FakeSession);
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_FAKE_SESSION_H_
