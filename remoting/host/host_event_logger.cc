// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/host_event_logger.h"

#if defined(OS_LINUX)
#include <syslog.h>
#endif

namespace {

void Log(const std::string& message) {
  // TODO(lambroslambrou): Refactor this part into platform-specific classes.
#if defined(OS_LINUX)
  syslog(LOG_USER | LOG_NOTICE, "%s", message.c_str());
#endif
}

}  // namespace

namespace remoting {

HostEventLogger::HostEventLogger() {
#if defined(OS_LINUX)
  openlog("chromoting_host", 0, LOG_USER);
#endif
}

HostEventLogger::~HostEventLogger() {
}

void HostEventLogger::OnClientAuthenticated(const std::string& jid) {
  std::string username = jid.substr(0, jid.find('/'));
  Log("Client connected: " + username);
}

void HostEventLogger::OnClientDisconnected(const std::string& jid) {
  std::string username = jid.substr(0, jid.find('/'));
  Log("Client disconnected: " + username);
}

void HostEventLogger::OnAccessDenied() {
  Log("Access denied");
}

void HostEventLogger::OnShutdown() {
}

}  // namespace remoting
