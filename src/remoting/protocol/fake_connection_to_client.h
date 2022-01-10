// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_FAKE_CONNECTION_TO_CLIENT_H_
#define REMOTING_PROTOCOL_FAKE_CONNECTION_TO_CLIENT_H_

#include <stdint.h>

#include "base/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "remoting/protocol/connection_to_client.h"
#include "remoting/protocol/video_feedback_stub.h"
#include "remoting/protocol/video_stream.h"
#include "remoting/protocol/video_stub.h"

namespace remoting {
namespace protocol {

class FakeVideoStream : public protocol::VideoStream {
 public:
  FakeVideoStream();

  FakeVideoStream(const FakeVideoStream&) = delete;
  FakeVideoStream& operator=(const FakeVideoStream&) = delete;

  ~FakeVideoStream() override;

  // protocol::VideoStream interface.
  void SetEventTimestampsSource(scoped_refptr<InputEventTimestampsSource>
                                    event_timestamps_source) override;
  void Pause(bool pause) override;
  void SetLosslessEncode(bool want_lossless) override;
  void SetLosslessColor(bool want_lossless) override;
  void SetObserver(Observer* observer) override;
  void SelectSource(int id) override;

  Observer* observer() { return observer_; }

  base::WeakPtr<FakeVideoStream> GetWeakPtr();

 private:
  raw_ptr<Observer> observer_ = nullptr;

  base::WeakPtrFactory<FakeVideoStream> weak_factory_{this};
};

class FakeConnectionToClient : public ConnectionToClient {
 public:
  FakeConnectionToClient(std::unique_ptr<Session> session);

  FakeConnectionToClient(const FakeConnectionToClient&) = delete;
  FakeConnectionToClient& operator=(const FakeConnectionToClient&) = delete;

  ~FakeConnectionToClient() override;

  void SetEventHandler(EventHandler* event_handler) override;

  std::unique_ptr<VideoStream> StartVideoStream(
      std::unique_ptr<webrtc::DesktopCapturer> desktop_capturer) override;
  std::unique_ptr<AudioStream> StartAudioStream(
      std::unique_ptr<AudioSource> audio_source) override;

  ClientStub* client_stub() override;
  void Disconnect(ErrorCode disconnect_error) override;

  Session* session() override;

  void set_clipboard_stub(ClipboardStub* clipboard_stub) override;
  void set_host_stub(HostStub* host_stub) override;
  void set_input_stub(InputStub* input_stub) override;

  PeerConnectionControls* peer_connection_controls() override;
  WebrtcEventLogData* rtc_event_log() override;

  base::WeakPtr<FakeVideoStream> last_video_stream() {
    return last_video_stream_;
  }

  void set_client_stub(ClientStub* client_stub) { client_stub_ = client_stub; }
  void set_video_stub(VideoStub* video_stub) { video_stub_ = video_stub; }
  void set_video_encode_task_runner(
      scoped_refptr<base::SingleThreadTaskRunner> runner) {
    video_encode_task_runner_ = runner;
  }

  EventHandler* event_handler() { return event_handler_; }
  ClipboardStub* clipboard_stub() { return clipboard_stub_; }
  HostStub* host_stub() { return host_stub_; }
  InputStub* input_stub() { return input_stub_; }
  VideoStub* video_stub() { return video_stub_; }
  VideoFeedbackStub* video_feedback_stub() { return video_feedback_stub_; }

  bool is_connected() { return is_connected_; }
  ErrorCode disconnect_error() { return disconnect_error_; }

 private:
  // TODO(crbug.com/1043325): Remove the requirement that ConnectionToClient
  // retains a pointer to the capturer if the relative pointer experiment is
  // a success.
  std::unique_ptr<webrtc::DesktopCapturer> desktop_capturer_;
  std::unique_ptr<Session> session_;
  raw_ptr<EventHandler> event_handler_ = nullptr;

  base::WeakPtr<FakeVideoStream> last_video_stream_;

  raw_ptr<ClientStub> client_stub_ = nullptr;

  raw_ptr<ClipboardStub> clipboard_stub_ = nullptr;
  raw_ptr<HostStub> host_stub_ = nullptr;
  raw_ptr<InputStub> input_stub_ = nullptr;
  raw_ptr<VideoStub> video_stub_ = nullptr;
  raw_ptr<VideoFeedbackStub> video_feedback_stub_ = nullptr;

  scoped_refptr<base::SingleThreadTaskRunner> video_encode_task_runner_;

  bool is_connected_ = true;
  ErrorCode disconnect_error_ = OK;
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_FAKE_CONNECTION_TO_CLIENT_H_
