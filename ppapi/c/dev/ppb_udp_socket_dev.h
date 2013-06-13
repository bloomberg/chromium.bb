/* Copyright 2013 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* From dev/ppb_udp_socket_dev.idl modified Thu Jun 13 09:38:45 2013. */

#ifndef PPAPI_C_DEV_PPB_UDP_SOCKET_DEV_H_
#define PPAPI_C_DEV_PPB_UDP_SOCKET_DEV_H_

#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_macros.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/pp_stdint.h"
#include "ppapi/c/pp_var.h"

#define PPB_UDPSOCKET_DEV_INTERFACE_0_1 "PPB_UDPSocket(Dev);0.1"
#define PPB_UDPSOCKET_DEV_INTERFACE PPB_UDPSOCKET_DEV_INTERFACE_0_1

/**
 * @file
 * This file defines the <code>PPB_UDPSocket_Dev</code> interface.
 * TODO(yzshen): Tidy up the document.
 */


/**
 * @addtogroup Enums
 * @{
 */
typedef enum {
  /* Allows the socket to share the local address to which it will be bound with
   * other processes. Value's type should be PP_VARTYPE_BOOL. */
  PP_UDPSOCKET_OPTION_ADDRESS_REUSE = 0,
  /* Allows sending and receiving packets to and from broadcast addresses.
   * Value's type should be PP_VARTYPE_BOOL. */
  PP_UDPSOCKET_OPTION_BROADCAST = 1,
  /* Specifies the total per-socket buffer space reserved for sends. Value's
   * type should be PP_VARTYPE_INT32.
   * Note: This is only treated as a hint for the browser to set the buffer
   * size. Even if SetOption() reports that this option has been successfully
   * set, the browser doesn't guarantee it will conform to it. */
  PP_UDPSOCKET_OPTION_SEND_BUFFER_SIZE = 2,
  /* Specifies the total per-socket buffer space reserved for receives. Value's
   * type should be PP_VARTYPE_INT32.
   * Note: This is only treated as a hint for the browser to set the buffer
   * size. Even if SetOption() reports that this option has been successfully
   * set, the browser doesn't guarantee it will conform to it. */
  PP_UDPSOCKET_OPTION_RECV_BUFFER_SIZE = 3
} PP_UDPSocket_Option_Dev;
PP_COMPILE_ASSERT_SIZE_IN_BYTES(PP_UDPSocket_Option_Dev, 4);
/**
 * @}
 */

/**
 * @addtogroup Interfaces
 * @{
 */
struct PPB_UDPSocket_Dev_0_1 {
  /**
   * Creates a UDP socket resource.
   */
  PP_Resource (*Create)(PP_Instance instance);
  /**
   * Determines if a given resource is a UDP socket.
   */
  PP_Bool (*IsUDPSocket)(PP_Resource resource);
  /**
   * Binds to the address given by |addr|, which is a PPB_NetAddress_Dev
   * resource.
   */
  int32_t (*Bind)(PP_Resource udp_socket,
                  PP_Resource addr,
                  struct PP_CompletionCallback callback);
  /**
   * Returns the address that the socket has bound to, as a PPB_NetAddress_Dev
   * resource.  Bind must be called and succeed first. Returns 0 if Bind fails,
   * or if Close has been called.
   */
  PP_Resource (*GetBoundAddress)(PP_Resource udp_socket);
  /**
   * Performs a non-blocking recvfrom call on socket.
   * Bind must be called first. |callback| is invoked when recvfrom reads data.
   * |addr| will store a PPB_NetAddress_Dev resource on success.
   */
  int32_t (*RecvFrom)(PP_Resource udp_socket,
                      char* buffer,
                      int32_t num_bytes,
                      PP_Resource* addr,
                      struct PP_CompletionCallback callback);
  /**
   * Performs a non-blocking sendto call on the socket.
   * Bind must be called first. |addr| is a PPB_NetAddress_Dev resource holding
   * the target address. |callback| is invoked when sendto completes.
   */
  int32_t (*SendTo)(PP_Resource udp_socket,
                    const char* buffer,
                    int32_t num_bytes,
                    PP_Resource addr,
                    struct PP_CompletionCallback callback);
  /**
   * Cancels all pending reads and writes, and closes the socket.
   */
  void (*Close)(PP_Resource udp_socket);
  /**
   * Sets a socket option to |udp_socket|. Should be called before Bind().
   * See the PP_UDPSocket_Option_Dev description for option names, value types
   * and allowed values.
   * Returns PP_OK on success. Otherwise, returns PP_ERROR_BADRESOURCE (if bad
   * |udp_socket| provided), PP_ERROR_BADARGUMENT (if bad name/value/value's
   * type provided) or PP_ERROR_FAILED in the case of internal errors.
   */
  int32_t (*SetOption)(PP_Resource udp_socket,
                       PP_UDPSocket_Option_Dev name,
                       struct PP_Var value,
                       struct PP_CompletionCallback callback);
};

typedef struct PPB_UDPSocket_Dev_0_1 PPB_UDPSocket_Dev;
/**
 * @}
 */

#endif  /* PPAPI_C_DEV_PPB_UDP_SOCKET_DEV_H_ */

