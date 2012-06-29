// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/media/crypto/proxy_decryptor.h"

#include "base/logging.h"
#include "media/base/decoder_buffer.h"
#include "media/base/decryptor_client.h"
#include "media/crypto/aes_decryptor.h"
#include "webkit/media/crypto/key_systems.h"

namespace webkit_media {

ProxyDecryptor::ProxyDecryptor(media::DecryptorClient* client)
    : client_(client) {
}

ProxyDecryptor::~ProxyDecryptor() {
}

void ProxyDecryptor::GenerateKeyRequest(const std::string& key_system,
                                        const uint8* init_data,
                                        int init_data_length) {
  // We do not support run-time switching of decryptors. GenerateKeyRequest()
  // only creates a new decryptor when |decryptor_| is not initialized.
  if (!decryptor_.get()) {
    base::AutoLock auto_lock(lock_);
    decryptor_ = CreateDecryptor(key_system, client_);
  }

  DCHECK(decryptor_.get());
  decryptor_->GenerateKeyRequest(key_system, init_data, init_data_length);
}

void ProxyDecryptor::AddKey(const std::string& key_system,
                            const uint8* key,
                            int key_length,
                            const uint8* init_data,
                            int init_data_length,
                            const std::string& session_id) {
  // WebMediaPlayerImpl ensures GenerateKeyRequest() has been called.
  DCHECK(decryptor_.get());
  decryptor_->AddKey(key_system, key, key_length, init_data, init_data_length,
                     session_id);
}

void ProxyDecryptor::CancelKeyRequest(const std::string& key_system,
                                      const std::string& session_id) {
  // WebMediaPlayerImpl ensures GenerateKeyRequest() has been called.
  DCHECK(decryptor_.get());
  decryptor_->CancelKeyRequest(key_system, session_id);
}

scoped_refptr<media::DecoderBuffer> ProxyDecryptor::Decrypt(
    const scoped_refptr<media::DecoderBuffer>& input) {
  // This is safe as we do not replace/delete an existing decryptor at run-time.
  Decryptor* decryptor = NULL;
  {
    base::AutoLock auto_lock(lock_);
    decryptor = decryptor_.get();
  }
  if (!decryptor)
    return NULL;

  return decryptor->Decrypt(input);
}

}  // namespace webkit_media
