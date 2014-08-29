// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_FAKE_HOST_EXTENSION_H_
#define REMOTING_HOST_FAKE_HOST_EXTENSION_H_

#include <string>

#include "remoting/host/host_extension.h"

namespace remoting {

class ClientSessionControl;
class HostExtensionSession;

namespace protocol {
class ClientStub;
}

// |HostExtension| implementation that can report a specified capability, and
// reports messages matching a specified type as having been handled.
class FakeExtension : public HostExtension {
 public:
  FakeExtension(const std::string& message_type,
                const std::string& capability);
  virtual ~FakeExtension();

  // HostExtension interface.
  virtual std::string capability() const OVERRIDE;
  virtual scoped_ptr<HostExtensionSession> CreateExtensionSession(
      ClientSessionControl* client_session_control,
      protocol::ClientStub* client_stub) OVERRIDE;

  // Controls for testing.
  void set_steal_video_capturer(bool steal_video_capturer) {
    steal_video_capturer_ = steal_video_capturer;
  }

  // Accessors for testing.
  bool has_handled_message() const { return has_handled_message_; }
  bool has_wrapped_video_encoder() const { return has_wrapped_video_encoder_; }
  bool has_wrapped_video_capturer() const {
    return has_wrapped_video_capturer_;
  }
  bool was_instantiated() const { return was_instantiated_; }

 private:
  class Session;
  friend class Session;

  // The type name of extension messages that this fake consumes.
  std::string message_type_;

  // The capability this fake reports, and requires clients to support, if any.
  std::string capability_;

  // True if this extension should intercept creation of the session's video
  // capturer and consume it, preventing the video pipeline being created.
  bool steal_video_capturer_;

  // True if a message of |message_type_| has been processed by this extension.
  bool has_handled_message_;

  // True if this extension had the opportunity to modify the video pipeline.
  bool has_wrapped_video_encoder_;
  bool has_wrapped_video_capturer_;

  // True if CreateExtensionSession() was called on this extension.
  bool was_instantiated_;

  DISALLOW_COPY_AND_ASSIGN(FakeExtension);
};

} // namespace remoting

#endif  // REMOTING_HOST_FAKE_HOST_EXTENSION_H_
