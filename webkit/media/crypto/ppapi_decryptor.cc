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
#include "media/base/audio_decoder_config.h"
#include "media/base/data_buffer.h"
#include "media/base/decoder_buffer.h"
#include "media/base/video_decoder_config.h"
#include "media/base/video_frame.h"
#include "webkit/media/crypto/key_systems.h"
#include "webkit/plugins/ppapi/content_decryptor_delegate.h"
#include "webkit/plugins/ppapi/ppapi_plugin_instance.h"

namespace webkit_media {

PpapiDecryptor::PpapiDecryptor(
    const scoped_refptr<webkit::ppapi::PluginInstance>& plugin_instance,
    const media::KeyAddedCB& key_added_cb,
    const media::KeyErrorCB& key_error_cb,
    const media::KeyMessageCB& key_message_cb,
    const media::NeedKeyCB& need_key_cb)
    : plugin_instance_(plugin_instance),
      key_added_cb_(key_added_cb),
      key_error_cb_(key_error_cb),
      key_message_cb_(key_message_cb),
      need_key_cb_(need_key_cb),
      plugin_cdm_delegate_(NULL),
      render_loop_proxy_(base::MessageLoopProxy::current()),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_ptr_factory_(this)),
      weak_this_(weak_ptr_factory_.GetWeakPtr()) {
  DCHECK(plugin_instance_);
}

PpapiDecryptor::~PpapiDecryptor() {
  plugin_cdm_delegate_ = NULL;
  plugin_instance_ = NULL;
}

bool PpapiDecryptor::GenerateKeyRequest(const std::string& key_system,
                                        const std::string& type,
                                        const uint8* init_data,
                                        int init_data_length) {
  DVLOG(2) << "GenerateKeyRequest()";
  DCHECK(render_loop_proxy_->BelongsToCurrentThread());

  if (!plugin_cdm_delegate_) {
    plugin_cdm_delegate_ = plugin_instance_->GetContentDecryptorDelegate();
    if (!plugin_cdm_delegate_) {
      DVLOG(1) << "PpapiDecryptor: plugin cdm delegate creation failed.";
      return false;
    }
    plugin_cdm_delegate_->SetKeyEventCallbacks(
        base::Bind(&PpapiDecryptor::KeyAdded, weak_this_),
        base::Bind(&PpapiDecryptor::KeyError, weak_this_),
        base::Bind(&PpapiDecryptor::KeyMessage, weak_this_),
        base::Bind(&PpapiDecryptor::NeedKey, weak_this_));
  }

  if (!plugin_cdm_delegate_->GenerateKeyRequest(
      key_system, type, init_data, init_data_length)) {
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

  if (!plugin_cdm_delegate_->AddKey(
      session_id, key, key_length, init_data, init_data_length)) {
    ReportFailureToCallPlugin(key_system, session_id);
  }

  if (!new_audio_key_cb_.is_null())
    new_audio_key_cb_.Run();

  if (!new_video_key_cb_.is_null())
    new_video_key_cb_.Run();
}

void PpapiDecryptor::CancelKeyRequest(const std::string& key_system,
                                      const std::string& session_id) {
  DVLOG(2) << "CancelKeyRequest()";
  DCHECK(render_loop_proxy_->BelongsToCurrentThread());

  if (!plugin_cdm_delegate_->CancelKeyRequest(session_id))
    ReportFailureToCallPlugin(key_system, session_id);
}

void PpapiDecryptor::RegisterNewKeyCB(StreamType stream_type,
                                      const NewKeyCB& new_key_cb) {
  switch (stream_type) {
    case kAudio:
      new_audio_key_cb_ = new_key_cb;
      break;
    case kVideo:
      new_video_key_cb_ = new_key_cb;
      break;
    default:
      NOTREACHED();
  }
}

void PpapiDecryptor::Decrypt(
    StreamType stream_type,
    const scoped_refptr<media::DecoderBuffer>& encrypted,
    const DecryptCB& decrypt_cb) {
  if (!render_loop_proxy_->BelongsToCurrentThread()) {
    render_loop_proxy_->PostTask(FROM_HERE, base::Bind(
        &PpapiDecryptor::Decrypt, weak_this_,
        stream_type, encrypted, decrypt_cb));
    return;
  }

  DVLOG(3) << "Decrypt() - stream_type: " << stream_type;
  if (!plugin_cdm_delegate_->Decrypt(stream_type, encrypted, decrypt_cb))
    decrypt_cb.Run(kError, NULL);
}

void PpapiDecryptor::CancelDecrypt(StreamType stream_type) {
  DVLOG(1) << "CancelDecrypt() - stream_type: " << stream_type;
  plugin_cdm_delegate_->CancelDecrypt(stream_type);
}

void PpapiDecryptor::InitializeAudioDecoder(
      scoped_ptr<media::AudioDecoderConfig> config,
      const DecoderInitCB& init_cb) {
  if (!render_loop_proxy_->BelongsToCurrentThread()) {
    render_loop_proxy_->PostTask(FROM_HERE, base::Bind(
        &PpapiDecryptor::InitializeAudioDecoder, weak_this_,
        base::Passed(&config), init_cb));
    return;
  }

  DVLOG(2) << "InitializeAudioDecoder()";
  DCHECK(config->is_encrypted());
  DCHECK(config->IsValidConfig());

  audio_decoder_init_cb_ = init_cb;
  if (!plugin_cdm_delegate_->InitializeAudioDecoder(*config, base::Bind(
      &PpapiDecryptor::OnDecoderInitialized, weak_this_, kAudio))) {
    base::ResetAndReturn(&audio_decoder_init_cb_).Run(false);
    return;
  }
}

void PpapiDecryptor::InitializeVideoDecoder(
    scoped_ptr<media::VideoDecoderConfig> config,
    const DecoderInitCB& init_cb) {
  if (!render_loop_proxy_->BelongsToCurrentThread()) {
    render_loop_proxy_->PostTask(FROM_HERE, base::Bind(
        &PpapiDecryptor::InitializeVideoDecoder, weak_this_,
        base::Passed(&config), init_cb));
    return;
  }

  DVLOG(2) << "InitializeVideoDecoder()";
  DCHECK(config->is_encrypted());
  DCHECK(config->IsValidConfig());

  video_decoder_init_cb_ = init_cb;
  if (!plugin_cdm_delegate_->InitializeVideoDecoder(*config, base::Bind(
      &PpapiDecryptor::OnDecoderInitialized, weak_this_, kVideo))) {
    base::ResetAndReturn(&video_decoder_init_cb_).Run(false);
    return;
  }
}

void PpapiDecryptor::DecryptAndDecodeAudio(
    const scoped_refptr<media::DecoderBuffer>& encrypted,
    const AudioDecodeCB& audio_decode_cb) {
  if (!render_loop_proxy_->BelongsToCurrentThread()) {
    render_loop_proxy_->PostTask(FROM_HERE, base::Bind(
        &PpapiDecryptor::DecryptAndDecodeAudio, weak_this_,
        encrypted, audio_decode_cb));
    return;
  }

  DVLOG(3) << "DecryptAndDecodeAudio()";
  if (!plugin_cdm_delegate_->DecryptAndDecodeAudio(encrypted, audio_decode_cb))
    audio_decode_cb.Run(kError, AudioBuffers());
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
  if (!plugin_cdm_delegate_->DecryptAndDecodeVideo(encrypted, video_decode_cb))
    video_decode_cb.Run(kError, NULL);
}

void PpapiDecryptor::ResetDecoder(StreamType stream_type) {
  if (!render_loop_proxy_->BelongsToCurrentThread()) {
    render_loop_proxy_->PostTask(FROM_HERE, base::Bind(
        &PpapiDecryptor::ResetDecoder, weak_this_, stream_type));
    return;
  }

  DVLOG(2) << "ResetDecoder() - stream_type: " << stream_type;
  plugin_cdm_delegate_->ResetDecoder(stream_type);
}

void PpapiDecryptor::DeinitializeDecoder(StreamType stream_type) {
  if (!render_loop_proxy_->BelongsToCurrentThread()) {
    render_loop_proxy_->PostTask(FROM_HERE, base::Bind(
        &PpapiDecryptor::DeinitializeDecoder, weak_this_, stream_type));
    return;
  }

  DVLOG(2) << "DeinitializeDecoder() - stream_type: " << stream_type;
  plugin_cdm_delegate_->DeinitializeDecoder(stream_type);
}

void PpapiDecryptor::ReportFailureToCallPlugin(const std::string& key_system,
                                               const std::string& session_id) {
  DVLOG(1) << "Failed to call plugin.";
  key_error_cb_.Run(key_system, session_id, kUnknownError, 0);
}

void PpapiDecryptor::OnDecoderInitialized(StreamType stream_type,
                                          bool success) {
  switch (stream_type) {
    case kAudio:
      DCHECK(!audio_decoder_init_cb_.is_null());
      base::ResetAndReturn(&audio_decoder_init_cb_).Run(success);
      break;
    case kVideo:
      DCHECK(!video_decoder_init_cb_.is_null());
      base::ResetAndReturn(&video_decoder_init_cb_).Run(success);
      break;
    default:
      NOTREACHED();
  }
}

void PpapiDecryptor::KeyAdded(const std::string& key_system,
                              const std::string& session_id) {
  DCHECK(render_loop_proxy_->BelongsToCurrentThread());
  key_added_cb_.Run(key_system, session_id);
}

void PpapiDecryptor::KeyError(const std::string& key_system,
                              const std::string& session_id,
                              media::Decryptor::KeyError error_code,
                              int system_code) {
  DCHECK(render_loop_proxy_->BelongsToCurrentThread());
  key_error_cb_.Run(key_system, session_id, error_code, system_code);
}

void PpapiDecryptor::KeyMessage(const std::string& key_system,
                                const std::string& session_id,
                                const std::string& message,
                                const std::string& default_url) {
  DCHECK(render_loop_proxy_->BelongsToCurrentThread());
  key_message_cb_.Run(key_system, session_id, message, default_url);
}

void PpapiDecryptor::NeedKey(const std::string& key_system,
                             const std::string& session_id,
                             const std::string& type,
                             scoped_array<uint8> init_data,
                             int init_data_size) {
  DCHECK(render_loop_proxy_->BelongsToCurrentThread());
  need_key_cb_.Run(key_system, session_id, type,
                   init_data.Pass(), init_data_size);
}

}  // namespace webkit_media
