// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/client_session.h"

#include <algorithm>

#include "base/message_loop_proxy.h"
#include "base/task.h"
#include "remoting/host/capturer.h"
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
using protocol::MouseEvent;

ClientSession::ClientSession(
    EventHandler* event_handler,
    protocol::ConnectionToClient* connection,
    protocol::InputStub* input_stub,
    Capturer* capturer)
    : event_handler_(event_handler),
      connection_(connection),
      client_jid_(connection->session()->jid()),
      input_stub_(input_stub),
      capturer_(capturer),
      authenticated_(false),
      awaiting_continue_approval_(false),
      remote_mouse_button_state_(0) {
  connection_->SetEventHandler(this);

  // TODO(sergeyu): Currently ConnectionToClient expects stubs to be
  // set before channels are connected. Make it possible to set stubs
  // later and set them only when connection is authenticated.
  connection_->set_host_stub(this);
  connection_->set_input_stub(this);
}

ClientSession::~ClientSession() {
}

void ClientSession::InjectKeyEvent(const KeyEvent& event) {
  DCHECK(CalledOnValidThread());

  if (authenticated_ && !ShouldIgnoreRemoteKeyboardInput(event)) {
    RecordKeyEvent(event);
    input_stub_->InjectKeyEvent(event);
  }
}

void ClientSession::InjectMouseEvent(const MouseEvent& event) {
  DCHECK(CalledOnValidThread());

  if (authenticated_ && !ShouldIgnoreRemoteMouseInput(event)) {
    RecordMouseButtonState(event);
    MouseEvent event_to_inject = event;
    if (event.has_x() && event.has_y()) {
      // In case the client sends events with off-screen coordinates, modify
      // the event to lie within the current screen area.  This is better than
      // simply discarding the event, which might lose a button-up event at the
      // end of a drag'n'drop (or cause other related problems).
      SkIPoint pos(SkIPoint::Make(event.x(), event.y()));
      const SkISize& screen = capturer_->size_most_recent();
      pos.setX(std::max(0, std::min(screen.width() - 1, pos.x())));
      pos.setY(std::max(0, std::min(screen.height() - 1, pos.y())));
      event_to_inject.set_x(pos.x());
      event_to_inject.set_y(pos.y());

      // Record the mouse position so we can use it if we need to inject
      // fake mouse button events. Note that we need to do this after we
      // clamp the values to the screen area.
      remote_mouse_pos_ = pos;

      injected_mouse_positions_.push_back(pos);
      if (injected_mouse_positions_.size() > kNumRemoteMousePositions) {
        VLOG(1) << "Injected mouse positions queue full.";
        injected_mouse_positions_.pop_front();
      }
    }
    input_stub_->InjectMouseEvent(event_to_inject);
  }
}

void ClientSession::OnConnectionOpened(
    protocol::ConnectionToClient* connection) {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(connection_.get(), connection);
  authenticated_ = true;
  event_handler_->OnSessionAuthenticated(this);
}

void ClientSession::OnConnectionClosed(
    protocol::ConnectionToClient* connection) {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(connection_.get(), connection);
  event_handler_->OnSessionClosed(this);
}

void ClientSession::OnConnectionFailed(
    protocol::ConnectionToClient* connection,
    protocol::Session::Error error) {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(connection_.get(), connection);
  if (error == protocol::Session::AUTHENTICATION_FAILED)
    event_handler_->OnSessionAuthenticationFailed(this);
  // TODO(sergeyu): Log failure reason?
  event_handler_->OnSessionClosed(this);
}

void ClientSession::OnSequenceNumberUpdated(
    protocol::ConnectionToClient* connection, int64 sequence_number) {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(connection_.get(), connection);
  event_handler_->OnSessionSequenceNumber(this, sequence_number);
}

void ClientSession::Disconnect() {
  DCHECK(CalledOnValidThread());
  DCHECK(connection_.get());
  authenticated_ = false;
  RestoreEventState();

  // This triggers OnSessionClosed() and the session may be destroyed
  // as the result, so this call must be the last in this method.
  connection_->Disconnect();
}

void ClientSession::LocalMouseMoved(const SkIPoint& mouse_pos) {
  DCHECK(CalledOnValidThread());

  // If this is a genuine local input event (rather than an echo of a remote
  // input event that we've just injected), then ignore remote inputs for a
  // short time.
  std::list<SkIPoint>::iterator found_position =
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
    const protocol::MouseEvent& event) const {
  DCHECK(CalledOnValidThread());

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
    const KeyEvent& event) const {
  DCHECK(CalledOnValidThread());

  // If the host user has not yet approved the continuation of the connection,
  // then all remote keyboard input is ignored, except to release keys that
  // were already pressed.
  if (awaiting_continue_approval_) {
    return event.pressed() ||
        (pressed_keys_.find(event.keycode()) == pressed_keys_.end());
  }
  return false;
}

void ClientSession::RecordKeyEvent(const KeyEvent& event) {
  DCHECK(CalledOnValidThread());

  if (event.pressed()) {
    pressed_keys_.insert(event.keycode());
  } else {
    pressed_keys_.erase(event.keycode());
  }
}

void ClientSession::RecordMouseButtonState(const MouseEvent& event) {
  DCHECK(CalledOnValidThread());

  if (event.has_button() && event.has_button_down()) {
    // Button values are defined in remoting/proto/event.proto.
    if (event.button() >= 1 && event.button() < MouseEvent::BUTTON_MAX) {
      uint32 button_change = 1 << (event.button() - 1);
      if (event.button_down()) {
        remote_mouse_button_state_ |= button_change;
      } else {
        remote_mouse_button_state_ &= ~button_change;
      }
    }
  }
}

void ClientSession::RestoreEventState() {
  DCHECK(CalledOnValidThread());

  // Undo any currently pressed keys.
  std::set<int>::iterator i;
  for (i = pressed_keys_.begin(); i != pressed_keys_.end(); ++i) {
    KeyEvent key;
    key.set_keycode(*i);
    key.set_pressed(false);
    input_stub_->InjectKeyEvent(key);
  }
  pressed_keys_.clear();

  // Undo any currently pressed mouse buttons.
  for (int i = 1; i < MouseEvent::BUTTON_MAX; i++) {
    if (remote_mouse_button_state_ & (1 << (i - 1))) {
      MouseEvent mouse;
      mouse.set_x(remote_mouse_pos_.x());
      mouse.set_y(remote_mouse_pos_.y());
      mouse.set_button((MouseEvent::MouseButton)i);
      mouse.set_button_down(false);
      input_stub_->InjectMouseEvent(mouse);
    }
  }
  remote_mouse_button_state_ = 0;
}

}  // namespace remoting
