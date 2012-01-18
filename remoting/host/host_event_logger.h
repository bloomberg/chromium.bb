// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_HOST_EVENT_LOGGER_H_
#define REMOTING_HOST_HOST_EVENT_LOGGER_H_

#include <string>

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "remoting/host/host_status_observer.h"

namespace remoting {

class ChromotingHost;

class HostEventLogger : public HostStatusObserver {
 public:
  HostEventLogger(ChromotingHost* host);
  virtual ~HostEventLogger();

  // HostStatusObserver implementation.  These methods will be called from the
  // network thread.
  virtual void OnClientAuthenticated(const std::string& jid) OVERRIDE;
  virtual void OnClientDisconnected(const std::string& jid) OVERRIDE;
  virtual void OnAccessDenied(const std::string& jid) OVERRIDE;
  virtual void OnShutdown() OVERRIDE;

 private:
  scoped_refptr<ChromotingHost> host_;

  DISALLOW_COPY_AND_ASSIGN(HostEventLogger);
};

}

#endif  // REMOTING_HOST_HOST_EVENT_LOGGER_H_
