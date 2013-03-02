// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/curtaining_host_observer.h"

#include "base/logging.h"
#include "remoting/host/curtain_mode.h"
#include "remoting/host/host_status_monitor.h"

namespace remoting {

CurtainingHostObserver::CurtainingHostObserver(
    CurtainMode *curtain, base::WeakPtr<HostStatusMonitor> monitor)
    : curtain_(curtain), monitor_(monitor) {
  monitor_->AddStatusObserver(this);
}

CurtainingHostObserver::~CurtainingHostObserver() {
  if (monitor_)
    monitor_->RemoveStatusObserver(this);
  curtain_->SetActivated(false);
}

void CurtainingHostObserver::SetEnableCurtaining(bool enable) {
  enable_curtaining_ = enable;
  curtain_->SetActivated(enable_curtaining_ && !active_clients_.empty());
}

void CurtainingHostObserver::OnClientAuthenticated(const std::string& jid) {
  active_clients_.insert(jid);
  curtain_->SetActivated(enable_curtaining_ && !active_clients_.empty());
}

void CurtainingHostObserver::OnClientDisconnected(const std::string& jid) {
  active_clients_.erase(jid);
  curtain_->SetActivated(enable_curtaining_ && !active_clients_.empty());
}

}  // namespace remoting
