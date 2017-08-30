// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_PPP_CONTENT_DECRYPTOR_PRIVATE_PROXY_H_
#define PPAPI_PROXY_PPP_CONTENT_DECRYPTOR_PRIVATE_PROXY_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "base/macros.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/private/ppp_content_decryptor_private.h"
#include "ppapi/proxy/interface_proxy.h"
#include "ppapi/proxy/serialized_structs.h"
#include "ppapi/shared_impl/host_resource.h"

namespace ppapi {
namespace proxy {

class SerializedVarReceiveInput;

class PPP_ContentDecryptor_Private_Proxy : public InterfaceProxy {
 public:
  explicit PPP_ContentDecryptor_Private_Proxy(Dispatcher* dispatcher);
  ~PPP_ContentDecryptor_Private_Proxy() override;

  static const PPP_ContentDecryptor_Private* GetProxyInterface();

 private:
  // InterfaceProxy implementation.
  bool OnMessageReceived(const IPC::Message& msg) override;

  // Message handlers.
  void OnMsgInitialize(PP_Instance instance,
                       uint32_t promise_id,
                       SerializedVarReceiveInput key_system,
                       PP_Bool allow_distinctive_identifier,
                       PP_Bool allow_persistent_state);
  void OnMsgSetServerCertificate(PP_Instance instance,
                                 uint32_t promise_id,
                                 std::vector<uint8_t> server_certificate);
  void OnMsgGetStatusForPolicy(PP_Instance instance,
                               uint32_t promise_id,
                               PP_HdcpVersion min_hdcp_version);
  void OnMsgCreateSessionAndGenerateRequest(
      PP_Instance instance,
      uint32_t promise_id,
      PP_SessionType session_type,
      PP_InitDataType init_data_type,
      SerializedVarReceiveInput init_data);
  void OnMsgLoadSession(PP_Instance instance,
                        uint32_t promise_id,
                        PP_SessionType session_type,
                        SerializedVarReceiveInput session_id);
  void OnMsgUpdateSession(PP_Instance instance,
                          uint32_t promise_id,
                          SerializedVarReceiveInput session_id,
                          SerializedVarReceiveInput response);
  void OnMsgCloseSession(PP_Instance instance,
                         uint32_t promise_id,
                         const std::string& session_id);
  void OnMsgRemoveSession(PP_Instance instance,
                          uint32_t promise_id,
                          const std::string& session_id);
  void OnMsgDecrypt(PP_Instance instance,
                    const PPPDecryptor_Buffer& encrypted_buffer,
                    const std::string& serialized_encrypted_block_info);
  void OnMsgInitializeAudioDecoder(
      PP_Instance instance,
      const std::string& decoder_config,
      const PPPDecryptor_Buffer& extra_data_buffer);
  void OnMsgInitializeVideoDecoder(
      PP_Instance instance,
      const std::string& decoder_config,
      const PPPDecryptor_Buffer& extra_data_buffer);
  void OnMsgDeinitializeDecoder(PP_Instance instance,
                                PP_DecryptorStreamType decoder_type,
                                uint32_t request_id);
  void OnMsgResetDecoder(PP_Instance instance,
                         PP_DecryptorStreamType decoder_type,
                         uint32_t request_id);
  void OnMsgDecryptAndDecode(
      PP_Instance instance,
      PP_DecryptorStreamType decoder_type,
      const PPPDecryptor_Buffer& encrypted_buffer,
      const std::string& serialized_encrypted_block_info);

  const PPP_ContentDecryptor_Private* ppp_decryptor_impl_;

  DISALLOW_COPY_AND_ASSIGN(PPP_ContentDecryptor_Private_Proxy);
};

}  // namespace proxy
}  // namespace ppapi

#endif  // PPAPI_PROXY_PPP_CONTENT_DECRYPTOR_PRIVATE_PROXY_H_
