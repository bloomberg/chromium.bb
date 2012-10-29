// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_PPP_CONTENT_DECRYPTOR_PRIVATE_PROXY_H_
#define PPAPI_PROXY_PPP_CONTENT_DECRYPTOR_PRIVATE_PROXY_H_

#include <string>

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
  virtual ~PPP_ContentDecryptor_Private_Proxy();

  static const PPP_ContentDecryptor_Private* GetProxyInterface();

 private:
  // InterfaceProxy implementation.
  virtual bool OnMessageReceived(const IPC::Message& msg);

  // Message handlers.
  void OnMsgGenerateKeyRequest(PP_Instance instance,
                               SerializedVarReceiveInput key_system,
                               SerializedVarReceiveInput type,
                               SerializedVarReceiveInput init_data);
  void OnMsgAddKey(PP_Instance instance,
                   SerializedVarReceiveInput session_id,
                   SerializedVarReceiveInput key,
                   SerializedVarReceiveInput init_data);
  void OnMsgCancelKeyRequest(PP_Instance instance,
                             SerializedVarReceiveInput session_id);
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
