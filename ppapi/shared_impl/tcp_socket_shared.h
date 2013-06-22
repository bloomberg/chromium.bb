// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_SHARED_IMPL_TCP_SOCKET_SHARED_H_
#define PPAPI_SHARED_IMPL_TCP_SOCKET_SHARED_H_

#include <queue>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "ppapi/c/ppb_tcp_socket.h"
#include "ppapi/c/private/ppb_net_address_private.h"
#include "ppapi/shared_impl/resource.h"
#include "ppapi/shared_impl/tracked_callback.h"

namespace ppapi {

class PPB_X509Certificate_Fields;
class PPB_X509Certificate_Private_Shared;
class SocketOptionData;

// This class provides the shared implementation for both PPB_TCPSocket and
// PPB_TCPSocket_Private.
class PPAPI_SHARED_EXPORT TCPSocketShared {
 public:
  // The maximum number of bytes that each PpapiHostMsg_PPBTCPSocket_Read
  // message is allowed to request.
  static const int32_t kMaxReadSize;
  // The maximum number of bytes that each PpapiHostMsg_PPBTCPSocket_Write
  // message is allowed to carry.
  static const int32_t kMaxWriteSize;

  // The maximum number that we allow for setting
  // PP_TCPSOCKET_OPTION_SEND_BUFFER_SIZE. This number is only for input
  // argument sanity check, it doesn't mean the browser guarantees to support
  // such a buffer size.
  static const int32_t kMaxSendBufferSize;
  // The maximum number that we allow for setting
  // PP_TCPSOCKET_OPTION_RECV_BUFFER_SIZE. This number is only for input
  // argument sanity check, it doesn't mean the browser guarantees to support
  // such a buffer size.
  static const int32_t kMaxReceiveBufferSize;

  // Notifications on operations completion.
  void OnConnectCompleted(int32_t result,
                          const PP_NetAddress_Private& local_addr,
                          const PP_NetAddress_Private& remote_addr);
  void OnSSLHandshakeCompleted(
      bool succeeded,
      const PPB_X509Certificate_Fields& certificate_fields);
  void OnReadCompleted(int32_t result, const std::string& data);
  void OnWriteCompleted(int32_t result);
  void OnSetOptionCompleted(int32_t result);

  // Send functions that need to be implemented differently for the
  // proxied and non-proxied derived classes.
  virtual void SendConnect(const std::string& host, uint16_t port) = 0;
  virtual void SendConnectWithNetAddress(const PP_NetAddress_Private& addr) = 0;
  virtual void SendSSLHandshake(
      const std::string& server_name,
      uint16_t server_port,
      const std::vector<std::vector<char> >& trusted_certs,
      const std::vector<std::vector<char> >& untrusted_certs) = 0;
  virtual void SendRead(int32_t bytes_to_read) = 0;
  virtual void SendWrite(const std::string& buffer) = 0;
  virtual void SendDisconnect() = 0;
  virtual void SendSetOption(PP_TCPSocket_Option name,
                             const SocketOptionData& value) = 0;

  virtual Resource* GetOwnerResource() = 0;

  // Used to override PP_Error codes received from the browser side.
  virtual int32_t OverridePPError(int32_t pp_error);

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

  TCPSocketShared(ResourceObjectType resource_type, uint32 socket_id);
  virtual ~TCPSocketShared();

  int32_t ConnectImpl(const char* host,
                      uint16_t port,
                      scoped_refptr<TrackedCallback> callback);
  int32_t ConnectWithNetAddressImpl(const PP_NetAddress_Private* addr,
                                    scoped_refptr<TrackedCallback> callback);
  PP_Bool GetLocalAddressImpl(PP_NetAddress_Private* local_addr);
  PP_Bool GetRemoteAddressImpl(PP_NetAddress_Private* remote_addr);
  int32_t SSLHandshakeImpl(const char* server_name,
                           uint16_t server_port,
                           scoped_refptr<TrackedCallback> callback);
  PP_Resource GetServerCertificateImpl();
  PP_Bool AddChainBuildingCertificateImpl(PP_Resource certificate,
                                          PP_Bool trusted);
  int32_t ReadImpl(char* buffer,
                   int32_t bytes_to_read,
                   scoped_refptr<TrackedCallback> callback);
  int32_t WriteImpl(const char* buffer,
                    int32_t bytes_to_write,
                    scoped_refptr<TrackedCallback> callback);
  void DisconnectImpl();
  int32_t SetOptionImpl(PP_TCPSocket_Option name,
                        const PP_Var& value,
                        scoped_refptr<TrackedCallback> callback);

  void Init(uint32 socket_id);
  bool IsConnected() const;
  void PostAbortIfNecessary(scoped_refptr<TrackedCallback>* callback);

  ResourceObjectType resource_type_;

  uint32 socket_id_;
  ConnectionState connection_state_;

  scoped_refptr<TrackedCallback> connect_callback_;
  scoped_refptr<TrackedCallback> ssl_handshake_callback_;
  scoped_refptr<TrackedCallback> read_callback_;
  scoped_refptr<TrackedCallback> write_callback_;
  std::queue<scoped_refptr<TrackedCallback> > set_option_callbacks_;

  char* read_buffer_;
  int32_t bytes_to_read_;

  PP_NetAddress_Private local_addr_;
  PP_NetAddress_Private remote_addr_;

  scoped_refptr<PPB_X509Certificate_Private_Shared> server_certificate_;

  std::vector<std::vector<char> > trusted_certificates_;
  std::vector<std::vector<char> > untrusted_certificates_;

 private:
  DISALLOW_COPY_AND_ASSIGN(TCPSocketShared);
};

}  // namespace ppapi

#endif  // PPAPI_SHARED_IMPL_TCP_SOCKET_SHARED_H_
