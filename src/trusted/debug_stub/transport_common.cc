/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#ifdef _WIN32
#include <windows.h>
#ifndef AF_IPX
#include <winsock2.h>
#endif
#define SOCKET_HANDLE SOCKET
#else

#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#define SOCKET_HANDLE int
#define closesocket close
#endif

#include "native_client/src/trusted/port/platform.h"
#include "native_client/src/trusted/port/transport.h"

namespace port {

typedef int socklen_t;

class Transport : public ITransport {
 public:
  Transport() {
    handle_ = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  }

  explicit Transport(SOCKET_HANDLE s) {
    handle_ = s;
  }

  ~Transport() {
    if (handle_ != -1) closesocket(handle_);
  }

  // Read from this transport, return a negative value if there is an error
  // otherwise return the number of bytes actually read.
  virtual int32_t Read(void *ptr, int32_t len) {
    return ::recv(handle_, reinterpret_cast<char *>(ptr), len, 0);
  }

  // Write to this transport, return a negative value if there is an error
  // otherwise return the number of bytes actually written.
  virtual int32_t Write(const void *ptr, int32_t len) {
    return ::send(handle_, reinterpret_cast<const char *>(ptr), len, 0);
  }

  // Return true if data becomes availible or false after ms milliseconds.
  virtual bool ReadWaitWithTimeout(uint32_t ms = 0) {
    fd_set fds;

    FD_ZERO(&fds);
    FD_SET(handle_, &fds);

    // We want a "non-blocking" check
    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec= ms * 1000;

    // Check if this file handle can select on read
    int cnt = select(0, &fds, 0, 0, &timeout);

    // If we are ready, or if there is an error.  We return true
    // on error, to "timeout" and let the next IO request fail.
    if (cnt != 0) return true;

    return false;
  }

// On windows, the header that defines this has other definition
// colitions, so we define it outselves just in case
#ifndef SD_BOTH
#define SD_BOTH 2
#endif

  virtual void Disconnect() {
    // Shutdown the conneciton in both diections.  This should
    // always succeed, and nothing we can do if this fails.
    (void) ::shutdown(handle_, SD_BOTH);
  }

 protected:
  SOCKET_HANDLE handle_;
};


static SOCKET_HANDLE s_ServerSock;

static bool SocketInit() {
  s_ServerSock = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (s_ServerSock == -1) {
    IPlatform::LogError("Failed to create socket.\n");
    return false;
  }
  struct sockaddr_in saddr;
  socklen_t addrlen = (socklen_t) sizeof(saddr);
  saddr.sin_family = AF_INET;
  // TODO(noelallen) use environment and default to 127.0.0.1
  saddr.sin_addr.s_addr = htonl(0);
  saddr.sin_port = htons(4014);

  if (bind(s_ServerSock, (struct sockaddr *) &saddr, addrlen)) {
    IPlatform::LogError("Failed to bind server.\n");
    return false;
  }

  if (listen(s_ServerSock, 1)) {
    IPlatform::LogError("Failed to listen.\n");
    return false;
  }

  return true;
}

static bool SocketsAvailible() {
  static bool _init = SocketInit();
  return _init;
}

ITransport* ITransport::Connect(const char *addr) {
  if (!SocketsAvailible())
    return NULL;

  SOCKET s = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (s == -1) {
    IPlatform::LogError("Failed to create connection socket.\n");
    return NULL;
  }

  struct sockaddr_in saddr;
  socklen_t addrlen = (socklen_t) sizeof(saddr);
  saddr.sin_family = AF_INET;
  saddr.sin_addr.s_addr = htonl(0x7F000001);
  saddr.sin_port = htons(4014);

  if (::connect(s, reinterpret_cast<sockaddr*>(&saddr), sizeof(saddr)) != 0) {
    IPlatform::LogError("Failed to connect.\n");
    return NULL;
  }

  return new Transport(s);
}

ITransport* ITransport::Accept(const char *addr) {
  if (!SocketsAvailible()) return NULL;

  SOCKET s = ::accept(s_ServerSock, NULL, 0);
  return new Transport(s);
}

void ITransport::Free(ITransport* itrans) {
  Transport* trans = static_cast<Transport*>(itrans);
  delete trans;
}

}  // namespace port

