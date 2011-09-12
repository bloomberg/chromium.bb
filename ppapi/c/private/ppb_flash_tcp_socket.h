// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_C_PRIVATE_PPB_FLASH_TCP_SOCKET_H_
#define PPAPI_C_PRIVATE_PPB_FLASH_TCP_SOCKET_H_

#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/pp_stdint.h"

// This is an opaque type holding a network address.
struct PP_Flash_NetAddress {
  uint32_t size;
  char data[128];
};

#define PPB_FLASH_TCPSOCKET_INTERFACE "PPB_Flash_TCPSocket;0.2"

struct PPB_Flash_TCPSocket {
  PP_Resource (*Create)(PP_Instance instance);

  PP_Bool (*IsFlashTCPSocket)(PP_Resource resource);

  // Connects to a TCP port given as a host-port pair.
  // When a proxy server is used, |host| and |port| refer to the proxy server
  // instead of the destination server.
  int32_t (*Connect)(PP_Resource tcp_socket,
                     const char* host,
                     uint16_t port,
                     struct PP_CompletionCallback callback);

  // Same as Connect(), but connecting to the address given by |addr|. A typical
  // use-case would be for reconnections.
  int32_t (*ConnectWithNetAddress)(PP_Resource tcp_socket,
                                   const struct PP_Flash_NetAddress* addr,
                                   struct PP_CompletionCallback callback);

  // Gets the local address of the socket, if it has been connected.
  // Returns PP_TRUE on success.
  PP_Bool (*GetLocalAddress)(PP_Resource tcp_socket,
                             struct PP_Flash_NetAddress* local_addr);

  // Gets the remote address of the socket, if it has been connected.
  // Returns PP_TRUE on success.
  PP_Bool (*GetRemoteAddress)(PP_Resource tcp_socket,
                              struct PP_Flash_NetAddress* remote_addr);

  // Does SSL handshake and moves to sending and receiving encrypted data. The
  // socket must have been successfully connected. |server_name| will be
  // compared with the name(s) in the server's certificate during the SSL
  // handshake. |server_port| is only used to identify an SSL server in the SSL
  // session cache.
  // When a proxy server is used, |server_name| and |server_port| refer to the
  // destination server.
  // If the socket is not connected, or there are pending read/write requests,
  // SSLHandshake() will fail without starting a handshake. Otherwise, any
  // failure during the handshake process will cause the socket to be
  // disconnected.
  int32_t (*SSLHandshake)(PP_Resource tcp_socket,
                          const char* server_name,
                          uint16_t server_port,
                          struct PP_CompletionCallback callback);

  // Reads data from the socket. The size of |buffer| must be at least as large
  // as |bytes_to_read|. May perform a partial read. Returns the number of bytes
  // read or an error code. If the return value is 0, then it indicates that
  // end-of-file was reached.
  // This method won't return more than 1 megabyte, so if |bytes_to_read|
  // exceeds 1 megabyte, it will always perform a partial read.
  // Multiple outstanding read requests are not supported.
  int32_t (*Read)(PP_Resource tcp_socket,
                  char* buffer,
                  int32_t bytes_to_read,
                  struct PP_CompletionCallback callback);

  // Writes data to the socket. May perform a partial write. Returns the number
  // of bytes written or an error code.
  // This method won't write more than 1 megabyte, so if |bytes_to_write|
  // exceeds 1 megabyte, it will always perform a partial write.
  // Multiple outstanding write requests are not supported.
  int32_t (*Write)(PP_Resource tcp_socket,
                   const char* buffer,
                   int32_t bytes_to_write,
                   struct PP_CompletionCallback callback);

  // Cancels any IO that may be pending, and disconnects the socket. Any pending
  // callbacks will still run, reporting PP_Error_Aborted if pending IO was
  // interrupted. It is NOT valid to call Connect() again after a call to this
  // method. Note: If the socket is destroyed when it is still connected, then
  // it will be implicitly disconnected, so you are not required to call this
  // method.
  void (*Disconnect)(PP_Resource tcp_socket);
};

#endif  // PPAPI_C_PRIVATE_PPB_FLASH_TCP_SOCKET_H_
