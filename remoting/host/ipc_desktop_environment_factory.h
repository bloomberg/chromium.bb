// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_IPC_DESKTOP_ENVIRONMENT_FACTORY_H_
#define REMOTING_HOST_IPC_DESKTOP_ENVIRONMENT_FACTORY_H_

#include <map>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "remoting/host/desktop_environment_factory.h"
#include "remoting/host/desktop_session_connector.h"

namespace IPC {
class ChannelProxy;
}  // namespace IPC

namespace remoting {

class DesktopSessionConnector;
class DesktopSessionProxy;

// Used to create IpcDesktopEnvironment objects intergating with the desktop via
// a helper process and talking to that process via IPC.
class IpcDesktopEnvironmentFactory
    : public DesktopEnvironmentFactory,
      public DesktopSessionConnector {
 public:
  // Passes a reference to the IPC channel connected to the daemon process and
  // relevant task runners. |daemon_channel| must outlive this object.
  IpcDesktopEnvironmentFactory(
      IPC::ChannelProxy* daemon_channel,
      scoped_refptr<base::SingleThreadTaskRunner> audio_capture_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> input_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> network_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> video_capture_task_runner);
  virtual ~IpcDesktopEnvironmentFactory();

  virtual scoped_ptr<DesktopEnvironment> Create() OVERRIDE;

  // DesktopSessionConnector implementation.
  virtual void ConnectTerminal(
      scoped_refptr<DesktopSessionProxy> desktop_session_proxy) OVERRIDE;
  virtual void DisconnectTerminal(
      scoped_refptr<DesktopSessionProxy> desktop_session_proxy) OVERRIDE;
  virtual void OnDesktopSessionAgentAttached(
      int terminal_id,
      IPC::PlatformFileForTransit desktop_process,
      IPC::PlatformFileForTransit desktop_pipe) OVERRIDE;
  virtual void OnTerminalDisconnected(int terminal_id) OVERRIDE;

 private:
  // IPC channel connected to the daemon process.
  IPC::ChannelProxy* daemon_channel_;

  // Task runner used to run the audio capturer.
  scoped_refptr<base::SingleThreadTaskRunner> audio_capture_task_runner_;

  // Task runner used to service calls to the DesktopSessionConnector APIs.
  scoped_refptr<base::SingleThreadTaskRunner> network_task_runner_;

  // Task runner used to run the video capturer.
  scoped_refptr<base::SingleThreadTaskRunner> video_capture_task_runner_;

  // List of DesktopEnvironment instances we've told the daemon process about.
  typedef std::map<int, scoped_refptr<DesktopSessionProxy> >
      ActiveConnectionsList;
  ActiveConnectionsList active_connections_;

  // Next desktop session ID. IDs are allocated sequentially starting from 0.
  // This gives us more than 67 years of unique IDs assuming a new ID is
  // allocated every second.
  int next_id_;

  DISALLOW_COPY_AND_ASSIGN(IpcDesktopEnvironmentFactory);
};

}  // namespace remoting

#endif  // REMOTING_HOST_IPC_DESKTOP_ENVIRONMENT_FACTORY_H_
