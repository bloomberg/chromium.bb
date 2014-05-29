// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/mac/launchd_interception_server.h"

#include <bsm/libbsm.h>
#include <servers/bootstrap.h>

#include "base/logging.h"
#include "base/mac/mach_logging.h"
#include "sandbox/mac/bootstrap_sandbox.h"

namespace sandbox {

// The buffer size for all launchd messages. This comes from
// sizeof(union __RequestUnion__vproc_mig_job_subsystem) in launchd, and it
// is larger than the __ReplyUnion.
const mach_msg_size_t kBufferSize = mach_vm_round_page(2096 +
    sizeof(mach_msg_audit_trailer_t));

LaunchdInterceptionServer::LaunchdInterceptionServer(
    const BootstrapSandbox* sandbox)
    : sandbox_(sandbox),
      server_port_(MACH_PORT_NULL),
      server_queue_(NULL),
      server_source_(NULL),
      did_forward_message_(false),
      sandbox_port_(MACH_PORT_NULL),
      compat_shim_(GetLaunchdCompatibilityShim()) {
}

LaunchdInterceptionServer::~LaunchdInterceptionServer() {
  if (server_source_)
    dispatch_release(server_source_);
  if (server_queue_)
    dispatch_release(server_queue_);
}

bool LaunchdInterceptionServer::Initialize() {
  mach_port_t task = mach_task_self();
  kern_return_t kr;

  // Allocate a port for use as a new bootstrap port.
  mach_port_t port;
  if ((kr = mach_port_allocate(task, MACH_PORT_RIGHT_RECEIVE, &port)) !=
          KERN_SUCCESS) {
    MACH_LOG(ERROR, kr) << "Failed to allocate new bootstrap port.";
    return false;
  }
  server_port_.reset(port);

  // Allocate the message request and reply buffers.
  const int kMachMsgMemoryFlags = VM_MAKE_TAG(VM_MEMORY_MACH_MSG) |
                                  VM_FLAGS_ANYWHERE;
  vm_address_t buffer = 0;

  kr = vm_allocate(task, &buffer, kBufferSize, kMachMsgMemoryFlags);
  if (kr != KERN_SUCCESS) {
    MACH_LOG(ERROR, kr) << "Failed to allocate request buffer.";
    return false;
  }
  request_buffer_.reset(buffer, kBufferSize);

  kr = vm_allocate(task, &buffer, kBufferSize, kMachMsgMemoryFlags);
  if (kr != KERN_SUCCESS) {
    MACH_LOG(ERROR, kr) << "Failed to allocate reply buffer.";
    return false;
  }
  reply_buffer_.reset(buffer, kBufferSize);

  // Allocate the dummy sandbox port.
  if ((kr = mach_port_allocate(task, MACH_PORT_RIGHT_RECEIVE, &port)) !=
          KERN_SUCCESS) {
    MACH_LOG(ERROR, kr) << "Failed to allocate dummy sandbox port.";
    return false;
  }
  sandbox_port_.reset(port);

  // Set up the dispatch queue to service the bootstrap port.
  // TODO(rsesek): Specify DISPATCH_QUEUE_SERIAL, in the 10.7 SDK. NULL means
  // the same thing but is not symbolically clear.
  server_queue_ = dispatch_queue_create(
      "org.chromium.sandbox.LaunchdInterceptionServer", NULL);
  server_source_ = dispatch_source_create(DISPATCH_SOURCE_TYPE_MACH_RECV,
      server_port_.get(), 0, server_queue_);
  dispatch_source_set_event_handler(server_source_, ^{ ReceiveMessage(); });
  dispatch_resume(server_source_);

  return true;
}

void LaunchdInterceptionServer::ReceiveMessage() {
  const mach_msg_options_t kRcvOptions = MACH_RCV_MSG |
      MACH_RCV_TRAILER_TYPE(MACH_MSG_TRAILER_FORMAT_0) |
      MACH_RCV_TRAILER_ELEMENTS(MACH_RCV_TRAILER_AUDIT);

  mach_msg_header_t* request =
      reinterpret_cast<mach_msg_header_t*>(request_buffer_.address());
  mach_msg_header_t* reply =
      reinterpret_cast<mach_msg_header_t*>(reply_buffer_.address());

  // Zero out the buffers from handling any previous message.
  bzero(request, kBufferSize);
  bzero(reply, kBufferSize);
  did_forward_message_ = false;

  // A Mach message server-once. The system library to run a message server
  // cannot be used here, because some requests are conditionally forwarded
  // to another server.
  kern_return_t kr = mach_msg(request, kRcvOptions, 0, kBufferSize,
      server_port_.get(), MACH_MSG_TIMEOUT_NONE, MACH_PORT_NULL);
  if (kr != KERN_SUCCESS) {
    MACH_LOG(ERROR, kr) << "Unable to receive message.";
    return;
  }

  // Set up a reply message in case it will be used.
  reply->msgh_bits = MACH_MSGH_BITS_REMOTE(reply->msgh_bits);
  // Since mach_msg will automatically swap the request and reply ports,
  // undo that.
  reply->msgh_remote_port = request->msgh_remote_port;
  reply->msgh_local_port = MACH_PORT_NULL;
  // MIG servers simply add 100 to the request ID to generate the reply ID.
  reply->msgh_id = request->msgh_id + 100;

  // Process the message.
  DemuxMessage(request, reply);

  // Free any descriptors in the message body. If the message was forwarded,
  // any descriptors would have been moved out of the process on send. If the
  // forwarded message was sent from the process hosting this sandbox server,
  // destroying the message could also destroy rights held outside the scope of
  // this message server.
  if (!did_forward_message_) {
    mach_msg_destroy(request);
    mach_msg_destroy(reply);
  }
}

void LaunchdInterceptionServer::DemuxMessage(mach_msg_header_t* request,
                                             mach_msg_header_t* reply) {
  VLOG(3) << "Incoming message #" << request->msgh_id;

  // Get the PID of the task that sent this request. This requires getting at
  // the trailer of the message, from the header.
  mach_msg_audit_trailer_t* trailer =
      reinterpret_cast<mach_msg_audit_trailer_t*>(
          reinterpret_cast<vm_address_t>(request) +
              round_msg(request->msgh_size));
  // TODO(rsesek): In the 10.7 SDK, there's audit_token_to_pid().
  pid_t sender_pid;
  audit_token_to_au32(trailer->msgh_audit,
      NULL, NULL, NULL, NULL, NULL, &sender_pid, NULL, NULL);

  if (sandbox_->PolicyForProcess(sender_pid) == NULL) {
    // No sandbox policy is in place for the sender of this message, which
    // means it is from the sandbox host process or an unsandboxed child.
    VLOG(3) << "Message from pid " << sender_pid << " forwarded to launchd";
    ForwardMessage(request, reply);
    return;
  }

  if (request->msgh_id == compat_shim_.msg_id_look_up2) {
    // Filter messages sent via bootstrap_look_up to enforce the sandbox policy
    // over the bootstrap namespace.
    HandleLookUp(request, reply, sender_pid);
  } else if (request->msgh_id == compat_shim_.msg_id_swap_integer) {
    // Ensure that any vproc_swap_integer requests are safe.
    HandleSwapInteger(request, reply, sender_pid);
  } else {
    // All other messages are not permitted.
    VLOG(1) << "Rejecting unhandled message #" << request->msgh_id;
    RejectMessage(request, reply, MIG_REMOTE_ERROR);
  }
}

void LaunchdInterceptionServer::HandleLookUp(mach_msg_header_t* request,
                                             mach_msg_header_t* reply,
                                             pid_t sender_pid) {
  const std::string request_service_name(
      compat_shim_.look_up2_get_request_name(request));
  VLOG(2) << "Incoming look_up2 request for " << request_service_name;

  // Find the Rule for this service. If one is not found, use
  // a safe default, POLICY_DENY_ERROR.
  const BootstrapSandboxPolicy* policy = sandbox_->PolicyForProcess(sender_pid);
  const BootstrapSandboxPolicy::const_iterator it =
      policy->find(request_service_name);
  Rule rule(POLICY_DENY_ERROR);
  if (it != policy->end())
    rule = it->second;

  if (rule.result == POLICY_ALLOW) {
    // This service is explicitly allowed, so this message will not be
    // intercepted by the sandbox.
    VLOG(1) << "Permitting and forwarding look_up2: " << request_service_name;
    ForwardMessage(request, reply);
  } else if (rule.result == POLICY_DENY_ERROR) {
    // The child is not permitted to look up this service. Send a MIG error
    // reply to the client. Returning a NULL or unserviced port for a look up
    // can cause clients to crash or hang.
    VLOG(1) << "Denying look_up2 with MIG error: " << request_service_name;
    RejectMessage(request, reply, BOOTSTRAP_UNKNOWN_SERVICE);
  } else if (rule.result == POLICY_DENY_DUMMY_PORT ||
             rule.result == POLICY_SUBSTITUTE_PORT) {
    // The policy result is to deny access to the real service port, replying
    // with a sandboxed port in its stead. Use either the dummy sandbox_port_
    // or the one specified in the policy.
    VLOG(1) << "Intercepting look_up2 with a sandboxed service port: "
            << request_service_name;

    mach_port_t result_port;
    if (rule.result == POLICY_DENY_DUMMY_PORT)
      result_port = sandbox_port_.get();
    else
      result_port = rule.substitute_port;

    // Grant an additional send right on the result_port so that it can be
    // sent to the sandboxed child process.
    kern_return_t kr = mach_port_insert_right(mach_task_self(),
        result_port, result_port, MACH_MSG_TYPE_MAKE_SEND);
    if (kr != KERN_SUCCESS) {
      MACH_LOG(ERROR, kr) << "Unable to insert right on result_port.";
    }

    compat_shim_.look_up2_fill_reply(reply, result_port);
    SendReply(reply);
  } else {
    NOTREACHED();
  }
}

void LaunchdInterceptionServer::HandleSwapInteger(mach_msg_header_t* request,
                                                  mach_msg_header_t* reply,
                                                  pid_t sender_pid) {
  // Only allow getting information out of launchd. Do not allow setting
  // values. Two commonly observed values that are retrieved are
  // VPROC_GSK_MGR_PID and VPROC_GSK_TRANSACTIONS_ENABLED.
  if (compat_shim_.swap_integer_is_get_only(request)) {
    VLOG(2) << "Forwarding vproc swap_integer message.";
    ForwardMessage(request, reply);
  } else {
    VLOG(2) << "Rejecting non-read-only swap_integer message.";
    RejectMessage(request, reply, BOOTSTRAP_NOT_PRIVILEGED);
  }
}

void LaunchdInterceptionServer::SendReply(mach_msg_header_t* reply) {
  kern_return_t kr = mach_msg(reply, MACH_SEND_MSG, reply->msgh_size, 0,
      MACH_PORT_NULL, MACH_MSG_TIMEOUT_NONE, MACH_PORT_NULL);
  if (kr != KERN_SUCCESS) {
    MACH_LOG(ERROR, kr) << "Unable to send intercepted reply message.";
  }
}

void LaunchdInterceptionServer::ForwardMessage(mach_msg_header_t* request,
                                               mach_msg_header_t* reply) {
  request->msgh_local_port = request->msgh_remote_port;
  request->msgh_remote_port = sandbox_->real_bootstrap_port();
  // Preserve the msgh_bits that do not deal with the local and remote ports.
  request->msgh_bits = (request->msgh_bits & ~MACH_MSGH_BITS_PORTS_MASK) |
      MACH_MSGH_BITS(MACH_MSG_TYPE_COPY_SEND, MACH_MSG_TYPE_MOVE_SEND_ONCE);
  kern_return_t kr = mach_msg_send(request);
  if (kr == KERN_SUCCESS) {
    did_forward_message_ = true;
  } else {
    MACH_LOG(ERROR, kr) << "Unable to forward message to the real launchd.";
  }
}

void LaunchdInterceptionServer::RejectMessage(mach_msg_header_t* request,
                                              mach_msg_header_t* reply,
                                              int error_code) {
  mig_reply_error_t* error_reply = reinterpret_cast<mig_reply_error_t*>(reply);
  error_reply->Head.msgh_size = sizeof(mig_reply_error_t);
  error_reply->Head.msgh_bits =
      MACH_MSGH_BITS_REMOTE(MACH_MSG_TYPE_MOVE_SEND_ONCE);
  error_reply->NDR = NDR_record;
  error_reply->RetCode = error_code;
  SendReply(&error_reply->Head);
}

}  // namespace sandbox
