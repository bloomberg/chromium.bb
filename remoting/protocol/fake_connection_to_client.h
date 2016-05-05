// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_FAKE_CONNECTION_TO_CLIENT_H_
#define REMOTING_PROTOCOL_FAKE_CONNECTION_TO_CLIENT_H_

#include <stdint.h>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "remoting/protocol/connection_to_client.h"
#include "remoting/protocol/video_feedback_stub.h"
#include "remoting/protocol/video_stream.h"
#include "remoting/protocol/video_stub.h"

namespace remoting {
namespace protocol {

class FakeVideoStream : public protocol::VideoStream {
 public:
  FakeVideoStream();
  ~FakeVideoStream() override;

  // protocol::VideoStream interface.
  void Pause(bool pause) override;
  void OnInputEventReceived(int64_t event_timestamp) override;
  void SetLosslessEncode(bool want_lossless) override;
  void SetLosslessColor(bool want_lossless) override;
  void SetSizeCallback(const SizeCallback& size_callback) override;

  const SizeCallback& size_callback() { return size_callback_; }

  base::WeakPtr<FakeVideoStream> GetWeakPtr();

 private:
  SizeCallback size_callback_;

  base::WeakPtrFactory<FakeVideoStream> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(FakeVideoStream);
};

class FakeConnectionToClient : public ConnectionToClient {
 public:
  FakeConnectionToClient(std::unique_ptr<Session> session);
  ~FakeConnectionToClient() override;

  void SetEventHandler(EventHandler* event_handler) override;

  std::unique_ptr<VideoStream> StartVideoStream(
      std::unique_ptr<webrtc::DesktopCapturer> desktop_capturer) override;

  AudioStub* audio_stub() override;
  ClientStub* client_stub() override;
  void Disconnect(ErrorCode disconnect_error) override;

  Session* session() override;
  void OnInputEventReceived(int64_t timestamp) override;

  void set_clipboard_stub(ClipboardStub* clipboard_stub) override;
  void set_host_stub(HostStub* host_stub) override;
  void set_input_stub(InputStub* input_stub) override;

  base::WeakPtr<FakeVideoStream> last_video_stream() {
    return last_video_stream_;
  }

  void set_audio_stub(AudioStub* audio_stub) { audio_stub_ = audio_stub; }
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
  std::unique_ptr<Session> session_;
  EventHandler* event_handler_ = nullptr;

  base::WeakPtr<FakeVideoStream> last_video_stream_;

  AudioStub* audio_stub_ = nullptr;
  ClientStub* client_stub_ = nullptr;

  ClipboardStub* clipboard_stub_ = nullptr;
  HostStub* host_stub_ = nullptr;
  InputStub* input_stub_ = nullptr;
  VideoStub* video_stub_ = nullptr;
  VideoFeedbackStub* video_feedback_stub_ = nullptr;

  scoped_refptr<base::SingleThreadTaskRunner> video_encode_task_runner_;

  bool is_connected_ = true;
  ErrorCode disconnect_error_ = OK;

  DISALLOW_COPY_AND_ASSIGN(FakeConnectionToClient);
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_FAKE_CONNECTION_TO_CLIENT_H_
