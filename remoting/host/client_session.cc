// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/client_session.h"

#include "base/task.h"
#include "remoting/host/user_authenticator.h"
#include "remoting/proto/auth.pb.h"
#include "remoting/proto/event.pb.h"

// The number of remote mouse events to record for the purpose of eliminating
// "echoes" detected by the local input detector. The value should be large
// enough to cope with the fact that multiple events might be injected before
// any echoes are detected.
static const unsigned int kNumRemoteMousePositions = 10;

// The number of milliseconds for which to block remote input when local input
// is received.
static const int64 kRemoteBlockTimeoutMillis = 2000;

namespace remoting {

ClientSession::ClientSession(
    EventHandler* event_handler,
    UserAuthenticator* user_authenticator,
    scoped_refptr<protocol::ConnectionToClient> connection,
    protocol::InputStub* input_stub)
    : event_handler_(event_handler),
      user_authenticator_(user_authenticator),
      connection_(connection),
      input_stub_(input_stub),
      authenticated_(false),
      remote_mouse_button_state_(0) {
}

ClientSession::~ClientSession() {
}

void ClientSession::SuggestResolution(
    const protocol::SuggestResolutionRequest* msg, Task* done) {
  base::ScopedTaskRunner done_runner(done);

  if (!authenticated_) {
    LOG(WARNING) << "Invalid control message received "
                 << "(client not authenticated).";
    return;
  }
}

void ClientSession::BeginSessionRequest(
    const protocol::LocalLoginCredentials* credentials, Task* done) {
  DCHECK(event_handler_);

  base::ScopedTaskRunner done_runner(done);

  bool success = false;
  switch (credentials->type()) {
    case protocol::PASSWORD:
      success = user_authenticator_->Authenticate(credentials->username(),
                                                  credentials->credential());
      break;

    default:
      LOG(ERROR) << "Invalid credentials type " << credentials->type();
      break;
  }

  OnAuthorizationComplete(success);
}

void ClientSession::OnAuthorizationComplete(bool success) {
  if (success) {
    authenticated_ = true;
    event_handler_->LocalLoginSucceeded(connection_.get());
  } else {
    LOG(WARNING) << "Login failed";
    event_handler_->LocalLoginFailed(connection_.get());
  }
}

void ClientSession::InjectKeyEvent(const protocol::KeyEvent* event,
                                   Task* done) {
  base::ScopedTaskRunner done_runner(done);
  if (authenticated_) {
    input_stub_->InjectKeyEvent(event, done_runner.Release());
  }
}

void ClientSession::InjectMouseEvent(const protocol::MouseEvent* event,
                                     Task* done) {
  base::ScopedTaskRunner done_runner(done);
  if (authenticated_ && !ShouldIgnoreRemoteInput()) {
    if (event->has_button() && event->has_button_down()) {
      if (event->button() >= 1 && event->button() < 32) {
        uint32 button_change = 1 << (event->button() - 1);
        if (event->button_down()) {
          remote_mouse_button_state_ |= button_change;
        } else {
          remote_mouse_button_state_ &= ~button_change;
        }
      }
    }
    if (event->has_x() && event->has_y()) {
      gfx::Point pos(event->x(), event->y());
      recent_remote_mouse_positions_.push_back(pos);
      if (recent_remote_mouse_positions_.size() > kNumRemoteMousePositions) {
        recent_remote_mouse_positions_.pop_front();
      }
    }
    input_stub_->InjectMouseEvent(event, done_runner.Release());
  }
}

void ClientSession::Disconnect() {
  connection_->Disconnect();
  authenticated_ = false;
}

void ClientSession::LocalMouseMoved(const gfx::Point& mouse_pos) {
  // If this is a genuine local input event (rather than an echo of a remote
  // input event that we've just injected), then ignore remote inputs for a
  // short time.
  if (!recent_remote_mouse_positions_.empty() &&
      mouse_pos == *recent_remote_mouse_positions_.begin()) {
    recent_remote_mouse_positions_.pop_front();
  } else {
    latest_local_input_time_ = base::Time::Now();
  }
}

bool ClientSession::ShouldIgnoreRemoteInput() const {
  // If the last remote input event was a click or a drag, then it's not safe
  // to block remote input. For example, it might result in the host missing
  // the mouse-up event and being stuck with the button pressed.
  if (remote_mouse_button_state_ != 0)
    return false;
  int64 millis = (base::Time::Now() - latest_local_input_time_)
                 .InMilliseconds();
  if (millis < kRemoteBlockTimeoutMillis)
    return true;
  return false;
}

}  // namespace remoting
