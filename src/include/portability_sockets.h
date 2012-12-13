/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef NATIVE_CLIENT_SRC_INCLUDE_PORTABILITY_SOCKETS_H_
#define NATIVE_CLIENT_SRC_INCLUDE_PORTABILITY_SOCKETS_H_ 1

#if NACL_WINDOWS

# include <windows.h>
/*
 * Gyp and scons builds have some different defines that include
 * WIN32_LEAN_AND_MEAN. So we have to detect whether winsock.h is already
 * included and including winsock2.h will lead to redefinition errors.
 * TODO(halyavin): ensure that WIN32_LEAN_AND_MEAN is set in both builds and
 * replace this with a meaningful error message.
 */
# ifndef AF_IPX
#  include <winsock2.h>
# endif

typedef SOCKET NaClSocketHandle;

# define NaClCloseSocket closesocket
# define NACL_INVALID_SOCKET INVALID_SOCKET
# define NaClSocketGetLastError() WSAGetLastError()
#else

# include <arpa/inet.h>
# include <netdb.h>
# include <netinet/tcp.h>
# include <sys/select.h>
# include <sys/socket.h>

typedef int NaClSocketHandle;

# define NaClCloseSocket close
# define NACL_INVALID_SOCKET (-1)
# define NaClSocketGetLastError() errno

#endif

#endif  // NATIVE_CLIENT_SRC_INCLUDE_PORTABILITY_SOCKETS_H_
