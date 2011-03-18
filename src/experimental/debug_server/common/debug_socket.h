#ifndef NACL_SDK_BUILD_TOOLS_DEBUG_SERVER_COMMON_DEBUG_SOCKET_H_
#define NACL_SDK_BUILD_TOOLS_DEBUG_SERVER_COMMON_DEBUG_SOCKET_H_

#include <winsock.h>
#include <string>
#include "debug_blob.h"

namespace debug {

// Implements a listening socket.
class Socket;
class ListeningSocket {
 public:
  ListeningSocket();
  ~ListeningSocket();
  
  bool Setup(int port);  // Listens on port.
  bool Accept(Socket* new_connection, long wait_ms);
  void Close();

 protected:
  SOCKET sock_;
  bool init_success_;
};

// Implements a raw socket interface.
class Socket {
 public:
  Socket();
  ~Socket();

  bool ConnectTo(const std::string& host, int port);
  bool IsConnected() const;
  void Close();
  size_t Read(void* buff, size_t sz, int wait_ms);
  size_t Write(const void* buff, size_t sz, int wait_ms);
  void WriteAll(const void* buff, size_t sz);
  void WriteAll(const Blob& blob);
  
 protected:
  void AttachTo(SOCKET sock);

  SOCKET sock_;
  bool init_success_;
  friend class ListeningSocket;
};
}  // namespace debug
#endif  // NACL_SDK_BUILD_TOOLS_DEBUG_SERVER_COMMON_DEBUG_SOCKET_H_