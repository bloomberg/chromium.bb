// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_STATUS_OBSERVER_H_
#define REMOTING_HOST_STATUS_OBSERVER_H_

#include "base/memory/ref_counted.h"

namespace remoting {

class SignalStrategy;

class HostStatusObserver
    : public base::RefCountedThreadSafe<HostStatusObserver> {
 public:
  HostStatusObserver() { }

  // Called on the network thread when status of the XMPP changes.
  virtual void OnSignallingConnected(SignalStrategy* signal_strategy,
                                     const std::string& full_jid) = 0;
  virtual void OnSignallingDisconnected() = 0;

  // Called on the main thread when a client authenticates, or disconnects.
  // The observer must not tear-down ChromotingHost state on receipt of
  // this callback; it is purely informational.
  virtual void OnAuthenticatedClientsChanged(int authenticated_clients) = 0;

  // Called on the main thread when the host shuts down.
  virtual void OnShutdown() = 0;

 protected:
  friend class base::RefCountedThreadSafe<HostStatusObserver>;
  virtual ~HostStatusObserver() { }
};

}  // namespace remoting

#endif  // REMOTING_HOST_STATUS_OBSERVER_H_
