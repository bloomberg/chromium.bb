// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_HOST_EVENT_LOGGER_H_
#define REMOTING_HOST_HOST_EVENT_LOGGER_H_

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "remoting/host/host_status_observer.h"

namespace remoting {

class ChromotingHost;
class SystemEventLogger;

class HostEventLogger : public HostStatusObserver {
 public:
  HostEventLogger(ChromotingHost* host, const std::string& application_name);
  virtual ~HostEventLogger();

  // HostStatusObserver implementation.  These methods will be called from the
  // network thread.
  virtual void OnClientAuthenticated(const std::string& jid) OVERRIDE;
  virtual void OnClientDisconnected(const std::string& jid) OVERRIDE;
  virtual void OnAccessDenied(const std::string& jid) OVERRIDE;
  virtual void OnShutdown() OVERRIDE;

 private:
  void Log(const std::string& message);

  scoped_refptr<ChromotingHost> host_;
  scoped_ptr<SystemEventLogger> system_event_logger_;

  DISALLOW_COPY_AND_ASSIGN(HostEventLogger);
};

}

#endif  // REMOTING_HOST_HOST_EVENT_LOGGER_H_
