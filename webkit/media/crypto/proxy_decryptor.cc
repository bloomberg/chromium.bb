// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/media/crypto/proxy_decryptor.h"

#include "base/logging.h"
#include "media/base/decoder_buffer.h"
#include "media/base/decryptor_client.h"
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

ProxyDecryptor::ProxyDecryptor(
    media::DecryptorClient* decryptor_client,
    WebKit::WebMediaPlayerClient* web_media_player_client,
    WebKit::WebFrame* web_frame)
    : client_(decryptor_client),
      web_media_player_client_(web_media_player_client),
      web_frame_(web_frame) {
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
    decryptor_ = CreateDecryptor(key_system);
  }

  DCHECK(client_);
  if (!decryptor_.get()) {
    client_->KeyError(key_system, "", media::Decryptor::kUnknownError, 0);
    return;
  }

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

void ProxyDecryptor::Decrypt(
    const scoped_refptr<media::DecoderBuffer>& encrypted,
    const DecryptCB& decrypt_cb) {
  // This is safe as we do not replace/delete an existing decryptor at run-time.
  media::Decryptor* decryptor = NULL;
  {
    base::AutoLock auto_lock(lock_);
    decryptor = decryptor_.get();
  }
  if (!decryptor) {
    DVLOG(1) << "ProxyDecryptor::Decrypt(): decryptor not initialized.";
    decrypt_cb.Run(kError, NULL);
    return;
  }

  decryptor->Decrypt(encrypted, decrypt_cb);
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
    DVLOG(1) << "PpapiDecryptor: plugin instance creation failed.";
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

}  // namespace webkit_media
