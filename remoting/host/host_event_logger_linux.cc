// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/host_event_logger.h"

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "net/base/ip_endpoint.h"
#include "remoting/host/chromoting_host.h"
#include "remoting/host/host_status_observer.h"
#include "remoting/host/system_event_logger.h"

namespace remoting {

namespace {

class HostEventLoggerLinux : public HostEventLogger, public HostStatusObserver {
 public:
  HostEventLoggerLinux(ChromotingHost* host,
                       const std::string& application_name);

  virtual ~HostEventLoggerLinux();

  // HostStatusObserver implementation.  These methods will be called from the
  // network thread.
  virtual void OnClientAuthenticated(const std::string& jid) OVERRIDE;
  virtual void OnClientDisconnected(const std::string& jid) OVERRIDE;
  virtual void OnAccessDenied(const std::string& jid) OVERRIDE;
  virtual void OnClientRouteChange(
      const std::string& jid,
      const std::string& channel_name,
      const net::IPEndPoint& remote_end_point,
      const net::IPEndPoint& local_end_point) OVERRIDE;
  virtual void OnShutdown() OVERRIDE;

 private:
  void Log(const std::string& message);

  scoped_refptr<ChromotingHost> host_;
  scoped_ptr<SystemEventLogger> system_event_logger_;

  DISALLOW_COPY_AND_ASSIGN(HostEventLoggerLinux);
};

} //namespace

HostEventLoggerLinux::HostEventLoggerLinux(ChromotingHost* host,
                                           const std::string& application_name)
    : host_(host),
      system_event_logger_(SystemEventLogger::Create(application_name)) {
  host_->AddStatusObserver(this);
}

HostEventLoggerLinux::~HostEventLoggerLinux() {
  host_->RemoveStatusObserver(this);
}

void HostEventLoggerLinux::OnClientAuthenticated(const std::string& jid) {
  Log("Client connected: " + jid);
}

void HostEventLoggerLinux::OnClientDisconnected(const std::string& jid) {
  Log("Client disconnected: " + jid);
}

void HostEventLoggerLinux::OnAccessDenied(const std::string& jid) {
  Log("Access denied for client: " + jid);
}

void HostEventLoggerLinux::OnClientRouteChange(
    const std::string& jid,
    const std::string& channel_name,
    const net::IPEndPoint& remote_end_point,
    const net::IPEndPoint& local_end_point) {
  Log("Channel IP for client: " + jid +
      " ip='" + remote_end_point.ToString() +
      "' host_ip='" + local_end_point.ToString() +
      "' channel='" + channel_name + "'");
}

void HostEventLoggerLinux::OnShutdown() {
}

void HostEventLoggerLinux::Log(const std::string& message) {
  system_event_logger_->Log(message);
}

// static
scoped_ptr<HostEventLogger> HostEventLogger::Create(
    ChromotingHost* host, const std::string& application_name) {
  return scoped_ptr<HostEventLogger>(
      new HostEventLoggerLinux(host, application_name));
}

}  // namespace remoting
