// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/media/crypto/ppapi_decryptor.h"

#include <string>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/message_loop_proxy.h"
#include "media/base/decoder_buffer.h"
#include "media/base/decryptor_client.h"
#include "media/base/video_decoder_config.h"
#include "media/base/video_frame.h"
#include "webkit/media/crypto/key_systems.h"
#include "webkit/plugins/ppapi/ppapi_plugin_instance.h"

namespace webkit_media {

PpapiDecryptor::PpapiDecryptor(
  media::DecryptorClient* client,
  const scoped_refptr<webkit::ppapi::PluginInstance>& plugin_instance)
    : client_(client),
      cdm_plugin_(plugin_instance),
      render_loop_proxy_(base::MessageLoopProxy::current()),
      weak_ptr_factory_(this),
      weak_this_(weak_ptr_factory_.GetWeakPtr()) {
  DCHECK(client_);
  DCHECK(cdm_plugin_);
  cdm_plugin_->set_decrypt_client(client);
}

PpapiDecryptor::~PpapiDecryptor() {
}

bool PpapiDecryptor::GenerateKeyRequest(const std::string& key_system,
                                        const uint8* init_data,
                                        int init_data_length) {
  DVLOG(2) << "GenerateKeyRequest()";
  DCHECK(render_loop_proxy_->BelongsToCurrentThread());
  DCHECK(cdm_plugin_);

  // TODO(xhwang): Finalize the data type for |init_data| to avoid unnecessary
  // data type conversions.
  if (!cdm_plugin_->GenerateKeyRequest(
      key_system,
      std::string(reinterpret_cast<const char*>(init_data),
                  init_data_length))) {
    ReportFailureToCallPlugin(key_system, "");
    return false;
  }

  return true;
}

void PpapiDecryptor::AddKey(const std::string& key_system,
                            const uint8* key,
                            int key_length,
                            const uint8* init_data,
                            int init_data_length,
                            const std::string& session_id) {
  DVLOG(2) << "AddKey()";
  DCHECK(render_loop_proxy_->BelongsToCurrentThread());
  DCHECK(cdm_plugin_);

  if (!cdm_plugin_->AddKey(session_id,
                           std::string(reinterpret_cast<const char*>(key),
                                       key_length),
                           std::string(reinterpret_cast<const char*>(init_data),
                                       init_data_length))) {
    ReportFailureToCallPlugin(key_system, session_id);
  }

  if (!key_added_cb_.is_null())
    key_added_cb_.Run();
}

void PpapiDecryptor::CancelKeyRequest(const std::string& key_system,
                                      const std::string& session_id) {
  DVLOG(2) << "CancelKeyRequest()";
  DCHECK(render_loop_proxy_->BelongsToCurrentThread());
  DCHECK(cdm_plugin_);

  if (!cdm_plugin_->CancelKeyRequest(session_id))
    ReportFailureToCallPlugin(key_system, session_id);
}

void PpapiDecryptor::Decrypt(
    const scoped_refptr<media::DecoderBuffer>& encrypted,
    const DecryptCB& decrypt_cb) {
  if (!render_loop_proxy_->BelongsToCurrentThread()) {
    render_loop_proxy_->PostTask(FROM_HERE, base::Bind(
        &PpapiDecryptor::Decrypt, weak_this_, encrypted, decrypt_cb));
    return;
  }

  DVLOG(3) << "Decrypt()";
  if (!cdm_plugin_->Decrypt(encrypted, decrypt_cb))
    decrypt_cb.Run(kError, NULL);
}

void PpapiDecryptor::CancelDecrypt() {
  DVLOG(1) << "CancelDecrypt()";
  // TODO(xhwang): Implement CancelDecrypt() in PluginInstance and call it here.
}

void PpapiDecryptor::InitializeVideoDecoder(
    scoped_ptr<media::VideoDecoderConfig> config,
    const DecoderInitCB& init_cb,
    const KeyAddedCB& key_added_cb) {
  if (!render_loop_proxy_->BelongsToCurrentThread()) {
    render_loop_proxy_->PostTask(FROM_HERE, base::Bind(
        &PpapiDecryptor::InitializeVideoDecoder, weak_this_,
        base::Passed(&config), init_cb, key_added_cb));
    return;
  }

  DVLOG(2) << "InitializeVideoDecoder()";
  DCHECK(config->is_encrypted());
  DCHECK(config->IsValidConfig());

  video_decoder_init_cb_ = init_cb;
  key_added_cb_ = key_added_cb;

  if (!cdm_plugin_->InitializeVideoDecoder(
      *config,
      base::Bind(&PpapiDecryptor::OnVideoDecoderInitialized, weak_this_))) {
    key_added_cb_.Reset();
    base::ResetAndReturn(&video_decoder_init_cb_).Run(false);
    return;
  }
}

void PpapiDecryptor::DecryptAndDecodeVideo(
    const scoped_refptr<media::DecoderBuffer>& encrypted,
    const VideoDecodeCB& video_decode_cb) {
  if (!render_loop_proxy_->BelongsToCurrentThread()) {
    render_loop_proxy_->PostTask(FROM_HERE, base::Bind(
        &PpapiDecryptor::DecryptAndDecodeVideo, weak_this_,
        encrypted, video_decode_cb));
    return;
  }

  DVLOG(3) << "DecryptAndDecodeVideo()";
  if (!cdm_plugin_->DecryptAndDecode(encrypted, video_decode_cb))
    video_decode_cb.Run(kError, NULL);
}

void PpapiDecryptor::CancelDecryptAndDecodeVideo() {
  if (!render_loop_proxy_->BelongsToCurrentThread()) {
    render_loop_proxy_->PostTask(FROM_HERE, base::Bind(
        &PpapiDecryptor::CancelDecryptAndDecodeVideo, weak_this_));
    return;
  }

  DVLOG(2) << "CancelDecryptAndDecodeVideo()";
  cdm_plugin_->ResetDecoder();
}

void PpapiDecryptor::StopVideoDecoder() {
  if (!render_loop_proxy_->BelongsToCurrentThread()) {
    render_loop_proxy_->PostTask(FROM_HERE, base::Bind(
        &PpapiDecryptor::StopVideoDecoder, weak_this_));
    return;
  }
  DVLOG(2) << "StopVideoDecoder()";
  cdm_plugin_->DeinitializeDecoder();
}

void PpapiDecryptor::ReportFailureToCallPlugin(const std::string& key_system,
                                               const std::string& session_id) {
  DVLOG(1) << "Failed to call plugin.";
  client_->KeyError(key_system, session_id, kUnknownError, 0);
}

void PpapiDecryptor::OnVideoDecoderInitialized(bool success) {
  DCHECK(!key_added_cb_.is_null());
  DCHECK(!video_decoder_init_cb_.is_null());

  if (!success)
    key_added_cb_.Reset();

  base::ResetAndReturn(&video_decoder_init_cb_).Run(success);
}

}  // namespace webkit_media
