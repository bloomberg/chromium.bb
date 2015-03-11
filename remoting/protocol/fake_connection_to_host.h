// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_FAKE_CONNECTION_TO_HOST_H_
#define REMOTING_PROTOCOL_FAKE_CONNECTION_TO_HOST_H_

#include <string>

#include "remoting/protocol/connection_to_host.h"
#include "remoting/protocol/protocol_mock_objects.h"

namespace remoting {
namespace test {

class FakeConnectionToHost : public protocol::ConnectionToHost {
 public:
  FakeConnectionToHost();
  ~FakeConnectionToHost() override;

  // ConnectionToHost interface.
  void set_candidate_config(
      scoped_ptr<protocol::CandidateSessionConfig> config) override;
  void set_client_stub(protocol::ClientStub* client_stub) override;
  void set_clipboard_stub(protocol::ClipboardStub* clipboard_stub) override;
  void set_video_stub(protocol::VideoStub* video_stub) override;
  void set_audio_stub(protocol::AudioStub* audio_stub) override;
  void Connect(SignalStrategy* signal_strategy,
               scoped_ptr<protocol::TransportFactory> transport_factory,
               scoped_ptr<protocol::Authenticator> authenticator,
               const std::string& host_jid,
               HostEventCallback* event_callback) override;
  const protocol::SessionConfig& config() override;
  protocol::ClipboardStub* clipboard_forwarder() override;
  protocol::HostStub* host_stub() override;
  protocol::InputStub* input_stub() override;
  State state() const override;

  // Calls OnConnectionState on the |event_callback_| with the supplied state
  // and error.
  void SignalStateChange(protocol::Session::State state,
                         protocol::ErrorCode error);

  // Calls OnConnectionReady on the |event_callback_| with the supplied bool.
  void SignalConnectionReady(bool ready);

 private:
  void SetState(State state, protocol::ErrorCode error);

  State state_;

  HostEventCallback* event_callback_;

  testing::NiceMock<protocol::MockClipboardStub> mock_clipboard_stub_;
  testing::NiceMock<protocol::MockHostStub> mock_host_stub_;
  testing::NiceMock<protocol::MockInputStub> mock_input_stub_;
  protocol::SessionConfig session_config_;

  DISALLOW_COPY_AND_ASSIGN(FakeConnectionToHost);
};

}  // namespace test
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_FAKE_CONNECTION_TO_HOST_H_
