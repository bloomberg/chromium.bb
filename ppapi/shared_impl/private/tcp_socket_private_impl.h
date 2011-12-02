// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_SHARED_IMPL_PRIVATE_TCP_SOCKET_PRIVATE_IMPL_H_
#define PPAPI_SHARED_IMPL_PRIVATE_TCP_SOCKET_PRIVATE_IMPL_H_

#include <string>

#include "base/compiler_specific.h"
#include "ppapi/shared_impl/resource.h"
#include "ppapi/thunk/ppb_tcp_socket_private_api.h"

namespace ppapi {

// This class provides the shared implementation of a
// PPB_TCPSocket_Private.  The functions that actually send messages
// to browser are implemented differently for the proxied and
// non-proxied derived classes.
class PPAPI_SHARED_EXPORT TCPSocketPrivateImpl
    : public thunk::PPB_TCPSocket_Private_API,
      public Resource {
 public:
  // C-tor used in Impl case.
  TCPSocketPrivateImpl(PP_Instance instance, uint32 socket_id);
  // C-tor used in Proxy case.
  TCPSocketPrivateImpl(const HostResource& resource, uint32 socket_id);

  virtual ~TCPSocketPrivateImpl();

  // The maximum number of bytes that each PpapiHostMsg_PPBTCPSocket_Read
  // message is allowed to request.
  static const int32_t kMaxReadSize;
  // The maximum number of bytes that each PpapiHostMsg_PPBTCPSocket_Write
  // message is allowed to carry.
  static const int32_t kMaxWriteSize;

  // Resource overrides.
  virtual PPB_TCPSocket_Private_API* AsPPB_TCPSocket_Private_API() OVERRIDE;

  // PPB_TCPSocket_Private_API implementation.
  virtual int32_t Connect(const char* host,
                          uint16_t port,
                          PP_CompletionCallback callback) OVERRIDE;
  virtual int32_t ConnectWithNetAddress(
      const PP_NetAddress_Private* addr,
      PP_CompletionCallback callback) OVERRIDE;
  virtual PP_Bool GetLocalAddress(PP_NetAddress_Private* local_addr) OVERRIDE;
  virtual PP_Bool GetRemoteAddress(PP_NetAddress_Private* remote_addr) OVERRIDE;
  virtual int32_t SSLHandshake(const char* server_name,
                              uint16_t server_port,
                              PP_CompletionCallback callback) OVERRIDE;
  virtual int32_t Read(char* buffer,
                      int32_t bytes_to_read,
                      PP_CompletionCallback callback) OVERRIDE;
  virtual int32_t Write(const char* buffer,
                        int32_t bytes_to_write,
                        PP_CompletionCallback callback) OVERRIDE;
  virtual void Disconnect() OVERRIDE;

  // Notifications on operations completion.
  void OnConnectCompleted(bool succeeded,
                          const PP_NetAddress_Private& local_addr,
                          const PP_NetAddress_Private& remote_addr);
  void OnSSLHandshakeCompleted(bool succeeded);
  void OnReadCompleted(bool succeeded, const std::string& data);
  void OnWriteCompleted(bool succeeded, int32_t bytes_written);

  // Send functions that need to be implemented differently for the
  // proxied and non-proxied derived classes.
  virtual void SendConnect(const std::string& host, uint16_t port) = 0;
  virtual void SendConnectWithNetAddress(const PP_NetAddress_Private& addr) = 0;
  virtual void SendSSLHandshake(const std::string& server_name,
                                uint16_t server_port) = 0;
  virtual void SendRead(int32_t bytes_to_read) = 0;
  virtual void SendWrite(const std::string& buffer) = 0;
  virtual void SendDisconnect() = 0;

 protected:
  enum ConnectionState {
    // Before a connection is successfully established (including a connect
    // request is pending or a previous connect request failed).
    BEFORE_CONNECT,
    // A connection has been successfully established (including a request of
    // initiating SSL is pending).
    CONNECTED,
    // An SSL connection has been successfully established.
    SSL_CONNECTED,
    // The connection has been ended.
    DISCONNECTED
  };

  void Init(uint32 socket_id);
  bool IsConnected() const;
  void PostAbortAndClearIfNecessary(PP_CompletionCallback* callback);

  uint32 socket_id_;
  ConnectionState connection_state_;

  PP_CompletionCallback connect_callback_;
  PP_CompletionCallback ssl_handshake_callback_;
  PP_CompletionCallback read_callback_;
  PP_CompletionCallback write_callback_;

  char* read_buffer_;
  int32_t bytes_to_read_;

  PP_NetAddress_Private local_addr_;
  PP_NetAddress_Private remote_addr_;

  DISALLOW_COPY_AND_ASSIGN(TCPSocketPrivateImpl);
};

}  // namespace ppapi

#endif  // PPAPI_SHARED_IMPL_PRIVATE_TCP_SOCKET_PRIVATE_IMPL_H_
