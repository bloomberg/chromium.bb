/* Copyright (c) 2012 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* From private/ppb_udp_socket_private.idl modified Thu Mar 28 15:59:15 2013. */

#ifndef PPAPI_C_PRIVATE_PPB_UDP_SOCKET_PRIVATE_H_
#define PPAPI_C_PRIVATE_PPB_UDP_SOCKET_PRIVATE_H_

#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_macros.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/pp_stdint.h"
#include "ppapi/c/pp_var.h"
#include "ppapi/c/private/ppb_net_address_private.h"

#define PPB_UDPSOCKET_PRIVATE_INTERFACE_0_2 "PPB_UDPSocket_Private;0.2"
#define PPB_UDPSOCKET_PRIVATE_INTERFACE_0_3 "PPB_UDPSocket_Private;0.3"
#define PPB_UDPSOCKET_PRIVATE_INTERFACE_0_4 "PPB_UDPSocket_Private;0.4"
#define PPB_UDPSOCKET_PRIVATE_INTERFACE_0_5 "PPB_UDPSocket_Private;0.5"
#define PPB_UDPSOCKET_PRIVATE_INTERFACE PPB_UDPSOCKET_PRIVATE_INTERFACE_0_5

/**
 * @file
 * This file defines the <code>PPB_UDPSocket_Private</code> interface.
 */


/**
 * @addtogroup Enums
 * @{
 */
typedef enum {
  /* Allow the socket to share the local address to which socket will
   * be bound with other processes. Value's type should be
   * PP_VARTYPE_BOOL. Takes effect if set before Bind(). */
  PP_UDPSOCKETFEATURE_ADDRESS_REUSE = 0,
  /* Allow sending and receiving packets to and from broadcast
   * addresses. Value's type should be PP_VARTYPE_BOOL. Takes effect
   * if set before Bind(). */
  PP_UDPSOCKETFEATURE_BROADCAST = 1,
  /* Specifies the total per-socket buffer space reserved for
   * sends. Value's type should be PP_VARTYPE_INT32. Supported in v0.5
   * and above. */
  PP_UDPSOCKETFEATURE_SEND_BUFFER_SIZE = 2,
  /* Specifies the total per-socket buffer space reserved for
   * receives. Value's type should be PP_VARTYPE_INT32. Supported in
   * v0.5 and above. */
  PP_UDPSOCKETFEATURE_RECV_BUFFER_SIZE = 3,
  /* Special value for counting the number of available
   * features. Should not be passed to SetSocketFeature(). */
  PP_UDPSOCKETFEATURE_COUNT = 4
} PP_UDPSocketFeature_Private;
PP_COMPILE_ASSERT_SIZE_IN_BYTES(PP_UDPSocketFeature_Private, 4);
/**
 * @}
 */

/**
 * @addtogroup Interfaces
 * @{
 */
struct PPB_UDPSocket_Private_0_5 {
  /**
   * Creates a UDP socket resource.
   */
  PP_Resource (*Create)(PP_Instance instance_id);
  /**
   * Determines if a given resource is a UDP socket.
   */
  PP_Bool (*IsUDPSocket)(PP_Resource resource_id);
  /**
   * Sets a socket feature to |udp_socket|. Possible values for
   * |name|, |value| and |value|'s type are described in
   * PP_UDPSocketFeature_Private description. If no error occurs,
   * returns PP_OK. Otherwise, returns PP_ERROR_BADRESOURCE (if bad
   * |udp_socket| provided), PP_ERROR_BADARGUMENT (if bad
   * name/value/value's type provided) or PP_ERROR_FAILED in the case
   * of internal errors.
   */
  int32_t (*SetSocketFeature)(PP_Resource udp_socket,
                              PP_UDPSocketFeature_Private name,
                              struct PP_Var value);
  /* Creates a socket and binds to the address given by |addr|. */
  int32_t (*Bind)(PP_Resource udp_socket,
                  const struct PP_NetAddress_Private* addr,
                  struct PP_CompletionCallback callback);
  /* Returns the address that the socket has bound to.  A successful
   * call to Bind() must be called first. Returns PP_FALSE if Bind
   * fails, or if Close() has been called.
   */
  PP_Bool (*GetBoundAddress)(PP_Resource udp_socket,
                             struct PP_NetAddress_Private* addr);
  /* Performs a non-blocking recvfrom call on socket.  Bind() must be
   * called first. |callback| is invoked when recvfrom reads data. It
   * is possible to call RecvFrom() again on the same socket before
   * completion of the previous call.
   */
  int32_t (*RecvFrom)(PP_Resource udp_socket,
                      char* buffer,
                      int32_t num_bytes,
                      struct PP_NetAddress_Private* addr,
                      struct PP_CompletionCallback callback);
  /* Performs a non-blocking sendto call on the socket created and
   * bound(has already called Bind()).  The callback |callback| is
   * invoked when sendto completes. |buffer| and |addr| don't need to
   * persist until |callback| is called.
   */
  int32_t (*SendTo)(PP_Resource udp_socket,
                    const char* buffer,
                    int32_t num_bytes,
                    const struct PP_NetAddress_Private* addr,
                    struct PP_CompletionCallback callback);
  /* Cancels all pending reads and writes, and closes the socket. */
  void (*Close)(PP_Resource udp_socket);
};

typedef struct PPB_UDPSocket_Private_0_5 PPB_UDPSocket_Private;

struct PPB_UDPSocket_Private_0_2 {
  PP_Resource (*Create)(PP_Instance instance_id);
  PP_Bool (*IsUDPSocket)(PP_Resource resource_id);
  int32_t (*Bind)(PP_Resource udp_socket,
                  const struct PP_NetAddress_Private* addr,
                  struct PP_CompletionCallback callback);
  int32_t (*RecvFrom)(PP_Resource udp_socket,
                      char* buffer,
                      int32_t num_bytes,
                      struct PP_CompletionCallback callback);
  PP_Bool (*GetRecvFromAddress)(PP_Resource udp_socket,
                                struct PP_NetAddress_Private* addr);
  int32_t (*SendTo)(PP_Resource udp_socket,
                    const char* buffer,
                    int32_t num_bytes,
                    const struct PP_NetAddress_Private* addr,
                    struct PP_CompletionCallback callback);
  void (*Close)(PP_Resource udp_socket);
};

struct PPB_UDPSocket_Private_0_3 {
  PP_Resource (*Create)(PP_Instance instance_id);
  PP_Bool (*IsUDPSocket)(PP_Resource resource_id);
  int32_t (*Bind)(PP_Resource udp_socket,
                  const struct PP_NetAddress_Private* addr,
                  struct PP_CompletionCallback callback);
  PP_Bool (*GetBoundAddress)(PP_Resource udp_socket,
                             struct PP_NetAddress_Private* addr);
  int32_t (*RecvFrom)(PP_Resource udp_socket,
                      char* buffer,
                      int32_t num_bytes,
                      struct PP_CompletionCallback callback);
  PP_Bool (*GetRecvFromAddress)(PP_Resource udp_socket,
                                struct PP_NetAddress_Private* addr);
  int32_t (*SendTo)(PP_Resource udp_socket,
                    const char* buffer,
                    int32_t num_bytes,
                    const struct PP_NetAddress_Private* addr,
                    struct PP_CompletionCallback callback);
  void (*Close)(PP_Resource udp_socket);
};

struct PPB_UDPSocket_Private_0_4 {
  PP_Resource (*Create)(PP_Instance instance_id);
  PP_Bool (*IsUDPSocket)(PP_Resource resource_id);
  int32_t (*SetSocketFeature)(PP_Resource udp_socket,
                              PP_UDPSocketFeature_Private name,
                              struct PP_Var value);
  int32_t (*Bind)(PP_Resource udp_socket,
                  const struct PP_NetAddress_Private* addr,
                  struct PP_CompletionCallback callback);
  PP_Bool (*GetBoundAddress)(PP_Resource udp_socket,
                             struct PP_NetAddress_Private* addr);
  int32_t (*RecvFrom)(PP_Resource udp_socket,
                      char* buffer,
                      int32_t num_bytes,
                      struct PP_CompletionCallback callback);
  PP_Bool (*GetRecvFromAddress)(PP_Resource udp_socket,
                                struct PP_NetAddress_Private* addr);
  int32_t (*SendTo)(PP_Resource udp_socket,
                    const char* buffer,
                    int32_t num_bytes,
                    const struct PP_NetAddress_Private* addr,
                    struct PP_CompletionCallback callback);
  void (*Close)(PP_Resource udp_socket);
};
/**
 * @}
 */

#endif  /* PPAPI_C_PRIVATE_PPB_UDP_SOCKET_PRIVATE_H_ */

