// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/media/crypto/ppapi_decryptor.h"

#include <string>

#include "base/bind.h"
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
      render_loop_proxy_(base::MessageLoopProxy::current()) {
  DCHECK(client_);
  DCHECK(cdm_plugin_);
  cdm_plugin_->set_decrypt_client(client);
}

PpapiDecryptor::~PpapiDecryptor() {
}

bool PpapiDecryptor::GenerateKeyRequest(const std::string& key_system,
                                        const uint8* init_data,
                                        int init_data_length) {
  DVLOG(1) << "GenerateKeyRequest()";
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
  DVLOG(1) << "AddKey()";
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
  DVLOG(1) << "CancelKeyRequest()";
  DCHECK(render_loop_proxy_->BelongsToCurrentThread());
  DCHECK(cdm_plugin_);

  if (!cdm_plugin_->CancelKeyRequest(session_id))
    ReportFailureToCallPlugin(key_system, session_id);
}

// TODO(xhwang): Remove Unretained in the following methods.

void PpapiDecryptor::Decrypt(
    const scoped_refptr<media::DecoderBuffer>& encrypted,
    const DecryptCB& decrypt_cb) {
  if (!render_loop_proxy_->BelongsToCurrentThread()) {
    render_loop_proxy_->PostTask(
        FROM_HERE,
        base::Bind(&PpapiDecryptor::Decrypt, base::Unretained(this),
                   encrypted, decrypt_cb));
    return;
  }

  DVLOG(1) << "Decrypt()";
  if (!cdm_plugin_->Decrypt(encrypted, decrypt_cb))
    decrypt_cb.Run(kError, NULL);
}

void PpapiDecryptor::CancelDecrypt() {
  // TODO(xhwang): Implement CancelDecrypt() in PluginInstance and call it here.
}

void PpapiDecryptor::InitializeVideoDecoder(
    scoped_ptr<media::VideoDecoderConfig> config,
    const DecoderInitCB& init_cb,
    const KeyAddedCB& key_added_cb) {
  if (!render_loop_proxy_->BelongsToCurrentThread()) {
    render_loop_proxy_->PostTask(
        FROM_HERE,
        base::Bind(&PpapiDecryptor::InitializeVideoDecoder,
                   base::Unretained(this), base::Passed(&config),
                   init_cb, key_added_cb));
    return;
  }

  DVLOG(1) << "InitializeVideoDecoder()";
  DCHECK(config->is_encrypted());
  DCHECK(config->IsValidConfig());

  key_added_cb_ = key_added_cb;

  // TODO(xhwang): Enable this once PluginInstance is updated.
  // if (!cdm_plugin_->InitializeVideoDecoder(video_config.Pass(), init_cb))
  //   init_cb.Run(false);
  init_cb.Run(false);
}

void PpapiDecryptor::DecryptAndDecodeVideo(
    const scoped_refptr<media::DecoderBuffer>& encrypted,
    const VideoDecodeCB& video_decode_cb) {
  if (!render_loop_proxy_->BelongsToCurrentThread()) {
    render_loop_proxy_->PostTask(
        FROM_HERE,
        base::Bind(&PpapiDecryptor::DecryptAndDecodeVideo,
                   base::Unretained(this), encrypted, video_decode_cb));
    return;
  }

  DVLOG(1) << "DecryptAndDecodeVideo()";
  // TODO(xhwang): Enable this once PluginInstance is updated.
  // if (!cdm_plugin_->DecryptAndDecodeVideo(encrypted, video_decode_cb))
  //   video_decode_cb.Run(kError, NULL);
  NOTIMPLEMENTED();
  video_decode_cb.Run(kError, NULL);
}

void PpapiDecryptor::CancelDecryptAndDecodeVideo() {
  DVLOG(1) << "CancelDecryptAndDecodeVideo()";
  // TODO(xhwang): Implement CancelDecryptAndDecodeVideo() in PluginInstance
  // and call it here.
  NOTIMPLEMENTED();
}

void PpapiDecryptor::StopVideoDecoder() {
  DVLOG(1) << "StopVideoDecoder()";
  // TODO(xhwang): Implement StopVideoDecoder() in PluginInstance
  // and call it here.
  NOTIMPLEMENTED();
}

void PpapiDecryptor::ReportFailureToCallPlugin(const std::string& key_system,
                                               const std::string& session_id) {
  DVLOG(1) << "Failed to call plugin.";
  client_->KeyError(key_system, session_id, kUnknownError, 0);
}

}  // namespace webkit_media
