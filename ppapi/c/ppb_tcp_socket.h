/* Copyright 2013 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* From ppb_tcp_socket.idl modified Sat Jun 22 11:17:34 2013. */

#ifndef PPAPI_C_PPB_TCP_SOCKET_H_
#define PPAPI_C_PPB_TCP_SOCKET_H_

#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_macros.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/pp_stdint.h"
#include "ppapi/c/pp_var.h"

#define PPB_TCPSOCKET_INTERFACE_1_0 "PPB_TCPSocket;1.0"
#define PPB_TCPSOCKET_INTERFACE PPB_TCPSOCKET_INTERFACE_1_0

/**
 * @file
 * This file defines the <code>PPB_TCPSocket</code> interface.
 */


/**
 * @addtogroup Enums
 * @{
 */
/**
 * Option names used by <code>SetOption()</code>.
 */
typedef enum {
  /**
   * Disables coalescing of small writes to make TCP segments, and instead
   * delivers data immediately. Value's type is <code>PP_VARTYPE_BOOL</code>.
   * This option can only be set after a successful <code>Connect()</code> call.
   */
  PP_TCPSOCKET_OPTION_NO_DELAY = 0,
  /**
   * Specifies the total per-socket buffer space reserved for sends. Value's
   * type should be <code>PP_VARTYPE_INT32</code>.
   * This option can only be set after a successful <code>Connect()</code> call.
   *
   * Note: This is only treated as a hint for the browser to set the buffer
   * size. Even if <code>SetOption()</code> succeeds, the browser doesn't
   * guarantee it will conform to the size.
   */
  PP_TCPSOCKET_OPTION_SEND_BUFFER_SIZE = 1,
  /**
   * Specifies the total per-socket buffer space reserved for receives. Value's
   * type should be <code>PP_VARTYPE_INT32</code>.
   * This option can only be set after a successful <code>Connect()</code> call.
   *
   * Note: This is only treated as a hint for the browser to set the buffer
   * size. Even if <code>SetOption()</code> succeeds, the browser doesn't
   * guarantee it will conform to the size.
   */
  PP_TCPSOCKET_OPTION_RECV_BUFFER_SIZE = 2
} PP_TCPSocket_Option;
PP_COMPILE_ASSERT_SIZE_IN_BYTES(PP_TCPSocket_Option, 4);
/**
 * @}
 */

/**
 * @addtogroup Interfaces
 * @{
 */
/**
 * The <code>PPB_TCPSocket</code> interface provides TCP socket operations.
 *
 * Permissions: Apps permission <code>socket</code> with subrule
 * <code>tcp-connect</code> is required for <code>Connect()</code>.
 * For more details about network communication permissions, please see:
 * http://developer.chrome.com/apps/app_network.html
 */
struct PPB_TCPSocket_1_0 {
  /**
   * Creates a TCP socket resource.
   *
   * @param[in] instance A <code>PP_Instance</code> identifying one instance of
   * a module.
   *
   * @return A <code>PP_Resource</code> corresponding to a TCP socket or 0
   * on failure.
   */
  PP_Resource (*Create)(PP_Instance instance);
  /**
   * Determines if a given resource is a TCP socket.
   *
   * @param[in] resource A <code>PP_Resource</code> to check.
   *
   * @return <code>PP_TRUE</code> if the input is a
   * <code>PPB_TCPSocket</code> resource; <code>PP_FALSE</code> otherwise.
   */
  PP_Bool (*IsTCPSocket)(PP_Resource resource);
  /**
   * Connects the socket to the given address.
   *
   * @param[in] tcp_socket A <code>PP_Resource</code> corresponding to a TCP
   * socket.
   * @param[in] addr A <code>PPB_NetAddress</code> resource.
   * @param[in] callback A <code>PP_CompletionCallback</code> to be called upon
   * completion.
   *
   * @return An int32_t containing an error code from <code>pp_errors.h</code>,
   * including (but not limited to):
   * - <code>PP_ERROR_NOACCESS</code>: the caller doesn't have required
   *   permissions.
   * - <code>PP_ERROR_ADDRESS_UNREACHABLE</code>: <code>addr</code> is
   *   unreachable.
   * - <code>PP_ERROR_CONNECTION_REFUSED</code>: the connection attempt was
   *   refused.
   * - <code>PP_ERROR_CONNECTION_FAILED</code>: the connection attempt failed.
   * - <code>PP_ERROR_CONNECTION_TIMEDOUT</code>: the connection attempt timed
   *   out.
   */
  int32_t (*Connect)(PP_Resource tcp_socket,
                     PP_Resource addr,
                     struct PP_CompletionCallback callback);
  /**
   * Gets the local address of the socket, if it is connected.
   *
   * @param[in] tcp_socket A <code>PP_Resource</code> corresponding to a TCP
   * socket.
   *
   * @return A <code>PPB_NetAddress</code> resource on success or 0 on failure.
   */
  PP_Resource (*GetLocalAddress)(PP_Resource tcp_socket);
  /**
   * Gets the remote address of the socket, if it is connected.
   *
   * @param[in] tcp_socket A <code>PP_Resource</code> corresponding to a TCP
   * socket.
   *
   * @return A <code>PPB_NetAddress</code> resource on success or 0 on failure.
   */
  PP_Resource (*GetRemoteAddress)(PP_Resource tcp_socket);
  /**
   * Reads data from the socket. The socket must be connected. It may perform a
   * partial read.
   *
   * @param[in] tcp_socket A <code>PP_Resource</code> corresponding to a TCP
   * socket.
   * @param[out] buffer The buffer to store the received data on success. It
   * must be at least as large as <code>bytes_to_read</code>.
   * @param[in] bytes_to_read The number of bytes to read.
   * @param[in] callback A <code>PP_CompletionCallback</code> to be called upon
   * completion.
   *
   * @return A non-negative number on success to indicate how many bytes have
   * been read, 0 means that end-of-file was reached; otherwise, an error code
   * from <code>pp_errors.h</code>.
   */
  int32_t (*Read)(PP_Resource tcp_socket,
                  char* buffer,
                  int32_t bytes_to_read,
                  struct PP_CompletionCallback callback);
  /**
   * Writes data to the socket. The socket must be connected. It may perform a
   * partial write.
   *
   * @param[in] tcp_socket A <code>PP_Resource</code> corresponding to a TCP
   * socket.
   * @param[in] buffer The buffer containing the data to write.
   * @param[in] bytes_to_write The number of bytes to write.
   * @param[in] callback A <code>PP_CompletionCallback</code> to be called upon
   * completion.
   *
   * @return A non-negative number on success to indicate how many bytes have
   * been written; otherwise, an error code from <code>pp_errors.h</code>.
   */
  int32_t (*Write)(PP_Resource tcp_socket,
                   const char* buffer,
                   int32_t bytes_to_write,
                   struct PP_CompletionCallback callback);
  /**
   * Cancels all pending reads and writes and disconnects the socket. Any
   * pending callbacks will still run, reporting <code>PP_ERROR_ABORTED</code>
   * if pending IO was interrupted. After a call to this method, no output
   * buffer pointers passed into previous <code>Read()</code> calls will be
   * accessed. It is not valid to call <code>Connect()</code> again.
   *
   * The socket is implicitly closed if it is destroyed, so you are not required
   * to call this method.
   *
   * @param[in] tcp_socket A <code>PP_Resource</code> corresponding to a TCP
   * socket.
   */
  void (*Close)(PP_Resource tcp_socket);
  /**
   * Sets a socket option on the TCP socket.
   * Please see the <code>PP_TCPSocket_Option</code> description for option
   * names, value types and allowed values.
   *
   * @param[in] tcp_socket A <code>PP_Resource</code> corresponding to a TCP
   * socket.
   * @param[in] name The option to set.
   * @param[in] value The option value to set.
   * @param[in] callback A <code>PP_CompletionCallback</code> to be called upon
   * completion.
   *
   * @return An int32_t containing an error code from <code>pp_errors.h</code>.
   */
  int32_t (*SetOption)(PP_Resource tcp_socket,
                       PP_TCPSocket_Option name,
                       struct PP_Var value,
                       struct PP_CompletionCallback callback);
};

typedef struct PPB_TCPSocket_1_0 PPB_TCPSocket;
/**
 * @}
 */

#endif  /* PPAPI_C_PPB_TCP_SOCKET_H_ */

