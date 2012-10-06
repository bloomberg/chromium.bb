// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/curtaining_host_observer.h"

#include "base/logging.h"
#include "remoting/host/curtain_mode.h"
#include "remoting/host/chromoting_host.h"

namespace remoting {

CurtainingHostObserver::CurtainingHostObserver(
    CurtainMode *curtain, scoped_refptr<ChromotingHost> host)
    : curtain_(curtain), host_(host) {
  host_->AddStatusObserver(this);
}

CurtainingHostObserver::~CurtainingHostObserver() {
  host_->RemoveStatusObserver(this);
  curtain_->SetActivated(false);
}

// TODO(jamiewalch): This code assumes at most one client connection at a time.
// Add OnFirstClientConnected and OnLastClientDisconnected optional callbacks
// to the HostStatusObserver interface to address this.
void CurtainingHostObserver::OnClientAuthenticated(const std::string& jid) {
  if (curtain_->required()) {
    curtain_->SetActivated(true);
  }
}

void CurtainingHostObserver::OnClientDisconnected(const std::string& jid) {
  curtain_->SetActivated(false);
}

}  // namespace remoting
