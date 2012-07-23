// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/media/crypto/ppapi_decryptor.h"

#include "base/logging.h"
#include "media/base/decoder_buffer.h"
#include "media/base/decryptor_client.h"
#include "webkit/media/crypto/key_systems.h"
#include "webkit/plugins/ppapi/ppapi_plugin_instance.h"

namespace webkit_media {

PpapiDecryptor::PpapiDecryptor(
  media::DecryptorClient* client,
  const scoped_refptr<webkit::ppapi::PluginInstance>& plugin_instance)
    : client_(client),
      cdm_plugin_(plugin_instance) {
}

PpapiDecryptor::~PpapiDecryptor() {
}

void PpapiDecryptor::GenerateKeyRequest(const std::string& key_system,
                                        const uint8* init_data,
                                        int init_data_length) {
  DCHECK(cdm_plugin_);
  // TODO(xhwang): Enable the following once we have updated PluginInstance.
  // if (!cdm_plugin_->GenerateKeyRequest(key_system,
  //                                      init_data, init_data_length)) {
  //   client_->KeyError(key_system, "", media::Decryptor::kUnknownError, 0);
  // }
}

void PpapiDecryptor::AddKey(const std::string& key_system,
                            const uint8* key,
                            int key_length,
                            const uint8* init_data,
                            int init_data_length,
                            const std::string& session_id) {
  DCHECK(cdm_plugin_);
  // TODO(xhwang): Enable the following once we have updated PluginInstance.
  // if (!cdm_plugin_->AddKey(key_system, key, key_length,
  //                          init_data, init_data_length, session_id)) {
  //   client_->KeyError(key_system, session_id, Decryptor::kUnknownError, 0);
  // }
}

void PpapiDecryptor::CancelKeyRequest(const std::string& key_system,
                                      const std::string& session_id) {
  DCHECK(cdm_plugin_);
  // TODO(xhwang): Enable the following once we have updated PluginInstance.
  // if (!cdm_plugin_->CancelKeyRequest(key_system, session_id))
  //   client_->KeyError(key_system, session_id, Decryptor::kUnknownError, 0);
}

void PpapiDecryptor::Decrypt(
    const scoped_refptr<media::DecoderBuffer>& encrypted,
    const DecryptCB& decrypt_cb) {
  DCHECK(cdm_plugin_);
  // TODO(xhwang): Enable the following once we have updated PluginInstance.
  // TODO(xhwang): Need to figure out thread safety about PPP calls.
  // if (!cdm_plugin_->Decrypt(
  //     encrypted, base::Bind(&PpapiDecryptor::DataReady, this, decrypt_cb))) {
  //   decrypt_cb.Run(kError, NULL);
  // }
}

void PpapiDecryptor::DataReady(const DecryptCB& decrypt_cb,
                               const uint8* data, int data_size ) {
  DCHECK(!decrypt_cb.is_null());
  scoped_refptr<media::DecoderBuffer> decrypted_data =
      media::DecoderBuffer::CopyFrom(data, data_size);
  decrypt_cb.Run(kSuccess, decrypted_data);
}

}  // namespace webkit_media
