// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/client_session.h"

#include <algorithm>

#include "base/task.h"
#include "remoting/host/user_authenticator.h"
#include "remoting/proto/auth.pb.h"
#include "remoting/proto/event.pb.h"

// The number of remote mouse events to record for the purpose of eliminating
// "echoes" detected by the local input detector. The value should be large
// enough to cope with the fact that multiple events might be injected before
// any echoes are detected.
static const unsigned int kNumRemoteMousePositions = 50;

// The number of milliseconds for which to block remote input when local input
// is received.
static const int64 kRemoteBlockTimeoutMillis = 2000;

namespace remoting {

using protocol::KeyEvent;

ClientSession::ClientSession(
    EventHandler* event_handler,
    UserAuthenticator* user_authenticator,
    scoped_refptr<protocol::ConnectionToClient> connection,
    protocol::InputStub* input_stub)
    : event_handler_(event_handler),
      user_authenticator_(user_authenticator),
      connection_(connection),
      client_jid_(connection->session()->jid()),
      input_stub_(input_stub),
      authenticated_(false),
      awaiting_continue_approval_(false),
      remote_mouse_button_state_(0) {
}

ClientSession::~ClientSession() {
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

void ClientSession::InjectKeyEvent(const KeyEvent* event, Task* done) {
  base::ScopedTaskRunner done_runner(done);
  if (authenticated_ && !ShouldIgnoreRemoteKeyboardInput(event)) {
    RecordKeyEvent(event);
    input_stub_->InjectKeyEvent(event, done_runner.Release());
  }
}

void ClientSession::InjectMouseEvent(const protocol::MouseEvent* event,
                                     Task* done) {
  base::ScopedTaskRunner done_runner(done);
  if (authenticated_ && !ShouldIgnoreRemoteMouseInput(event)) {
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
      injected_mouse_positions_.push_back(pos);
      if (injected_mouse_positions_.size() > kNumRemoteMousePositions) {
        VLOG(1) << "Injected mouse positions queue full.";
        injected_mouse_positions_.pop_front();
      }
    }
    input_stub_->InjectMouseEvent(event, done_runner.Release());
  }
}

void ClientSession::Disconnect() {
  connection_->Disconnect();
  UnpressKeys();
  authenticated_ = false;
}

void ClientSession::LocalMouseMoved(const gfx::Point& mouse_pos) {
  // If this is a genuine local input event (rather than an echo of a remote
  // input event that we've just injected), then ignore remote inputs for a
  // short time.
  std::list<gfx::Point>::iterator found_position =
      std::find(injected_mouse_positions_.begin(),
                injected_mouse_positions_.end(), mouse_pos);
  if (found_position != injected_mouse_positions_.end()) {
    // Remove it from the list, and any positions that were added before it,
    // if any.  This is because the local input monitor is assumed to receive
    // injected mouse position events in the order in which they were injected
    // (if at all).  If the position is found somewhere other than the front of
    // the queue, this would be because the earlier positions weren't
    // successfully injected (or the local input monitor might have skipped over
    // some positions), and not because the events were out-of-sequence.  These
    // spurious positions should therefore be discarded.
    injected_mouse_positions_.erase(injected_mouse_positions_.begin(),
                                    ++found_position);
  } else {
    latest_local_input_time_ = base::Time::Now();
  }
}

bool ClientSession::ShouldIgnoreRemoteMouseInput(
    const protocol::MouseEvent* event) const {
  // If the last remote input event was a click or a drag, then it's not safe
  // to block remote mouse events. For example, it might result in the host
  // missing the mouse-up event and being stuck with the button pressed.
  if (remote_mouse_button_state_ != 0)
    return false;
  // Otherwise, if the host user has not yet approved the continuation of the
  // connection, then ignore remote mouse events.
  if (awaiting_continue_approval_)
    return true;
  // Otherwise, ignore remote mouse events if the local mouse moved recently.
  int64 millis = (base::Time::Now() - latest_local_input_time_)
                 .InMilliseconds();
  if (millis < kRemoteBlockTimeoutMillis)
    return true;
  return false;
}

bool ClientSession::ShouldIgnoreRemoteKeyboardInput(
    const KeyEvent* event) const {
  // If the host user has not yet approved the continuation of the connection,
  // then all remote keyboard input is ignored, except to release keys that
  // were already pressed.
  if (awaiting_continue_approval_) {
    return event->pressed() ||
        (pressed_keys_.find(event->keycode()) == pressed_keys_.end());
  }
  return false;
}

void ClientSession::RecordKeyEvent(const KeyEvent* event) {
  if (event->pressed()) {
    pressed_keys_.insert(event->keycode());
  } else {
    pressed_keys_.erase(event->keycode());
  }
}

void ClientSession::UnpressKeys() {
  std::set<int>::iterator i;
  for (i = pressed_keys_.begin(); i != pressed_keys_.end(); ++i) {
    KeyEvent* key = new KeyEvent();
    key->set_keycode(*i);
    key->set_pressed(false);
    input_stub_->InjectKeyEvent(key, new DeleteTask<KeyEvent>(key));
  }
  pressed_keys_.clear();
}

}  // namespace remoting
