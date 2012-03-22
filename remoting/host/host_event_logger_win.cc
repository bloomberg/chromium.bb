// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/host_event_logger.h"

#include <windows.h>
#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/string16.h"
#include "base/utf_string_conversions.h"
#include "net/base/ip_endpoint.h"
#include "remoting/host/chromoting_host.h"
#include "remoting/host/host_status_observer.h"
#include "remoting/protocol/transport.h"

#include "remoting_host_messages.h"

namespace remoting {

namespace {

class HostEventLoggerWin : public HostEventLogger, public HostStatusObserver {
 public:
  HostEventLoggerWin(ChromotingHost* host,
                     const std::string& application_name);

  virtual ~HostEventLoggerWin();

  // HostStatusObserver implementation.  These methods will be called from the
  // network thread.
  virtual void OnClientAuthenticated(const std::string& jid) OVERRIDE;
  virtual void OnClientDisconnected(const std::string& jid) OVERRIDE;
  virtual void OnAccessDenied(const std::string& jid) OVERRIDE;
  virtual void OnClientRouteChange(
      const std::string& jid,
      const std::string& channel_name,
      const protocol::TransportRoute& route) OVERRIDE;
  virtual void OnShutdown() OVERRIDE;

 private:
  void LogString(WORD type, DWORD event_id, const std::string& string);
  void Log(WORD type, DWORD event_id, const std::vector<string16>& strings);

  scoped_refptr<ChromotingHost> host_;

  // The handle of the application event log.
  HANDLE event_log_;

  DISALLOW_COPY_AND_ASSIGN(HostEventLoggerWin);
};

} //namespace

HostEventLoggerWin::HostEventLoggerWin(ChromotingHost* host,
                                       const std::string& application_name)
    : host_(host),
      event_log_(NULL) {
  event_log_ = RegisterEventSourceW(NULL,
                                    ASCIIToUTF16(application_name).c_str());
  if (event_log_ != NULL) {
    host_->AddStatusObserver(this);
  } else {
    LOG_GETLASTERROR(ERROR) << "Failed to register the event source: "
                            << application_name;
  }
}

HostEventLoggerWin::~HostEventLoggerWin() {
  if (event_log_ != NULL) {
    host_->RemoveStatusObserver(this);
    DeregisterEventSource(event_log_);
  }
}

void HostEventLoggerWin::OnClientAuthenticated(const std::string& jid) {
  LogString(EVENTLOG_INFORMATION_TYPE, MSG_HOST_CLIENT_CONNECTED, jid);
}

void HostEventLoggerWin::OnClientDisconnected(const std::string& jid) {
  LogString(EVENTLOG_INFORMATION_TYPE, MSG_HOST_CLIENT_DISCONNECTED, jid);
}

void HostEventLoggerWin::OnAccessDenied(const std::string& jid) {
  LogString(EVENTLOG_ERROR_TYPE, MSG_HOST_CLIENT_ACCESS_DENIED, jid);
}

void HostEventLoggerWin::OnClientRouteChange(
    const std::string& jid,
    const std::string& channel_name,
    const protocol::TransportRoute& route) {
  std::vector<string16> strings(5);
  strings[0] = ASCIIToUTF16(jid);
  strings[1] = ASCIIToUTF16(route.remote_address.ToString());
  strings[2] = ASCIIToUTF16(route.local_address.ToString());
  strings[3] = ASCIIToUTF16(channel_name);
  strings[4] = ASCIIToUTF16(
      protocol::TransportRoute::GetTypeString(route.type));
  Log(EVENTLOG_INFORMATION_TYPE, MSG_HOST_CLIENT_ROUTING_CHANGED, strings);
}

void HostEventLoggerWin::OnShutdown() {
}

void HostEventLoggerWin::Log(WORD type,
                             DWORD event_id,
                             const std::vector<string16>& strings) {
  if (event_log_ == NULL)
    return;

  // ReportEventW() takes an array of raw string pointers. They should stay
  // valid for the duration of the call.
  std::vector<const WCHAR*> raw_strings(strings.size());
  for (size_t i = 0; i < strings.size(); ++i) {
    raw_strings[i] = strings[i].c_str();
  }

  if (!ReportEventW(event_log_,
                    type,
                    HOST_CATEGORY,
                    event_id,
                    NULL,
                    static_cast<WORD>(raw_strings.size()),
                    0,
                    &raw_strings[0],
                    NULL)) {
    LOG_GETLASTERROR(ERROR) << "Failed to write an event to the event log";
  }
}

void HostEventLoggerWin::LogString(WORD type,
                                   DWORD event_id,
                                   const std::string& string) {
  std::vector<string16> strings;
  strings.push_back(ASCIIToUTF16(string));
  Log(type, event_id, strings);
}

// static
scoped_ptr<HostEventLogger> HostEventLogger::Create(
    ChromotingHost* host, const std::string& application_name) {
  return scoped_ptr<HostEventLogger>(
      new HostEventLoggerWin(host, application_name));
}

}  // namespace remoting
