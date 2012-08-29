// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/media/crypto/proxy_decryptor.h"

#include <algorithm>

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/message_loop_proxy.h"
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
  client->NeedKey("", "", key_id_array.Pass(), key_id.size());
}

ProxyDecryptor::ProxyDecryptor(
    media::DecryptorClient* decryptor_client,
    WebKit::WebMediaPlayerClient* web_media_player_client,
    WebKit::WebFrame* web_frame)
    : client_(decryptor_client),
      web_media_player_client_(web_media_player_client),
      web_frame_(web_frame),
      stopped_(false) {
}

ProxyDecryptor::~ProxyDecryptor() {
}

bool ProxyDecryptor::GenerateKeyRequest(const std::string& key_system,
                                        const uint8* init_data,
                                        int init_data_length) {
  // We do not support run-time switching of decryptors. GenerateKeyRequest()
  // only creates a new decryptor when |decryptor_| is not initialized.
  DVLOG(1) << "GenerateKeyRequest: key_system = " << key_system;
  if (!decryptor_.get()) {
    base::AutoLock auto_lock(lock_);
    decryptor_ = CreateDecryptor(key_system);
  }

  if (!decryptor_.get()) {
    client_->KeyError(key_system, "", media::Decryptor::kUnknownError, 0);
    return false;
  }

  if(!decryptor_->GenerateKeyRequest(key_system, init_data, init_data_length)) {
    decryptor_.reset();
    return false;
  }

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
  DCHECK(decryptor_.get());
  decryptor_->AddKey(key_system, key, key_length, init_data, init_data_length,
                     session_id);

  std::vector<base::Closure> closures_to_run;
  {
    base::AutoLock auto_lock(lock_);
    std::swap(pending_decrypt_closures_, closures_to_run);
  }

  // Fire all pending callbacks here because only the |decryptor_| knows if the
  // pending buffers can be decrypted or not.
  for (std::vector<base::Closure>::iterator iter = closures_to_run.begin();
       iter != closures_to_run.end();
       ++iter) {
    iter->Run();
  }
}

void ProxyDecryptor::CancelKeyRequest(const std::string& key_system,
                                      const std::string& session_id) {
  DVLOG(1) << "CancelKeyRequest()";

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
    if (stopped_) {
      DVLOG(1) << "Decrypt(): fire decrypt callbacks with kError.";
      decrypt_cb.Run(kError, NULL);
      return;
    }
    decryptor = decryptor_.get();
    if (!decryptor) {
      DVLOG(1) << "ProxyDecryptor::Decrypt(): decryptor not initialized.";
      pending_decrypt_closures_.push_back(
          base::Bind(&ProxyDecryptor::DecryptOnMessageLoop,
                     base::Unretained(this),
                     base::MessageLoopProxy::current(), encrypted,
                     decrypt_cb));
      // TODO(xhwang): The same NeedKey may be fired here and multiple times in
      // OnBufferDecrypted(). While the spec says only one NeedKey should be
      // fired. Leave them as is since the spec about this may change.
      FireNeedKey(client_, encrypted);
      return;
    }
  }

  decryptor->Decrypt(encrypted, base::Bind(
      &ProxyDecryptor::OnBufferDecrypted, base::Unretained(this),
      base::MessageLoopProxy::current(), encrypted, decrypt_cb));
}

void ProxyDecryptor::Stop() {
  DVLOG(1) << "Stop()";

  std::vector<base::Closure> closures_to_run;
  {
    base::AutoLock auto_lock(lock_);
    if (decryptor_.get())
      decryptor_->Stop();
    stopped_ = true;
    std::swap(pending_decrypt_closures_, closures_to_run);
  }

  for (std::vector<base::Closure>::iterator iter = closures_to_run.begin();
       iter != closures_to_run.end();
       ++iter) {
    iter->Run();
  }
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

void ProxyDecryptor::DecryptOnMessageLoop(
    const scoped_refptr<base::MessageLoopProxy>& message_loop_proxy,
    const scoped_refptr<media::DecoderBuffer>& encrypted,
    const media::Decryptor::DecryptCB& decrypt_cb) {
  DCHECK(decryptor_.get());

  if (!message_loop_proxy->BelongsToCurrentThread()) {
    message_loop_proxy->PostTask(FROM_HERE, base::Bind(
        &ProxyDecryptor::DecryptOnMessageLoop, base::Unretained(this),
        message_loop_proxy, encrypted, decrypt_cb));
    return;
  }

  {
    base::AutoLock auto_lock(lock_);
    if (stopped_) {
      DVLOG(1) << "DecryptOnMessageLoop(): fire decrypt callbacks with kError.";
      decrypt_cb.Run(kError, NULL);
      return;
    }
  }

  decryptor_->Decrypt(encrypted, base::Bind(
      &ProxyDecryptor::OnBufferDecrypted, base::Unretained(this),
      message_loop_proxy, encrypted, decrypt_cb));
}

void ProxyDecryptor::OnBufferDecrypted(
    const scoped_refptr<base::MessageLoopProxy>& message_loop_proxy,
    const scoped_refptr<media::DecoderBuffer>& encrypted,
    const media::Decryptor::DecryptCB& decrypt_cb,
    media::Decryptor::Status status,
    const scoped_refptr<media::DecoderBuffer>& decrypted) {
  if (status == media::Decryptor::kSuccess ||
      status == media::Decryptor::kError) {
    decrypt_cb.Run(status, decrypted);
    return;
  }

  DCHECK_EQ(status, media::Decryptor::kNoKey);
  DVLOG(1) << "OnBufferDecrypted(): kNoKey fired";
  {
    base::AutoLock auto_lock(lock_);
    if (stopped_) {
      DVLOG(1) << "OnBufferDecrypted(): fire decrypt callbacks with kError.";
      decrypt_cb.Run(kError, NULL);
      return;
    }
    pending_decrypt_closures_.push_back(base::Bind(
        &ProxyDecryptor::DecryptOnMessageLoop, base::Unretained(this),
        message_loop_proxy, encrypted, decrypt_cb));
  }
  // TODO(xhwang): The same NeedKey may be fired multiple times here and also
  // in Decrypt(). While the spec says only one NeedKey should be fired. Leave
  // them as is since the spec about this may change.
  FireNeedKey(client_, encrypted);
}

}  // namespace webkit_media
