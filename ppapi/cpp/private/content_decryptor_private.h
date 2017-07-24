// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_CPP_PRIVATE_CONTENT_DECRYPTOR_PRIVATE_H_
#define PPAPI_CPP_PRIVATE_CONTENT_DECRYPTOR_PRIVATE_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "ppapi/c/private/pp_content_decryptor.h"
#include "ppapi/c/private/ppb_content_decryptor_private.h"
#include "ppapi/c/private/ppp_content_decryptor_private.h"
#include "ppapi/cpp/dev/buffer_dev.h"
#include "ppapi/cpp/instance_handle.h"
#include "ppapi/cpp/var.h"
#include "ppapi/cpp/var_array_buffer.h"

namespace pp {

class Instance;

// TODO(tomfinegan): Remove redundant pp:: usage, and pass VarArrayBuffers as
// const references.

class ContentDecryptor_Private {
 public:
  explicit ContentDecryptor_Private(Instance* instance);
  virtual ~ContentDecryptor_Private();

  // PPP_ContentDecryptor_Private functions exposed as virtual functions
  // for you to override.
  // TODO(tomfinegan): This could be optimized to pass pp::Var instead of
  // strings. The change would allow the CDM wrapper to reuse vars when
  // replying to the browser.
  virtual void Initialize(uint32_t promise_id,
                          const std::string& key_system,
                          bool allow_distinctive_identifier,
                          bool allow_persistent_state) = 0;
  virtual void SetServerCertificate(uint32_t promise_id,
                                    pp::VarArrayBuffer server_certificate) = 0;
  virtual void GetStatusForPolicy(uint32_t promise_id,
                                  PP_HdcpVersion min_hdcp_version) = 0;
  virtual void CreateSessionAndGenerateRequest(
      uint32_t promise_id,
      PP_SessionType session_type,
      PP_InitDataType init_data_type,
      pp::VarArrayBuffer init_data) = 0;
  virtual void LoadSession(uint32_t promise_id,
                           PP_SessionType session_type,
                           const std::string& session_id) = 0;
  virtual void UpdateSession(uint32_t promise_id,
                             const std::string& session_id,
                             pp::VarArrayBuffer response) = 0;
  virtual void CloseSession(uint32_t promise_id,
                            const std::string& session_id) = 0;
  virtual void RemoveSession(uint32_t promise_id,
                             const std::string& session_id) = 0;
  virtual void Decrypt(pp::Buffer_Dev encrypted_buffer,
                       const PP_EncryptedBlockInfo& encrypted_block_info) = 0;
  virtual void InitializeAudioDecoder(
      const PP_AudioDecoderConfig& decoder_config,
      pp::Buffer_Dev extra_data_resource) = 0;
  virtual void InitializeVideoDecoder(
      const PP_VideoDecoderConfig& decoder_config,
      pp::Buffer_Dev extra_data_resource) = 0;
  virtual void DeinitializeDecoder(PP_DecryptorStreamType decoder_type,
                                   uint32_t request_id) = 0;
  virtual void ResetDecoder(PP_DecryptorStreamType decoder_type,
                            uint32_t request_id) = 0;
  // Null |encrypted_frame| means end-of-stream buffer.
  virtual void DecryptAndDecode(
      PP_DecryptorStreamType decoder_type,
      pp::Buffer_Dev encrypted_buffer,
      const PP_EncryptedBlockInfo& encrypted_block_info) = 0;

  // PPB_ContentDecryptor_Private methods for passing data from the decryptor
  // to the browser.
  void PromiseResolved(uint32_t promise_id);
  void PromiseResolvedWithKeyStatus(uint32_t promise_id,
                                    PP_CdmKeyStatus key_status);
  void PromiseResolvedWithSession(uint32_t promise_id,
                                  const std::string& session_id);
  void PromiseRejected(uint32_t promise_id,
                       PP_CdmExceptionCode exception_code,
                       uint32_t system_code,
                       const std::string& error_description);
  void SessionMessage(const std::string& session_id,
                      PP_CdmMessageType message_type,
                      pp::VarArrayBuffer message,
                      const std::string& legacy_destination_url);
  void SessionKeysChange(const std::string& session_id,
                         bool has_additional_usable_key,
                         const std::vector<PP_KeyInformation>& key_information);
  void SessionExpirationChange(const std::string& session_id,
                               PP_Time new_expiry_time);
  void SessionClosed(const std::string& session_id);
  void LegacySessionError(const std::string& session_id,
                          PP_CdmExceptionCode exception_code,
                          uint32_t system_code,
                          const std::string& error_description);

  // The plugin must not hold a reference to the encrypted buffer resource
  // provided to Decrypt() when it calls this method. The browser will reuse
  // the buffer in a subsequent Decrypt() call.
  void DeliverBlock(pp::Buffer_Dev decrypted_block,
                    const PP_DecryptedBlockInfo& decrypted_block_info);

  void DecoderInitializeDone(PP_DecryptorStreamType decoder_type,
                             uint32_t request_id,
                             bool status);
  void DecoderDeinitializeDone(PP_DecryptorStreamType decoder_type,
                               uint32_t request_id);
  void DecoderResetDone(PP_DecryptorStreamType decoder_type,
                        uint32_t request_id);

  // The plugin must not hold a reference to the encrypted buffer resource
  // provided to DecryptAndDecode() when it calls this method. The browser will
  // reuse the buffer in a subsequent DecryptAndDecode() call.
  void DeliverFrame(pp::Buffer_Dev decrypted_frame,
                    const PP_DecryptedFrameInfo& decrypted_frame_info);

  // The plugin must not hold a reference to the encrypted buffer resource
  // provided to DecryptAndDecode() when it calls this method. The browser will
  // reuse the buffer in a subsequent DecryptAndDecode() call.
  void DeliverSamples(pp::Buffer_Dev audio_frames,
                      const PP_DecryptedSampleInfo& decrypted_sample_info);

 private:
  InstanceHandle associated_instance_;
};

}  // namespace pp

#endif  // PPAPI_CPP_PRIVATE_CONTENT_DECRYPTOR_PRIVATE_H_
