// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_DAEMON_PROCESS_H_
#define REMOTING_HOST_DAEMON_PROCESS_H_

#include <list>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/location.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/process.h"
#include "ipc/ipc_channel.h"
#include "ipc/ipc_channel_proxy.h"
#include "ipc/ipc_platform_file.h"
#include "remoting/base/stoppable.h"
#include "remoting/host/config_file_watcher.h"
#include "remoting/host/host_status_monitor.h"
#include "remoting/host/worker_process_ipc_delegate.h"

struct SerializedTransportRoute;

namespace remoting {

class AutoThreadTaskRunner;
class DesktopSession;
class HostEventLogger;
class HostStatusObserver;

// This class implements core of the daemon process. It manages the networking
// process running at lower privileges and maintains the list of desktop
// sessions.
class DaemonProcess
    : public Stoppable,
      public ConfigFileWatcher::Delegate,
      public HostStatusMonitor,
      public WorkerProcessIpcDelegate {
 public:
  typedef std::list<DesktopSession*> DesktopSessionList;

  virtual ~DaemonProcess();

  // Creates a platform-specific implementation of the daemon process object
  // passing relevant task runners. Public methods of this class must be called
  // on the |caller_task_runner| thread. |io_task_runner| is used to handle IPC
  // and background I/O tasks.
  static scoped_ptr<DaemonProcess> Create(
      scoped_refptr<AutoThreadTaskRunner> caller_task_runner,
      scoped_refptr<AutoThreadTaskRunner> io_task_runner,
      const base::Closure& stopped_callback);

  // ConfigFileWatcher::Delegate
  virtual void OnConfigUpdated(const std::string& serialized_config) OVERRIDE;
  virtual void OnConfigWatcherError() OVERRIDE;

  // HostStatusMonitor interface.
  virtual void AddStatusObserver(HostStatusObserver* observer) OVERRIDE;
  virtual void RemoveStatusObserver(HostStatusObserver* observer) OVERRIDE;

  // WorkerProcessIpcDelegate implementation.
  virtual void OnChannelConnected(int32 peer_pid) OVERRIDE;
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;
  virtual void OnPermanentError() OVERRIDE;

  // Sends an IPC message to the network process. The message will be dropped
  // unless the network process is connected over the IPC channel.
  virtual void SendToNetwork(IPC::Message* message) = 0;

  // Called when a desktop integration process attaches to |terminal_id|.
  // |desktop_process| is a handle of the desktop integration process.
  // |desktop_pipe| specifies the client end of the desktop pipe. Returns true
  // on success, false otherwise.
  virtual bool OnDesktopSessionAgentAttached(
      int terminal_id,
      base::ProcessHandle desktop_process,
      IPC::PlatformFileForTransit desktop_pipe) = 0;

  // Closes the desktop session identified by |terminal_id|.
  void CloseDesktopSession(int terminal_id);

 protected:
  DaemonProcess(scoped_refptr<AutoThreadTaskRunner> caller_task_runner,
                scoped_refptr<AutoThreadTaskRunner> io_task_runner,
                const base::Closure& stopped_callback);

  // Creates a desktop session and assigns a unique ID to it.
  void CreateDesktopSession(int terminal_id);

  // Requests the network process to crash.
  void CrashNetworkProcess(const tracked_objects::Location& location);

  // Reads the host configuration and launches the network process.
  void Initialize();

  // Returns true if |terminal_id| is considered to be known. I.e. it is
  // less or equal to the highest ID we have seen so far.
  bool IsTerminalIdKnown(int terminal_id);

  // Handlers for the host status notifications received from the network
  // process.
  void OnAccessDenied(const std::string& jid);
  void OnClientAuthenticated(const std::string& jid);
  void OnClientConnected(const std::string& jid);
  void OnClientDisconnected(const std::string& jid);
  void OnClientRouteChange(const std::string& jid,
                           const std::string& channel_name,
                           const SerializedTransportRoute& route);
  void OnHostStarted(const std::string& xmpp_login);
  void OnHostShutdown();

  // Stoppable implementation.
  virtual void DoStop() OVERRIDE;

  // Creates a platform-specific desktop session and assigns a unique ID to it.
  virtual scoped_ptr<DesktopSession> DoCreateDesktopSession(
      int terminal_id) = 0;

  // Launches the network process and establishes an IPC channel with it.
  virtual void LaunchNetworkProcess() = 0;

  scoped_refptr<AutoThreadTaskRunner> caller_task_runner() {
    return caller_task_runner_;
  }

  scoped_refptr<AutoThreadTaskRunner> io_task_runner() {
    return io_task_runner_;
  }

  // Let the test code analyze the list of desktop sessions.
  friend class DaemonProcessTest;
  const DesktopSessionList& desktop_sessions() const {
    return desktop_sessions_;
  }

 private:
  // Deletes all desktop sessions.
  void DeleteAllDesktopSessions();

  // Task runner on which public methods of this class must be called.
  scoped_refptr<AutoThreadTaskRunner> caller_task_runner_;

  // Handles IPC and background I/O tasks.
  scoped_refptr<AutoThreadTaskRunner> io_task_runner_;

  scoped_ptr<ConfigFileWatcher> config_watcher_;

  // The configuration file contents.
  std::string serialized_config_;

  // The list of active desktop sessions.
  DesktopSessionList desktop_sessions_;

  // The highest desktop session ID that has been seen so far.
  int next_terminal_id_;

  // Keeps track of observers receiving host status notifications.
  ObserverList<HostStatusObserver> status_observers_;

  // Writes host status updates to the system event log.
  scoped_ptr<HostEventLogger> host_event_logger_;

  base::WeakPtrFactory<DaemonProcess> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(DaemonProcess);
};

}  // namespace remoting

#endif  // REMOTING_HOST_DAEMON_PROCESS_H_
