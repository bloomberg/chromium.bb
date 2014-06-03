// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_MAC_LAUNCHD_INTERCEPTION_SERVER_H_
#define SANDBOX_MAC_LAUNCHD_INTERCEPTION_SERVER_H_

#include <dispatch/dispatch.h>
#include <mach/mach.h>

#include "base/mac/scoped_mach_port.h"
#include "base/mac/scoped_mach_vm.h"
#include "sandbox/mac/os_compatibility.h"

namespace sandbox {

class BootstrapSandbox;

// This class is used to run a Mach IPC message server. This server can
// hold the receive right for a bootstrap_port of a process, and it filters
// a subset of the launchd/bootstrap IPC call set for sandboxing. It permits
// or rejects requests based on the per-process policy specified in the
// BootstrapSandbox.
class LaunchdInterceptionServer {
 public:
  explicit LaunchdInterceptionServer(const BootstrapSandbox* sandbox);
  ~LaunchdInterceptionServer();

  // Initializes the class and starts running the message server.
  bool Initialize();

  mach_port_t server_port() const { return server_port_.get(); }

 private:
  // Event handler for the |server_source_| that reads a message from the queue
  // and processes it.
  void ReceiveMessage();

  // Decodes a message header and handles it by either servicing the request
  // itself, forwarding the message on to the real launchd, or rejecting the
  // message with an error.
  void DemuxMessage(mach_msg_header_t* request, mach_msg_header_t* reply);

  // Given a look_up2 request message, this looks up the appropriate sandbox
  // policy for the service name then formulates and sends the reply message.
  void HandleLookUp(mach_msg_header_t* request,
                    mach_msg_header_t* reply,
                    pid_t sender_pid);

  // Given a swap_integer request message, this verifies that it is safe, and
  // if so, forwards it on to launchd for servicing. If the request is unsafe,
  // it replies with an error.
  void HandleSwapInteger(mach_msg_header_t* request,
                         mach_msg_header_t* reply,
                         pid_t sender_pid);

  // Sends a reply message. Returns true if the message was sent successfully.
  bool SendReply(mach_msg_header_t* reply);

  // Forwards the original |request| on to real bootstrap server for handling.
  void ForwardMessage(mach_msg_header_t* request, mach_msg_header_t* reply);

  // Replies to the message with the specified |error_code| as a MIG
  // error_reply RetCode.
  void RejectMessage(mach_msg_header_t* request,
                     mach_msg_header_t* reply,
                     int error_code);

  // The sandbox for which this message server is running.
  const BootstrapSandbox* sandbox_;

  // The Mach port on which the server is receiving requests.
  base::mac::ScopedMachReceiveRight server_port_;

  // The dispatch queue used to service the server_source_.
  dispatch_queue_t server_queue_;

  // A MACH_RECV dispatch source for the server_port_.
  dispatch_source_t server_source_;

  // Request and reply buffers used in ReceiveMessage.
  base::mac::ScopedMachVM request_buffer_;
  base::mac::ScopedMachVM reply_buffer_;

  // Whether or not ForwardMessage() was called during ReceiveMessage().
  bool did_forward_message_;

  // The Mach port handed out in reply to denied look up requests. All denied
  // requests share the same port, though nothing reads messages from it.
  base::mac::ScopedMachReceiveRight sandbox_port_;
  // The send right for the above |sandbox_port_|, used with
  // MACH_MSG_TYPE_COPY_SEND when handing out references to the dummy port.
  base::mac::ScopedMachSendRight sandbox_send_port_;

  // The compatibility shim that handles differences in message header IDs and
  // request/reply structures between different OS X versions.
  const LaunchdCompatibilityShim compat_shim_;
};

}  // namespace sandbox

#endif  // SANDBOX_MAC_LAUNCHD_INTERCEPTION_SERVER_H_
