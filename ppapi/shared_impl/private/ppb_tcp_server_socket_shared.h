// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_SHARED_IMPL_PRIVATE_PPB_TCP_SERVER_SOCKET_SHARED_H_
#define PPAPI_SHARED_IMPL_PRIVATE_PPB_TCP_SERVER_SOCKET_SHARED_H_

#include "base/compiler_specific.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/shared_impl/resource.h"
#include "ppapi/shared_impl/tracked_callback.h"
#include "ppapi/thunk/ppb_tcp_server_socket_private_api.h"

namespace ppapi {

// This class provides the shared implementation of a
// PPB_TCPServerSocket_Private.  The functions that actually send
// messages to browser are implemented differently for the proxied and
// non-proxied derived classes.
class PPAPI_SHARED_EXPORT PPB_TCPServerSocket_Shared
    : public thunk::PPB_TCPServerSocket_Private_API,
      public Resource {
 public:
  // C-tor used in Impl case.
  PPB_TCPServerSocket_Shared(PP_Instance instance);
  // C-tor used in Proxy case.
  PPB_TCPServerSocket_Shared(const HostResource& resource);

  virtual ~PPB_TCPServerSocket_Shared();

  // Resource overrides.
  virtual PPB_TCPServerSocket_Private_API*
      AsPPB_TCPServerSocket_Private_API() OVERRIDE;

  // PPB_TCPServerSocket_Private_API implementation.
  virtual int32_t Listen(const PP_NetAddress_Private* addr,
                         int32_t backlog,
                         scoped_refptr<TrackedCallback> callback) OVERRIDE;
  virtual int32_t Accept(PP_Resource* tcp_socket,
                         scoped_refptr<TrackedCallback> callback) OVERRIDE;
  virtual void StopListening() OVERRIDE;

  void OnListenCompleted(uint32 socket_id, int32_t status);
  virtual void OnAcceptCompleted(bool succeeded,
                                 uint32 accepted_socket_id,
                                 const PP_NetAddress_Private& local_addr,
                                 const PP_NetAddress_Private& remote_addr) = 0;

  // Send functions that need to be implemented differently for the
  // proxied and non-proxied derived classes.
  virtual void SendListen(const PP_NetAddress_Private& addr,
                          int32_t backlog) = 0;
  virtual void SendAccept() = 0;
  virtual void SendStopListening() = 0;

 protected:
  enum State {
    // Before listening (including a listen request is pending or a
    // previous listen request failed).
    BEFORE_LISTENING,
    // Socket is successfully bound and in listening mode.
    LISTENING,
    // The socket is closed.
    CLOSED
  };

  uint32 socket_id_;
  State state_;

  scoped_refptr<TrackedCallback> listen_callback_;
  scoped_refptr<TrackedCallback> accept_callback_;

  PP_Resource* tcp_socket_buffer_;

  DISALLOW_COPY_AND_ASSIGN(PPB_TCPServerSocket_Shared);
};

}  // namespace ppapi

#endif  // PPAPI_SHARED_IMPL_PRIVATE_PPB_TCP_SERVER_SOCKET_SHARED_H_
