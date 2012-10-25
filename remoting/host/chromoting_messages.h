// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Defines IPC messages used by Chromoting components.

// Multiply-included message file, no traditional include guard.
#include "ipc/ipc_message_macros.h"
#include "ipc/ipc_platform_file.h"

#define IPC_MESSAGE_START ChromotingMsgStart

//-----------------------------------------------------------------------------
// Chromoting messages sent from the daemon to the network process.

// Requests the network process to crash producing a crash dump. The daemon
// sends this message when a fatal error has been detected indicating that
// the network process misbehaves. The daemon passes the location of the code
// that detected the error.
IPC_MESSAGE_CONTROL3(ChromotingDaemonNetworkMsg_Crash,
                     std::string /* function_name */,
                     std::string /* file_name */,
                     int /* line_number */)

// Delivers the host configuration (and updates) to the network process.
IPC_MESSAGE_CONTROL1(ChromotingDaemonNetworkMsg_Configuration, std::string)

// Notifies the network process that the terminal |terminal_id| has been
// disconnected from the desktop session.
IPC_MESSAGE_CONTROL1(ChromotingDaemonNetworkMsg_TerminalDisconnected,
                     int /* terminal_id */)

// Notifies the network process that |terminal_id| is now attached to
// a desktop integration process. |desktop_process| is the handle of the desktop
// process |desktop_pipe| is the client end of the desktop-to-network pipe
// opened.
//
// Windows only: |desktop_pipe| has to be duplicated from the desktop process
// by the receiver of the message. |desktop_process| is already duplicated by
// the sender.
IPC_MESSAGE_CONTROL3(ChromotingDaemonNetworkMsg_DesktopAttached,
                     int /* terminal_id */,
                     IPC::PlatformFileForTransit /* desktop_process */,
                     IPC::PlatformFileForTransit /* desktop_pipe */)

//-----------------------------------------------------------------------------
// Chromoting messages sent from the network to the daemon process.

// Connects the terminal |terminal_id| (i.e. the remote client) to a desktop
// session.
IPC_MESSAGE_CONTROL1(ChromotingNetworkHostMsg_ConnectTerminal,
                     int /* terminal_id */)

// Disconnects the terminal |terminal_id| from the desktop session it was
// connected to.
IPC_MESSAGE_CONTROL1(ChromotingNetworkHostMsg_DisconnectTerminal,
                     int /* terminal_id */)

//-----------------------------------------------------------------------------
// Chromoting messages sent from the daemon to the desktop process.

// Requests the desktop process to crash producing a crash dump. The daemon
// sends this message when a fatal error has been detected indicating that
// the desktop process misbehaves. The daemon passes the location of the code
// that detected the error.
IPC_MESSAGE_CONTROL3(ChromotingDaemonDesktopMsg_Crash,
                     std::string /* function_name */,
                     std::string /* file_name */,
                     int /* line_number */)

//-----------------------------------------------------------------------------
// Chromoting messages sent from the desktop to the daemon process.

// Notifies the daemon that a desktop integration process has been initialized.
// |desktop_pipe| specifies the client end of the desktop pipe. It is to be
// forwarded to the desktop environment stub.
//
// Windows only: |desktop_pipe| has to be duplicated from the desktop process by
// the receiver of the message.
IPC_MESSAGE_CONTROL1(ChromotingDesktopDaemonMsg_DesktopAttached,
                     IPC::PlatformFileForTransit /* desktop_pipe */)
