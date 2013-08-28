// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_TCP_SOCKET_RESOURCE_BASE_H_
#define PPAPI_PROXY_TCP_SOCKET_RESOURCE_BASE_H_

#include <queue>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "ppapi/c/ppb_tcp_socket.h"
#include "ppapi/c/private/ppb_net_address_private.h"
#include "ppapi/proxy/plugin_resource.h"
#include "ppapi/proxy/ppapi_proxy_export.h"
#include "ppapi/shared_impl/tracked_callback.h"

namespace ppapi {

class PPB_X509Certificate_Fields;
class PPB_X509Certificate_Private_Shared;
class SocketOptionData;

namespace proxy {

class PPAPI_PROXY_EXPORT TCPSocketResourceBase : public PluginResource {
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

  // C-tor used for new sockets.
  TCPSocketResourceBase(Connection connection,
                        PP_Instance instance,
                        bool private_api);

  // C-tor used for already accepted sockets.
  TCPSocketResourceBase(Connection connection,
                        PP_Instance instance,
                        bool private_api,
                        const PP_NetAddress_Private& local_addr,
                        const PP_NetAddress_Private& remote_addr);

  virtual ~TCPSocketResourceBase();

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

  bool IsConnected() const;
  void PostAbortIfNecessary(scoped_refptr<TrackedCallback>* callback);

  // IPC message handlers.
  void OnPluginMsgConnectReply(const ResourceMessageReplyParams& params,
                               const PP_NetAddress_Private& local_addr,
                               const PP_NetAddress_Private& remote_addr);
  void OnPluginMsgSSLHandshakeReply(
      const ResourceMessageReplyParams& params,
      const PPB_X509Certificate_Fields& certificate_fields);
  void OnPluginMsgReadReply(const ResourceMessageReplyParams& params,
                            const std::string& data);
  void OnPluginMsgWriteReply(const ResourceMessageReplyParams& params);
  void OnPluginMsgSetOptionReply(const ResourceMessageReplyParams& params);

  scoped_refptr<TrackedCallback> connect_callback_;
  scoped_refptr<TrackedCallback> ssl_handshake_callback_;
  scoped_refptr<TrackedCallback> read_callback_;
  scoped_refptr<TrackedCallback> write_callback_;
  std::queue<scoped_refptr<TrackedCallback> > set_option_callbacks_;

  ConnectionState connection_state_;
  char* read_buffer_;
  int32_t bytes_to_read_;

  PP_NetAddress_Private local_addr_;
  PP_NetAddress_Private remote_addr_;

  scoped_refptr<PPB_X509Certificate_Private_Shared> server_certificate_;

  std::vector<std::vector<char> > trusted_certificates_;
  std::vector<std::vector<char> > untrusted_certificates_;

 private:
  void RunCallback(scoped_refptr<TrackedCallback> callback, int32_t pp_result);

  bool private_api_;

  DISALLOW_COPY_AND_ASSIGN(TCPSocketResourceBase);
};

}  // namespace proxy
}  // namespace ppapi

#endif  // PPAPI_PROXY_TCP_SOCKET_RESOURCE_BASE_H_
