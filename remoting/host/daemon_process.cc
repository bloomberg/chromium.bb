// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/daemon_process.h"

#include <algorithm>
#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "net/base/net_util.h"
#include "remoting/base/auto_thread_task_runner.h"
#include "remoting/host/branding.h"
#include "remoting/host/chromoting_messages.h"
#include "remoting/host/config_file_watcher.h"
#include "remoting/host/desktop_session.h"
#include "remoting/host/host_event_logger.h"
#include "remoting/host/host_status_observer.h"
#include "remoting/host/screen_resolution.h"
#include "remoting/protocol/transport.h"

namespace remoting {

namespace {

// This is used for tagging system event logs.
const char kApplicationName[] = "chromoting";

std::ostream& operator<<(std::ostream& os, const ScreenResolution& resolution) {
  return os << resolution.dimensions().width() << "x"
            << resolution.dimensions().height() << " at "
            << resolution.dpi().x() << "x" << resolution.dpi().y() << " DPI";
}

}  // namespace

DaemonProcess::~DaemonProcess() {
  DCHECK(caller_task_runner()->BelongsToCurrentThread());

  host_event_logger_.reset();
  weak_factory_.InvalidateWeakPtrs();

  config_watcher_.reset();
  DeleteAllDesktopSessions();
}

void DaemonProcess::OnConfigUpdated(const std::string& serialized_config) {
  DCHECK(caller_task_runner()->BelongsToCurrentThread());

  if (serialized_config_ != serialized_config) {
    serialized_config_ = serialized_config;
    SendToNetwork(
        new ChromotingDaemonNetworkMsg_Configuration(serialized_config_));
  }
}

void DaemonProcess::OnConfigWatcherError() {
  DCHECK(caller_task_runner()->BelongsToCurrentThread());

  Stop();
}

void DaemonProcess::AddStatusObserver(HostStatusObserver* observer) {
  DCHECK(caller_task_runner()->BelongsToCurrentThread());

  status_observers_.AddObserver(observer);
}

void DaemonProcess::RemoveStatusObserver(HostStatusObserver* observer) {
  DCHECK(caller_task_runner()->BelongsToCurrentThread());

  status_observers_.RemoveObserver(observer);
}

void DaemonProcess::OnChannelConnected(int32 peer_pid) {
  DCHECK(caller_task_runner()->BelongsToCurrentThread());

  VLOG(1) << "IPC: daemon <- network (" << peer_pid << ")";

  DeleteAllDesktopSessions();

  // Reset the last known terminal ID because no IDs have been allocated
  // by the the newly started process yet.
  next_terminal_id_ = 0;

  // Send the configuration to the network process.
  SendToNetwork(
      new ChromotingDaemonNetworkMsg_Configuration(serialized_config_));
}

bool DaemonProcess::OnMessageReceived(const IPC::Message& message) {
  DCHECK(caller_task_runner()->BelongsToCurrentThread());

  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(DaemonProcess, message)
    IPC_MESSAGE_HANDLER(ChromotingNetworkHostMsg_ConnectTerminal,
                        CreateDesktopSession)
    IPC_MESSAGE_HANDLER(ChromotingNetworkHostMsg_DisconnectTerminal,
                        CloseDesktopSession)
    IPC_MESSAGE_HANDLER(ChromotingNetworkDaemonMsg_SetScreenResolution,
                        SetScreenResolution)
    IPC_MESSAGE_HANDLER(ChromotingNetworkDaemonMsg_AccessDenied,
                        OnAccessDenied)
    IPC_MESSAGE_HANDLER(ChromotingNetworkDaemonMsg_ClientAuthenticated,
                        OnClientAuthenticated)
    IPC_MESSAGE_HANDLER(ChromotingNetworkDaemonMsg_ClientConnected,
                        OnClientConnected)
    IPC_MESSAGE_HANDLER(ChromotingNetworkDaemonMsg_ClientDisconnected,
                        OnClientDisconnected)
    IPC_MESSAGE_HANDLER(ChromotingNetworkDaemonMsg_ClientRouteChange,
                        OnClientRouteChange)
    IPC_MESSAGE_HANDLER(ChromotingNetworkDaemonMsg_HostStarted,
                        OnHostStarted)
    IPC_MESSAGE_HANDLER(ChromotingNetworkDaemonMsg_HostShutdown,
                        OnHostShutdown)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  if (!handled) {
    LOG(ERROR) << "Received unexpected IPC type: " << message.type();
    CrashNetworkProcess(FROM_HERE);
  }

  return handled;
}

void DaemonProcess::OnPermanentError(int exit_code) {
  DCHECK(caller_task_runner()->BelongsToCurrentThread());
  Stop();
}

void DaemonProcess::CloseDesktopSession(int terminal_id) {
  DCHECK(caller_task_runner()->BelongsToCurrentThread());

  // Validate the supplied terminal ID. An attempt to use a desktop session ID
  // that couldn't possibly have been allocated is considered a protocol error
  // and the network process will be restarted.
  if (!WasTerminalIdAllocated(terminal_id)) {
    LOG(ERROR) << "Invalid terminal ID: " << terminal_id;
    CrashNetworkProcess(FROM_HERE);
    return;
  }

  DesktopSessionList::iterator i;
  for (i = desktop_sessions_.begin(); i != desktop_sessions_.end(); ++i) {
    if ((*i)->id() == terminal_id) {
      break;
    }
  }

  // It is OK if the terminal ID wasn't found. There is a race between
  // the network and daemon processes. Each frees its own recources first and
  // notifies the other party if there was something to clean up.
  if (i == desktop_sessions_.end())
    return;

  delete *i;
  desktop_sessions_.erase(i);

  VLOG(1) << "Daemon: closed desktop session " << terminal_id;
  SendToNetwork(
      new ChromotingDaemonNetworkMsg_TerminalDisconnected(terminal_id));
}

DaemonProcess::DaemonProcess(
    scoped_refptr<AutoThreadTaskRunner> caller_task_runner,
    scoped_refptr<AutoThreadTaskRunner> io_task_runner,
    const base::Closure& stopped_callback)
    : caller_task_runner_(caller_task_runner),
      io_task_runner_(io_task_runner),
      next_terminal_id_(0),
      stopped_callback_(stopped_callback),
      weak_factory_(this) {
  DCHECK(caller_task_runner->BelongsToCurrentThread());
}

void DaemonProcess::CreateDesktopSession(int terminal_id,
                                         const ScreenResolution& resolution,
                                         bool virtual_terminal) {
  DCHECK(caller_task_runner()->BelongsToCurrentThread());

  // Validate the supplied terminal ID. An attempt to create a desktop session
  // with an ID that could possibly have been allocated already is considered
  // a protocol error and the network process will be restarted.
  if (WasTerminalIdAllocated(terminal_id)) {
    LOG(ERROR) << "Invalid terminal ID: " << terminal_id;
    CrashNetworkProcess(FROM_HERE);
    return;
  }

  // Terminal IDs cannot be reused. Update the expected next terminal ID.
  next_terminal_id_ = std::max(next_terminal_id_, terminal_id + 1);

  // Create the desktop session.
  scoped_ptr<DesktopSession> session = DoCreateDesktopSession(
      terminal_id, resolution, virtual_terminal);
  if (!session) {
    LOG(ERROR) << "Failed to create a desktop session.";
    SendToNetwork(
        new ChromotingDaemonNetworkMsg_TerminalDisconnected(terminal_id));
    return;
  }

  VLOG(1) << "Daemon: opened desktop session " << terminal_id;
  desktop_sessions_.push_back(session.release());
}

void DaemonProcess::SetScreenResolution(int terminal_id,
                                        const ScreenResolution& resolution) {
  DCHECK(caller_task_runner()->BelongsToCurrentThread());

  // Validate the supplied terminal ID. An attempt to use a desktop session ID
  // that couldn't possibly have been allocated is considered a protocol error
  // and the network process will be restarted.
  if (!WasTerminalIdAllocated(terminal_id)) {
    LOG(ERROR) << "Invalid terminal ID: " << terminal_id;
    CrashNetworkProcess(FROM_HERE);
    return;
  }

  // Validate |resolution| and restart the sender if it is not valid.
  if (resolution.IsEmpty()) {
    LOG(ERROR) << "Invalid resolution specified: " << resolution;
    CrashNetworkProcess(FROM_HERE);
    return;
  }

  DesktopSessionList::iterator i;
  for (i = desktop_sessions_.begin(); i != desktop_sessions_.end(); ++i) {
    if ((*i)->id() == terminal_id) {
      break;
    }
  }

  // It is OK if the terminal ID wasn't found. There is a race between
  // the network and daemon processes. Each frees its own resources first and
  // notifies the other party if there was something to clean up.
  if (i == desktop_sessions_.end())
    return;

  (*i)->SetScreenResolution(resolution);
}

void DaemonProcess::CrashNetworkProcess(
    const tracked_objects::Location& location) {
  DCHECK(caller_task_runner()->BelongsToCurrentThread());

  DoCrashNetworkProcess(location);
  DeleteAllDesktopSessions();
}

void DaemonProcess::Initialize() {
  DCHECK(caller_task_runner()->BelongsToCurrentThread());

  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();
  // Get the name of the host configuration file.
  base::FilePath default_config_dir = remoting::GetConfigDir();
  base::FilePath config_path = default_config_dir.Append(
      kDefaultHostConfigFile);
  if (command_line->HasSwitch(kHostConfigSwitchName)) {
    config_path = command_line->GetSwitchValuePath(kHostConfigSwitchName);
  }
  config_watcher_.reset(new ConfigFileWatcher(
      caller_task_runner(), io_task_runner(), config_path));
  config_watcher_->Watch(this);
  host_event_logger_ =
      HostEventLogger::Create(weak_factory_.GetWeakPtr(), kApplicationName);

  // Launch the process.
  LaunchNetworkProcess();
}

void DaemonProcess::Stop() {
  DCHECK(caller_task_runner()->BelongsToCurrentThread());

  if (!stopped_callback_.is_null()) {
    base::Closure stopped_callback = stopped_callback_;
    stopped_callback_.Reset();
    stopped_callback.Run();
  }
}

bool DaemonProcess::WasTerminalIdAllocated(int terminal_id) {
  return terminal_id < next_terminal_id_;
}

void DaemonProcess::OnAccessDenied(const std::string& jid) {
  DCHECK(caller_task_runner()->BelongsToCurrentThread());

  FOR_EACH_OBSERVER(HostStatusObserver, status_observers_, OnAccessDenied(jid));
}

void DaemonProcess::OnClientAuthenticated(const std::string& jid) {
  DCHECK(caller_task_runner()->BelongsToCurrentThread());

  FOR_EACH_OBSERVER(HostStatusObserver, status_observers_,
                    OnClientAuthenticated(jid));
}

void DaemonProcess::OnClientConnected(const std::string& jid) {
  DCHECK(caller_task_runner()->BelongsToCurrentThread());

  FOR_EACH_OBSERVER(HostStatusObserver, status_observers_,
                    OnClientConnected(jid));
}

void DaemonProcess::OnClientDisconnected(const std::string& jid) {
  DCHECK(caller_task_runner()->BelongsToCurrentThread());

  FOR_EACH_OBSERVER(HostStatusObserver, status_observers_,
                    OnClientDisconnected(jid));
}

void DaemonProcess::OnClientRouteChange(const std::string& jid,
                                        const std::string& channel_name,
                                        const SerializedTransportRoute& route) {
  DCHECK(caller_task_runner()->BelongsToCurrentThread());

  // Validate |route|.
  if (route.type != protocol::TransportRoute::DIRECT &&
      route.type != protocol::TransportRoute::STUN &&
      route.type != protocol::TransportRoute::RELAY) {
    LOG(ERROR) << "An invalid RouteType " << route.type << " passed.";
    CrashNetworkProcess(FROM_HERE);
    return;
  }
  if (route.remote_address.size() != net::kIPv4AddressSize &&
      route.remote_address.size() != net::kIPv6AddressSize) {
    LOG(ERROR) << "An invalid net::IPAddressNumber size "
               << route.remote_address.size() << " passed.";
    CrashNetworkProcess(FROM_HERE);
    return;
  }
  if (route.local_address.size() != net::kIPv4AddressSize &&
      route.local_address.size() != net::kIPv6AddressSize) {
    LOG(ERROR) << "An invalid net::IPAddressNumber size "
               << route.local_address.size() << " passed.";
    CrashNetworkProcess(FROM_HERE);
    return;
  }

  protocol::TransportRoute parsed_route;
  parsed_route.type =
      static_cast<protocol::TransportRoute::RouteType>(route.type);
  parsed_route.remote_address =
      net::IPEndPoint(route.remote_address, route.remote_port);
  parsed_route.local_address =
      net::IPEndPoint(route.local_address, route.local_port);
  FOR_EACH_OBSERVER(HostStatusObserver, status_observers_,
                    OnClientRouteChange(jid, channel_name, parsed_route));
}

void DaemonProcess::OnHostStarted(const std::string& xmpp_login) {
  DCHECK(caller_task_runner()->BelongsToCurrentThread());

  FOR_EACH_OBSERVER(HostStatusObserver, status_observers_, OnStart(xmpp_login));
}

void DaemonProcess::OnHostShutdown() {
  DCHECK(caller_task_runner()->BelongsToCurrentThread());

  FOR_EACH_OBSERVER(HostStatusObserver, status_observers_, OnShutdown());
}

void DaemonProcess::DeleteAllDesktopSessions() {
  while (!desktop_sessions_.empty()) {
    delete desktop_sessions_.front();
    desktop_sessions_.pop_front();
  }
}

}  // namespace remoting
