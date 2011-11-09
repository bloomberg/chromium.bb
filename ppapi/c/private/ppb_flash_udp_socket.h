/* Copyright (c) 2011 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* From ppb_flash_udp_socket.idl modified Mon Sep 26 09:04:41 2011. */

#ifndef PPAPI_C_PRIVATE_PPB_FLASH_UDP_SOCKET_H_
#define PPAPI_C_PRIVATE_PPB_FLASH_UDP_SOCKET_H_

#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_macros.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/pp_stdint.h"
#include "ppapi/c/private/ppb_flash_tcp_socket.h"

#define PPB_FLASH_UDPSOCKET_INTERFACE_0_1 "PPB_Flash_UDPSocket;0.1"
#define PPB_FLASH_UDPSOCKET_INTERFACE PPB_FLASH_UDPSOCKET_INTERFACE_0_1

/**
 * @file
 * This file defines the <code>PPB_Flash_UDPSocket</code> interface.
 */


/**
 * @addtogroup Interfaces
 * @{
 */
struct PPB_Flash_UDPSocket {
  /**
   * Creates a UDP socket resource.
   */
  PP_Resource (*Create)(PP_Instance instance_id);
  /**
   * Determines if a given resource is a UDP socket.
   */
  PP_Bool (*IsFlashUDPSocket)(PP_Resource resource_id);
  /* Creates a socket and binds to the address given by |addr|. */
  int32_t (*Bind)(PP_Resource udp_socket,
                  const struct PP_Flash_NetAddress* addr,
                  struct PP_CompletionCallback callback);
  /* Performs a non-blocking recvfrom call on socket.
   * Bind must be called first. |callback| is invoked when recvfrom
   * reads data.  You must call GetRecvFromAddress to recover the
   * address the data was retrieved from.
   */
  int32_t (*RecvFrom)(PP_Resource udp_socket,
                      char* buffer,
                      int32_t num_bytes,
                      struct PP_CompletionCallback callback);
  /* Upon successful completion of RecvFrom, the address that the data
   * was received from is stored in |addr|.
   */
  PP_Bool (*GetRecvFromAddress)(PP_Resource udp_socket,
                                struct PP_Flash_NetAddress* addr);
  /* Performs a non-blocking sendto call on the socket created and
   * bound(has already called Bind).  The callback |callback| is
   * invoked when sendto completes.
   */
  int32_t (*SendTo)(PP_Resource udp_socket,
                    const char* buffer,
                    int32_t num_bytes,
                    const struct PP_Flash_NetAddress* addr,
                    struct PP_CompletionCallback callback);
  /* Cancels all pending reads and writes, and closes the socket. */
  void (*Close)(PP_Resource udp_socket);
};
/**
 * @}
 */

#endif  /* PPAPI_C_PRIVATE_PPB_FLASH_UDP_SOCKET_H_ */

