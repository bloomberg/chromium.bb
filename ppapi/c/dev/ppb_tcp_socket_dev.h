/* Copyright 2013 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* From dev/ppb_tcp_socket_dev.idl modified Wed Jun 12 11:16:37 2013. */

#ifndef PPAPI_C_DEV_PPB_TCP_SOCKET_DEV_H_
#define PPAPI_C_DEV_PPB_TCP_SOCKET_DEV_H_

#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_macros.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/pp_stdint.h"
#include "ppapi/c/pp_var.h"

#define PPB_TCPSOCKET_DEV_INTERFACE_0_1 "PPB_TCPSocket(Dev);0.1"
#define PPB_TCPSOCKET_DEV_INTERFACE PPB_TCPSOCKET_DEV_INTERFACE_0_1

/**
 * @file
 * This file defines the <code>PPB_TCPSocket_Dev</code> interface.
 */


/**
 * @addtogroup Enums
 * @{
 */
typedef enum {
  /* Disables coalescing of small writes to make TCP segments, and instead
   * deliver data immediately. Value type is PP_VARTYPE_BOOL. */
  PP_TCPSOCKET_OPTION_NO_DELAY = 0,
  /* Specifies the socket send buffer in bytes. Value's type should be
   * PP_VARTYPE_INT32.
   * Note: This is only treated as a hint for the browser to set the buffer
   * size. Even if SetOption() reports that this option has been successfully
   * set, the browser doesn't guarantee to conform to it. */
  PP_TCPSOCKET_OPTION_SEND_BUFFER_SIZE = 1,
  /* Specifies the socket receive buffer in bytes. Value's type should be
   * PP_VARTYPE_INT32.
   * Note: This is only treated as a hint for the browser to set the buffer
   * size. Even if SetOption() reports that this option has been successfully
   * set, the browser doesn't guarantee to conform to it. */
  PP_TCPSOCKET_OPTION_RECV_BUFFER_SIZE = 2
} PP_TCPSocket_Option_Dev;
PP_COMPILE_ASSERT_SIZE_IN_BYTES(PP_TCPSocket_Option_Dev, 4);
/**
 * @}
 */

/**
 * @addtogroup Interfaces
 * @{
 */
/**
 * The <code>PPB_TCPSocket_Dev</code> interface provides TCP socket operations.
 */
struct PPB_TCPSocket_Dev_0_1 {
  /**
   * Allocates a TCP socket resource.
   */
  PP_Resource (*Create)(PP_Instance instance);
  /**
   * Determines if a given resource is TCP socket.
   */
  PP_Bool (*IsTCPSocket)(PP_Resource resource);
  /**
   * Connects to an address given by |addr|, which is a PPB_NetAddress_Dev
   * resource.
   */
  int32_t (*Connect)(PP_Resource tcp_socket,
                     PP_Resource addr,
                     struct PP_CompletionCallback callback);
  /**
   * Gets the local address of the socket, if it has been connected.
   * Returns a PPB_NetAddress_Dev resource on success; returns 0 on failure.
   */
  PP_Resource (*GetLocalAddress)(PP_Resource tcp_socket);
  /**
   * Gets the remote address of the socket, if it has been connected.
   * Returns a PPB_NetAddress_Dev resource on success; returns 0 on failure.
   */
  PP_Resource (*GetRemoteAddress)(PP_Resource tcp_socket);
  /**
   * Reads data from the socket. The size of |buffer| must be at least as large
   * as |bytes_to_read|. May perform a partial read. Returns the number of bytes
   * read or an error code. If the return value is 0, then it indicates that
   * end-of-file was reached.
   *
   * Multiple outstanding read requests are not supported.
   */
  int32_t (*Read)(PP_Resource tcp_socket,
                  char* buffer,
                  int32_t bytes_to_read,
                  struct PP_CompletionCallback callback);
  /**
   * Writes data to the socket. May perform a partial write. Returns the number
   * of bytes written or an error code.
   *
   * Multiple outstanding write requests are not supported.
   */
  int32_t (*Write)(PP_Resource tcp_socket,
                   const char* buffer,
                   int32_t bytes_to_write,
                   struct PP_CompletionCallback callback);
  /**
   * Cancels any IO that may be pending, and disconnects the socket. Any pending
   * callbacks will still run, reporting PP_ERROR_ABORTED if pending IO was
   * interrupted. It is NOT valid to call Connect() again after a call to this
   * method. Note: If the socket is destroyed when it is still connected, then
   * it will be implicitly disconnected, so you are not required to call this
   * method.
   */
  void (*Close)(PP_Resource tcp_socket);
  /**
   * Sets an option on |tcp_socket|.  Supported |name| and |value| parameters
   * are as described for PP_TCPSocketOption_Dev.  |callback| will be
   * invoked with PP_OK if setting the option succeeds, or an error code
   * otherwise. The socket must be connected before SetOption is called.
   */
  int32_t (*SetOption)(PP_Resource tcp_socket,
                       PP_TCPSocket_Option_Dev name,
                       struct PP_Var value,
                       struct PP_CompletionCallback callback);
};

typedef struct PPB_TCPSocket_Dev_0_1 PPB_TCPSocket_Dev;
/**
 * @}
 */

#endif  /* PPAPI_C_DEV_PPB_TCP_SOCKET_DEV_H_ */

