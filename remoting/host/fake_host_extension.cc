// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/fake_host_extension.h"

#include <string>

#include "base/logging.h"
#include "remoting/codec/video_encoder.h"
#include "remoting/host/host_extension_session.h"
#include "remoting/proto/control.pb.h"
#include "third_party/webrtc/modules/desktop_capture/screen_capturer.h"

namespace remoting {

class FakeExtension::Session : public HostExtensionSession {
 public:
  Session(FakeExtension* extension, const std::string& message_type);
  virtual ~Session() {}

  virtual scoped_ptr<webrtc::ScreenCapturer> OnCreateVideoCapturer(
      scoped_ptr<webrtc::ScreenCapturer> encoder) OVERRIDE;
  virtual scoped_ptr<VideoEncoder> OnCreateVideoEncoder(
      scoped_ptr<VideoEncoder> encoder) OVERRIDE;
  virtual bool ModifiesVideoPipeline() const OVERRIDE;
  virtual bool OnExtensionMessage(
      ClientSessionControl* client_session_control,
      protocol::ClientStub* client_stub,
      const protocol::ExtensionMessage& message) OVERRIDE;

 private:
  FakeExtension* extension_;
  std::string message_type_;

  DISALLOW_COPY_AND_ASSIGN(Session);
};

FakeExtension::Session::Session(
    FakeExtension* extension, const std::string& message_type)
  : extension_(extension),
    message_type_(message_type) {
}

scoped_ptr<webrtc::ScreenCapturer>
FakeExtension::Session::OnCreateVideoCapturer(
    scoped_ptr<webrtc::ScreenCapturer> capturer) {
  extension_->has_wrapped_video_capturer_ = true;
  if (extension_->steal_video_capturer_) {
    capturer.reset();
  }
  return capturer.Pass();
}

scoped_ptr<VideoEncoder> FakeExtension::Session::OnCreateVideoEncoder(
    scoped_ptr<VideoEncoder> encoder) {
  extension_->has_wrapped_video_encoder_ = true;
  return encoder.Pass();
}

bool FakeExtension::Session::ModifiesVideoPipeline() const {
  return extension_->steal_video_capturer_;
}

bool FakeExtension::Session::OnExtensionMessage(
    ClientSessionControl* client_session_control,
    protocol::ClientStub* client_stub,
    const protocol::ExtensionMessage& message) {
  if (message.type() == message_type_) {
    extension_->has_handled_message_ = true;
    return true;
  }
  return false;
}

FakeExtension::FakeExtension(const std::string& message_type,
                             const std::string& capability)
  : message_type_(message_type),
    capability_(capability),
    steal_video_capturer_(false),
    has_handled_message_(false),
    has_wrapped_video_encoder_(false),
    has_wrapped_video_capturer_(false),
    was_instantiated_(false) {
}

FakeExtension::~FakeExtension() {
}

std::string FakeExtension::capability() const {
  return capability_;
}

scoped_ptr<HostExtensionSession> FakeExtension::CreateExtensionSession(
    ClientSessionControl* client_session_control,
    protocol::ClientStub* client_stub) {
  DCHECK(!was_instantiated());
  was_instantiated_ = true;
  scoped_ptr<HostExtensionSession> session(new Session(this, message_type_));
  return session.Pass();
}

void FakeExtension::set_steal_video_capturer(bool steal_video_capturer) {
  steal_video_capturer_ = steal_video_capturer;
}

bool FakeExtension::has_wrapped_video_encoder() {
  DCHECK(was_instantiated());
  return has_wrapped_video_encoder_;
}

bool FakeExtension::has_wrapped_video_capturer() {
  DCHECK(was_instantiated());
  return has_wrapped_video_capturer_;
}

bool FakeExtension::has_handled_message() {
  DCHECK(was_instantiated());
  return has_handled_message_;
}

} // namespace remoting
