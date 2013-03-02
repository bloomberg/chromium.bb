// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/host_event_logger.h"

#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/stringprintf.h"
#include "net/base/ip_endpoint.h"
#include "remoting/host/host_status_monitor.h"
#include "remoting/host/host_status_observer.h"
#include "remoting/protocol/transport.h"

// Included here, since the #define for LOG_USER in syslog.h conflicts with the
// constants in base/logging.h, and this source file should use the version in
// syslog.h.
#include <syslog.h>

namespace remoting {

namespace {

class HostEventLoggerPosix : public HostEventLogger, public HostStatusObserver {
 public:
  HostEventLoggerPosix(base::WeakPtr<HostStatusMonitor> monitor,
                       const std::string& application_name);

  virtual ~HostEventLoggerPosix();

  // HostStatusObserver implementation.  These methods will be called from the
  // network thread.
  virtual void OnClientAuthenticated(const std::string& jid) OVERRIDE;
  virtual void OnClientDisconnected(const std::string& jid) OVERRIDE;
  virtual void OnAccessDenied(const std::string& jid) OVERRIDE;
  virtual void OnClientRouteChange(
      const std::string& jid,
      const std::string& channel_name,
      const protocol::TransportRoute& route) OVERRIDE;
  virtual void OnStart(const std::string& xmpp_login) OVERRIDE;
  virtual void OnShutdown() OVERRIDE;

 private:
  void Log(const std::string& message);

  base::WeakPtr<HostStatusMonitor> monitor_;
  std::string application_name_;

  DISALLOW_COPY_AND_ASSIGN(HostEventLoggerPosix);
};

} //namespace

HostEventLoggerPosix::HostEventLoggerPosix(
    base::WeakPtr<HostStatusMonitor> monitor,
    const std::string& application_name)
    : monitor_(monitor),
      application_name_(application_name) {
  openlog(application_name_.c_str(), 0, LOG_USER);
  monitor_->AddStatusObserver(this);
}

HostEventLoggerPosix::~HostEventLoggerPosix() {
  if (monitor_)
    monitor_->RemoveStatusObserver(this);
  closelog();
}

void HostEventLoggerPosix::OnClientAuthenticated(const std::string& jid) {
  Log("Client connected: " + jid);
}

void HostEventLoggerPosix::OnClientDisconnected(const std::string& jid) {
  Log("Client disconnected: " + jid);
}

void HostEventLoggerPosix::OnAccessDenied(const std::string& jid) {
  Log("Access denied for client: " + jid);
}

void HostEventLoggerPosix::OnClientRouteChange(
    const std::string& jid,
    const std::string& channel_name,
    const protocol::TransportRoute& route) {
  Log(base::StringPrintf(
      "Channel IP for client: %s ip='%s' host_ip='%s' channel='%s' "
      "connection='%s'",
      jid.c_str(), route.remote_address.ToString().c_str(),
      route.local_address.ToString().c_str(), channel_name.c_str(),
      protocol::TransportRoute::GetTypeString(route.type).c_str()));
}

void HostEventLoggerPosix::OnShutdown() {
  // TODO(rmsousa): Fix host shutdown to actually call this, and add a log line.
}

void HostEventLoggerPosix::OnStart(const std::string& xmpp_login) {
  Log("Host started for user: " + xmpp_login);
}

void HostEventLoggerPosix::Log(const std::string& message) {
  syslog(LOG_USER | LOG_NOTICE, "%s", message.c_str());
}

// static
scoped_ptr<HostEventLogger> HostEventLogger::Create(
    base::WeakPtr<HostStatusMonitor> monitor,
    const std::string& application_name) {
  return scoped_ptr<HostEventLogger>(
      new HostEventLoggerPosix(monitor, application_name));
}

}  // namespace remoting
