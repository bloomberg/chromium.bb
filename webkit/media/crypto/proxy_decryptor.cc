// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/media/crypto/proxy_decryptor.h"

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/message_loop_proxy.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/decoder_buffer.h"
#include "media/base/decryptor_client.h"
#include "media/base/video_decoder_config.h"
#include "media/base/video_frame.h"
#include "media/crypto/aes_decryptor.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebString.h"
#include "webkit/media/crypto/key_systems.h"
#include "webkit/media/crypto/ppapi_decryptor.h"
#include "webkit/plugins/ppapi/ppapi_plugin_instance.h"
#include "webkit/plugins/ppapi/ppapi_webplugin_impl.h"
// TODO(xhwang): Put this include after "ppapi_plugin_instance.h" for definition
// of "uint8_t", which WebMediaPlayer.h uses without including a header for it.
// See: https://bugs.webkit.org/show_bug.cgi?id=92031
// Fix include order here when the bug is fixed.
#include "third_party/WebKit/Source/WebKit/chromium/public/WebMediaPlayerClient.h"

namespace webkit_media {

static scoped_refptr<webkit::ppapi::PluginInstance> CreatePluginInstance(
    const std::string& plugin_type,
    WebKit::WebMediaPlayerClient* web_media_player_client,
    WebKit::WebFrame* web_frame) {
  DCHECK(web_media_player_client);
  DCHECK(web_frame);

  WebKit::WebPlugin* web_plugin = web_media_player_client->createHelperPlugin(
      WebKit::WebString::fromUTF8(plugin_type), web_frame);
  if (!web_plugin)
    return NULL;

  DCHECK(!web_plugin->isPlaceholder());  // Prevented by WebKit.
  // Only Pepper plugins are supported, so it must be a ppapi object.
  webkit::ppapi::WebPluginImpl* ppapi_plugin =
      static_cast<webkit::ppapi::WebPluginImpl*>(web_plugin);
  return ppapi_plugin->instance();
}

// TODO(xhwang): Simplify this function. This is mostly caused by the fact that
// we need to copy a scoped_array<uint8>.
static void FireNeedKey(media::DecryptorClient* client,
                        const scoped_refptr<media::DecoderBuffer>& encrypted) {
  DCHECK(client);
  DCHECK(encrypted);
  DCHECK(encrypted->GetDecryptConfig());
  std::string key_id = encrypted->GetDecryptConfig()->key_id();
  scoped_array<uint8> key_id_array(new uint8[key_id.size()]);
  memcpy(key_id_array.get(), key_id.data(), key_id.size());
  client->NeedKey("", "", "", key_id_array.Pass(), key_id.size());
}

ProxyDecryptor::ProxyDecryptor(
    const scoped_refptr<base::MessageLoopProxy>& message_loop,
    media::DecryptorClient* decryptor_client,
    WebKit::WebMediaPlayerClient* web_media_player_client,
    WebKit::WebFrame* web_frame)
    : decryption_message_loop_(message_loop),
      client_(decryptor_client),
      web_media_player_client_(web_media_player_client),
      web_frame_(web_frame),
      is_waiting_for_decryptor_(false),
      is_canceling_decrypt_(false),
      has_new_key_added_(false) {
}

ProxyDecryptor::~ProxyDecryptor() {
}

// TODO(xhwang): Support multiple decryptor notification request (e.g. from
// video and audio decoders). The current implementation is okay for the current
// media pipeline since we initialize audio and video decoders in sequence.
// But ProxyDecryptor should not depend on media pipeline's implementation
// detail.
void ProxyDecryptor::SetDecryptorReadyCB(
     const media::DecryptorReadyCB& decryptor_ready_cb) {
  base::AutoLock auto_lock(lock_);

  // Cancels the previous decryptor request.
  if (decryptor_ready_cb.is_null()) {
    if (!decryptor_ready_cb_.is_null())
      base::ResetAndReturn(&decryptor_ready_cb_).Run(NULL);
    return;
  }

  // Normal decryptor request.
  DCHECK(decryptor_ready_cb_.is_null());
  if (decryptor_) {
    decryptor_ready_cb.Run(decryptor_.get());
    return;
  }
  decryptor_ready_cb_ = decryptor_ready_cb;
}

bool ProxyDecryptor::GenerateKeyRequest(const std::string& key_system,
                                        const std::string& type,
                                        const uint8* init_data,
                                        int init_data_length) {
  // We do not support run-time switching of decryptors. GenerateKeyRequest()
  // only creates a new decryptor when |decryptor_| is not initialized.
  DVLOG(1) << "GenerateKeyRequest: key_system = " << key_system;

  base::AutoLock auto_lock(lock_);

  if (!decryptor_) {
    decryptor_ = CreateDecryptor(key_system);
    if (!decryptor_) {
      client_->KeyError(key_system, "", media::Decryptor::kUnknownError, 0);
      return false;
    }
  }

  if (!decryptor_->GenerateKeyRequest(key_system, type,
                                      init_data, init_data_length)) {
    decryptor_.reset();
    return false;
  }

  if (!decryptor_ready_cb_.is_null())
    base::ResetAndReturn(&decryptor_ready_cb_).Run(decryptor_.get());

  return true;
}

void ProxyDecryptor::AddKey(const std::string& key_system,
                            const uint8* key,
                            int key_length,
                            const uint8* init_data,
                            int init_data_length,
                            const std::string& session_id) {
  DVLOG(1) << "AddKey()";

  // WebMediaPlayerImpl ensures GenerateKeyRequest() has been called.
  decryptor_->AddKey(key_system, key, key_length, init_data, init_data_length,
                     session_id);

  OnNewKeyAdded();
}

void ProxyDecryptor::CancelKeyRequest(const std::string& key_system,
                                      const std::string& session_id) {
  DVLOG(1) << "CancelKeyRequest()";

  // WebMediaPlayerImpl ensures GenerateKeyRequest() has been called.
  decryptor_->CancelKeyRequest(key_system, session_id);
}

void ProxyDecryptor::RegisterKeyAddedCB(StreamType stream_type,
                                        const KeyAddedCB& key_added_cb) {
  NOTREACHED() << "KeyAddedCB should not be registered with ProxyDecryptor.";
}

void ProxyDecryptor::Decrypt(
    StreamType stream_type,
    const scoped_refptr<media::DecoderBuffer>& encrypted,
    const DecryptCB& decrypt_cb) {
  DVLOG(2) << "Decrypt()";
  DCHECK(decryption_message_loop_->BelongsToCurrentThread());
  DCHECK_EQ(stream_type, kVideo);  // Only support video decrypt-only for now.

  DCHECK(!is_canceling_decrypt_);
  DCHECK(!pending_buffer_to_decrypt_);
  DCHECK(pending_decrypt_cb_.is_null());

  pending_buffer_to_decrypt_ = encrypted;
  pending_decrypt_cb_ = decrypt_cb;

  // This is safe as we do not replace/delete an existing decryptor at run-time.
  media::Decryptor* decryptor = NULL;
  {
    base::AutoLock auto_lock(lock_);
    decryptor = decryptor_.get();
  }
  if (!decryptor) {
    DVLOG(1) << "Decrypt(): decryptor not initialized.";

    // TODO(xhwang): The same NeedKey may be fired here and multiple times in
    // OnBufferDecrypted(). While the spec says only one NeedKey should be
    // fired. Leave them as is since the spec about this may change.
    FireNeedKey(client_, encrypted);
    return;
  }

  DecryptPendingBuffer();
}

void ProxyDecryptor::CancelDecrypt(StreamType stream_type) {
  DVLOG(1) << "CancelDecrypt()";
  DCHECK(decryption_message_loop_->BelongsToCurrentThread());
  DCHECK_EQ(stream_type, kVideo);  // Only support video decrypt-only for now.

  if (!pending_buffer_to_decrypt_) {
    DCHECK(pending_decrypt_cb_.is_null());
    DCHECK(!is_waiting_for_decryptor_);
    return;
  }

  DecryptCB decrypt_cb;
  if (!is_waiting_for_decryptor_) {
    pending_buffer_to_decrypt_ = NULL;
    base::ResetAndReturn(&pending_decrypt_cb_).Run(kSuccess, NULL);
    return;
  }

  is_canceling_decrypt_ = true;
  decryptor_->CancelDecrypt(stream_type);
}

void ProxyDecryptor::InitializeAudioDecoder(
    scoped_ptr<media::AudioDecoderConfig> config,
    const DecoderInitCB& init_cb) {
  NOTREACHED() << "ProxyDecryptor does not support audio decoding";
}

void ProxyDecryptor::InitializeVideoDecoder(
    scoped_ptr<media::VideoDecoderConfig> config,
    const DecoderInitCB& init_cb) {
  NOTREACHED() << "ProxyDecryptor does not support video decoding";
}

void ProxyDecryptor::DecryptAndDecodeAudio(
    const scoped_refptr<media::DecoderBuffer>& encrypted,
    const AudioDecodeCB& audio_decode_cb) {
  NOTREACHED() << "ProxyDecryptor does not support audio decoding";
}

void ProxyDecryptor::DecryptAndDecodeVideo(
    const scoped_refptr<media::DecoderBuffer>& encrypted,
    const VideoDecodeCB& video_decode_cb) {
  NOTREACHED() << "ProxyDecryptor does not support video decoding";
}

void ProxyDecryptor::ResetDecoder(StreamType stream_type) {
  NOTREACHED() << "ProxyDecryptor does not support audio/video decoding";
}

void ProxyDecryptor::DeinitializeDecoder(StreamType stream_type) {
  NOTREACHED() << "ProxyDecryptor does not support audio/video decoding";
}

scoped_ptr<media::Decryptor> ProxyDecryptor::CreatePpapiDecryptor(
    const std::string& key_system) {
  DCHECK(client_);
  DCHECK(web_media_player_client_);
  DCHECK(web_frame_);

  std::string plugin_type = GetPluginType(key_system);
  DCHECK(!plugin_type.empty());
  const scoped_refptr<webkit::ppapi::PluginInstance>& plugin_instance =
    CreatePluginInstance(plugin_type, web_media_player_client_, web_frame_);
  if (!plugin_instance) {
    DVLOG(1) << "ProxyDecryptor: plugin instance creation failed.";
    return scoped_ptr<media::Decryptor>();
  }

  return scoped_ptr<media::Decryptor>(new PpapiDecryptor(client_,
                                                         plugin_instance));
}

scoped_ptr<media::Decryptor> ProxyDecryptor::CreateDecryptor(
    const std::string& key_system) {
  DCHECK(client_);

  if (CanUseAesDecryptor(key_system))
    return scoped_ptr<media::Decryptor>(new media::AesDecryptor(client_));

  // We only support AesDecryptor and PpapiDecryptor. So if we cannot
  // use the AesDecryptor, then we'll try to create a PpapiDecryptor for given
  // |key_system|.
  return CreatePpapiDecryptor(key_system);
}

void ProxyDecryptor::OnNewKeyAdded() {
  if (!decryption_message_loop_->BelongsToCurrentThread()) {
    // TODO(xhwang): Using base::Unretained may cause race during destruction
    // in the future. Currently it's not a race because ~ProxyDecryptor() is
    // invoked after |decryption_message_loop_| is stopped.
    // Fix this with WeakPtr when decryption-related calls are separated from
    // key/session-related calls.
    decryption_message_loop_->PostTask(FROM_HERE, base::Bind(
        &ProxyDecryptor::OnNewKeyAdded, base::Unretained(this)));
    return;
  }

  DCHECK(decryptor_);

  if (!pending_buffer_to_decrypt_)
    return;

  if (is_waiting_for_decryptor_) {
    has_new_key_added_ = true;
    return;
  }

  DecryptPendingBuffer();
}

void ProxyDecryptor::DecryptPendingBuffer() {
  DVLOG(3) << "DecryptPendingBuffer()";
  DCHECK(decryption_message_loop_->BelongsToCurrentThread());
  DCHECK(pending_buffer_to_decrypt_);
  DCHECK(!is_waiting_for_decryptor_);

  is_waiting_for_decryptor_ = true;
  decryptor_->Decrypt(
      kVideo,  // Only support video decrypt-only for now.
      pending_buffer_to_decrypt_,
      base::Bind(&ProxyDecryptor::OnBufferDecrypted, base::Unretained(this)));
}

void ProxyDecryptor::OnBufferDecrypted(
    media::Decryptor::Status status,
    const scoped_refptr<media::DecoderBuffer>& decrypted) {
  if (!decryption_message_loop_->BelongsToCurrentThread()) {
    decryption_message_loop_->PostTask(FROM_HERE, base::Bind(
        &ProxyDecryptor::OnBufferDecrypted, base::Unretained(this),
        status, decrypted));
    return;
  }

  DVLOG(2) << "OnBufferDecrypted() - status: " << status;
  DCHECK(pending_buffer_to_decrypt_);
  DCHECK(!pending_decrypt_cb_.is_null());
  DCHECK(is_waiting_for_decryptor_);

  is_waiting_for_decryptor_ = false;
  bool need_to_try_again_if_nokey_is_returned = has_new_key_added_;
  has_new_key_added_ = false;

  if (is_canceling_decrypt_) {
    pending_buffer_to_decrypt_ = NULL;
    base::ResetAndReturn(&pending_decrypt_cb_).Run(kSuccess, NULL);
    is_canceling_decrypt_ = false;
    return;
  }

  if (status == media::Decryptor::kSuccess ||
      status == media::Decryptor::kError) {
    pending_buffer_to_decrypt_ = NULL;
    base::ResetAndReturn(&pending_decrypt_cb_).Run(status, decrypted);
    return;
  }

  DCHECK_EQ(status, media::Decryptor::kNoKey);
  DVLOG(1) << "OnBufferDecrypted(): kNoKey fired";
  if (need_to_try_again_if_nokey_is_returned) {
    DecryptPendingBuffer();
    return;
  }

  // TODO(xhwang): The same NeedKey may be fired multiple times here and also
  // in Decrypt(). While the spec says only one NeedKey should be fired. Leave
  // them as is since the spec about this may change.
  FireNeedKey(client_, pending_buffer_to_decrypt_);
}

}  // namespace webkit_media
