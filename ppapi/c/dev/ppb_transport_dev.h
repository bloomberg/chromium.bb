/* Copyright (c) 2011 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#ifndef PPAPI_C_PPB_TRANSPORT_DEV_H_
#define PPAPI_C_PPB_TRANSPORT_DEV_H_

#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_module.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/pp_stdint.h"
#include "ppapi/c/pp_var.h"

#define PPB_TRANSPORT_DEV_INTERFACE "PPB_Transport;0.4"

struct PPB_Transport_Dev {
  // Creates a new transport object with the specified name using the
  // specified protocol.
  PP_Resource (*CreateTransport)(PP_Instance instance,
                                 const char* name,
                                 const char* proto);

  // Returns PP_TRUE if resource is a Transport, PP_FALSE otherwise.
  PP_Bool (*IsTransport)(PP_Resource resource);

  // Returns PP_TRUE if the transport is currently writable (i.e. can
  // send data to the remote peer), PP_FALSE otherwise.
  PP_Bool (*IsWritable)(PP_Resource transport);
  // TODO(juberti): other getters/setters
  // connect state
  // connect type, protocol
  // RTT

  // Establishes a connection to the remote peer.  Returns
  // PP_ERROR_WOULDBLOCK and notifies on |cb| when connectivity is
  // established (or timeout occurs).
  int32_t (*Connect)(PP_Resource transport,
                     struct PP_CompletionCallback cb);

  // Obtains another ICE candidate address to be provided to the
  // remote peer. Returns PP_ERROR_WOULDBLOCK if there are no more
  // addresses to be sent. After the callback is called
  // GetNextAddress() must be called again to get the address.
  int32_t (*GetNextAddress)(PP_Resource transport,
                            struct PP_Var* address,
                            struct PP_CompletionCallback cb);
  // Provides an ICE candidate address that was received
  // from the remote peer.
  int32_t (*ReceiveRemoteAddress)(PP_Resource transport,
                                  struct PP_Var address);

  // Like recv(), receives data. Returns PP_ERROR_WOULDBLOCK if there
  // is currently no data to receive. In that case, the |data| pointer
  // should remain valid until the callback is called.
  int32_t (*Recv)(PP_Resource transport,
                  void* data,
                  uint32_t len,
                  struct PP_CompletionCallback cb);
  // Like send(), sends data. Returns PP_ERROR_WOULDBLOCK if the
  // socket is currently flow-controlled. In that case, the |data|
  // pointer should remain valid until the callback is called.
  int32_t (*Send)(PP_Resource transport,
                  const void* data,
                  uint32_t len,
                  struct PP_CompletionCallback cb);

  // Disconnects from the remote peer.
  int32_t (*Close)(PP_Resource transport);
};

#endif  /* PPAPI_C_PPB_TRANSPORT_DEV_H_ */
