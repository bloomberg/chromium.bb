// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/client_session.h"

#include <algorithm>

#include "base/message_loop_proxy.h"
#include "remoting/host/capturer.h"
#include "remoting/proto/control.pb.h"
#include "remoting/proto/event.pb.h"
#include "remoting/protocol/client_stub.h"
#include "remoting/protocol/clipboard_thread_proxy.h"

namespace remoting {

ClientSession::ClientSession(
    EventHandler* event_handler,
    scoped_ptr<protocol::ConnectionToClient> connection,
    protocol::HostEventStub* host_event_stub,
    Capturer* capturer,
    const base::TimeDelta& max_duration)
    : event_handler_(event_handler),
      connection_(connection.Pass()),
      client_jid_(connection_->session()->jid()),
      is_authenticated_(false),
      host_event_stub_(host_event_stub),
      input_tracker_(host_event_stub_),
      remote_input_filter_(&input_tracker_),
      mouse_input_filter_(&remote_input_filter_),
      client_clipboard_factory_(clipboard_echo_filter_.client_filter()),
      capturer_(capturer),
      max_duration_(max_duration) {
  connection_->SetEventHandler(this);

  // TODO(sergeyu): Currently ConnectionToClient expects stubs to be
  // set before channels are connected. Make it possible to set stubs
  // later and set them only when connection is authenticated.
  connection_->set_clipboard_stub(this);
  connection_->set_host_stub(this);
  connection_->set_input_stub(this);
  clipboard_echo_filter_.set_host_stub(host_event_stub_);
}

ClientSession::~ClientSession() {
}

void ClientSession::InjectClipboardEvent(
    const protocol::ClipboardEvent& event) {
  DCHECK(CalledOnValidThread());

  // TODO(wez): Disable clipboard in both directions on local activity, and
  // replace these tests with a HostInputFilter (or ClipboardFilter).
  if (auth_input_filter_.input_stub() == NULL)
    return;
  if (disable_input_filter_.input_stub() == NULL)
    return;

  clipboard_echo_filter_.host_filter()->InjectClipboardEvent(event);
}

void ClientSession::InjectKeyEvent(const protocol::KeyEvent& event) {
  DCHECK(CalledOnValidThread());
  auth_input_filter_.InjectKeyEvent(event);
}

void ClientSession::InjectMouseEvent(const protocol::MouseEvent& event) {
  DCHECK(CalledOnValidThread());

  // Ensure that the MouseInputFilter is clamping to the current dimensions.
  mouse_input_filter_.set_output_size(capturer_->size_most_recent());
  mouse_input_filter_.set_input_size(capturer_->size_most_recent());

  auth_input_filter_.InjectMouseEvent(event);
}

void ClientSession::NotifyClientDimensions(
    const protocol::ClientDimensions& dimensions) {
  // TODO(wez): Use the dimensions, e.g. to resize the host desktop to match.
  if (dimensions.has_width() && dimensions.has_height()) {
    VLOG(1) << "Received ClientDimensions (width="
            << dimensions.width() << ", height=" << dimensions.height() << ")";
  }
}

void ClientSession::ControlVideo(const protocol::VideoControl& video_control) {
  // TODO(wez): Pause/resume video updates, being careful not to let clients
  // override any host-initiated pause of the video channel.
  if (video_control.has_enable()) {
    VLOG(1) << "Received VideoControl (enable="
            << video_control.enable() << ")";
  }
}

void ClientSession::OnConnectionAuthenticated(
    protocol::ConnectionToClient* connection) {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(connection_.get(), connection);
  is_authenticated_ = true;
  auth_input_filter_.set_input_stub(&disable_input_filter_);
  clipboard_echo_filter_.set_client_stub(connection_->client_stub());
  if (max_duration_ > base::TimeDelta()) {
    // TODO(simonmorris): Let Disconnect() tell the client that the
    // disconnection was caused by the session exceeding its maximum duration.
    max_duration_timer_.Start(FROM_HERE, max_duration_,
                              this, &ClientSession::Disconnect);
  }
  event_handler_->OnSessionAuthenticated(this);
}

void ClientSession::OnConnectionChannelsConnected(
    protocol::ConnectionToClient* connection) {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(connection_.get(), connection);
  SetDisableInputs(false);
  event_handler_->OnSessionChannelsConnected(this);
}

void ClientSession::OnConnectionClosed(
    protocol::ConnectionToClient* connection,
    protocol::ErrorCode error) {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(connection_.get(), connection);
  if (!is_authenticated_)
    event_handler_->OnSessionAuthenticationFailed(this);
  auth_input_filter_.set_input_stub(NULL);

  // Ensure that any pressed keys or buttons are released.
  input_tracker_.ReleaseAll();

  // TODO(sergeyu): Log failure reason?
  event_handler_->OnSessionClosed(this);
}

void ClientSession::OnSequenceNumberUpdated(
    protocol::ConnectionToClient* connection, int64 sequence_number) {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(connection_.get(), connection);
  event_handler_->OnSessionSequenceNumber(this, sequence_number);
}

void ClientSession::OnRouteChange(
    protocol::ConnectionToClient* connection,
    const std::string& channel_name,
    const protocol::TransportRoute& route) {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(connection_.get(), connection);
  event_handler_->OnSessionRouteChange(this, channel_name, route);
}

void ClientSession::Disconnect() {
  DCHECK(CalledOnValidThread());
  DCHECK(connection_.get());

  max_duration_timer_.Stop();
  // This triggers OnConnectionClosed(), and the session may be destroyed
  // as the result, so this call must be the last in this method.
  connection_->Disconnect();
}

void ClientSession::LocalMouseMoved(const SkIPoint& mouse_pos) {
  DCHECK(CalledOnValidThread());
  remote_input_filter_.LocalMouseMoved(mouse_pos);
}

void ClientSession::SetDisableInputs(bool disable_inputs) {
  DCHECK(CalledOnValidThread());

  if (disable_inputs) {
    disable_input_filter_.set_input_stub(NULL);
    input_tracker_.ReleaseAll();
  } else {
    disable_input_filter_.set_input_stub(&mouse_input_filter_);
  }
}

scoped_ptr<protocol::ClipboardStub> ClientSession::CreateClipboardProxy() {
  DCHECK(CalledOnValidThread());

  return scoped_ptr<protocol::ClipboardStub>(
      new protocol::ClipboardThreadProxy(
          client_clipboard_factory_.GetWeakPtr(),
          base::MessageLoopProxy::current()));
}

}  // namespace remoting
