// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_IPC_HOST_EVENT_LOGGER_H_
#define REMOTING_HOST_IPC_HOST_EVENT_LOGGER_H_

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/non_thread_safe.h"
#include "remoting/host/host_event_logger.h"
#include "remoting/host/host_status_observer.h"

namespace IPC {
class Sender;
}  // namespace IPC

namespace remoting {

class HostStatusMonitor;

class IpcHostEventLogger
    : public base::NonThreadSafe,
      public HostEventLogger,
      public HostStatusObserver {
 public:
  // Initializes the logger. |daemon_channel| must outlive this object.
  IpcHostEventLogger(base::WeakPtr<HostStatusMonitor> monitor,
                     IPC::Sender* daemon_channel);
  virtual ~IpcHostEventLogger();

  // HostStatusObserver interface.
  virtual void OnAccessDenied(const std::string& jid) OVERRIDE;
  virtual void OnClientAuthenticated(const std::string& jid) OVERRIDE;
  virtual void OnClientConnected(const std::string& jid) OVERRIDE;
  virtual void OnClientDisconnected(const std::string& jid) OVERRIDE;
  virtual void OnClientRouteChange(
      const std::string& jid,
      const std::string& channel_name,
      const protocol::TransportRoute& route) OVERRIDE;
  virtual void OnStart(const std::string& xmpp_login) OVERRIDE;
  virtual void OnShutdown() OVERRIDE;

 private:
  // Used to report host status events to the daemon.
  IPC::Sender* daemon_channel_;

  base::WeakPtr<HostStatusMonitor> monitor_;

  DISALLOW_COPY_AND_ASSIGN(IpcHostEventLogger);
};

}

#endif  // REMOTING_HOST_IPC_HOST_EVENT_LOGGER_H_
