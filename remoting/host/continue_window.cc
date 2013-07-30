// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/continue_window.h"

#include "base/location.h"
#include "remoting/host/client_session_control.h"

// Minutes before the local user should confirm that the session should go on.
const int kSessionExpirationTimeoutMinutes = 10;

// Minutes before the session will be disconnected (from the moment the Continue
// window has been shown).
const int kSessionDisconnectTimeoutMinutes = 1;

namespace remoting {

ContinueWindow::~ContinueWindow() {
}

void ContinueWindow::Start(
    const base::WeakPtr<ClientSessionControl>& client_session_control) {
  DCHECK(CalledOnValidThread());
  DCHECK(!client_session_control_.get());
  DCHECK(client_session_control.get());

  client_session_control_ = client_session_control;

  session_expired_timer_.Start(
      FROM_HERE, base::TimeDelta::FromMinutes(kSessionExpirationTimeoutMinutes),
      this, &ContinueWindow::OnSessionExpired);
}

void ContinueWindow::ContinueSession() {
  DCHECK(CalledOnValidThread());

  disconnect_timer_.Stop();

  if (!client_session_control_.get())
    return;

  // Hide the Continue window and resume the session.
  HideUi();
  client_session_control_->SetDisableInputs(false);

  session_expired_timer_.Start(
      FROM_HERE, base::TimeDelta::FromMinutes(kSessionExpirationTimeoutMinutes),
      this, &ContinueWindow::OnSessionExpired);
}

void ContinueWindow::DisconnectSession() {
  DCHECK(CalledOnValidThread());

  disconnect_timer_.Stop();
  if (client_session_control_.get())
    client_session_control_->DisconnectSession();
}

ContinueWindow::ContinueWindow() {
}

void ContinueWindow::OnSessionExpired() {
  DCHECK(CalledOnValidThread());

  if (!client_session_control_.get())
    return;

  // Stop the remote input while the Continue window is shown.
  client_session_control_->SetDisableInputs(true);

  // Show the Continue window and wait for the local user input.
  ShowUi();
  disconnect_timer_.Start(
      FROM_HERE, base::TimeDelta::FromMinutes(kSessionDisconnectTimeoutMinutes),
      this, &ContinueWindow::DisconnectSession);
}

}  // namespace remoting
