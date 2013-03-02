// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_CURTAINING_HOST_OBSERVER_H_
#define REMOTING_HOST_CURTAINING_HOST_OBSERVER_H_

#include <set>
#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "remoting/host/host_status_observer.h"

namespace remoting {

class CurtainMode;
class HostStatusMonitor;

class CurtainingHostObserver : public HostStatusObserver {
 public:
  CurtainingHostObserver(CurtainMode *curtain,
                         base::WeakPtr<HostStatusMonitor> monitor);
  virtual ~CurtainingHostObserver();

  // Enables/disables curtaining when one or more clients are connected.
  // Takes immediate effect if clients are already connected.
  void SetEnableCurtaining(bool enable);

  // HostStatusObserver interface.
  virtual void OnClientAuthenticated(const std::string& jid) OVERRIDE;
  virtual void OnClientDisconnected(const std::string& jid) OVERRIDE;

 private:
  CurtainMode* curtain_;
  base::WeakPtr<HostStatusMonitor> monitor_;
  std::set<std::string> active_clients_;
  bool enable_curtaining_;
};

}  // namespace remoting
#endif
