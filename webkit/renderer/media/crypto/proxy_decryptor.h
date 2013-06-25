// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_RENDERER_MEDIA_CRYPTO_PROXY_DECRYPTOR_H_
#define WEBKIT_RENDERER_MEDIA_CRYPTO_PROXY_DECRYPTOR_H_

#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/synchronization/lock.h"
#include "media/base/decryptor.h"
#include "media/base/media_keys.h"

namespace WebKit {
class WebFrame;
class WebMediaPlayerClient;
}

namespace webkit_media {

// TODO(ddorwin): Move to its own file.
class ContentDecryptionModuleFactory {
 public:
  static scoped_ptr<media::MediaKeys> Create(
      const std::string& key_system,
#if defined(ENABLE_PEPPER_CDMS)
      // TODO(ddorwin): We need different pointers for the WD API.
      WebKit::WebMediaPlayerClient* web_media_player_client,
      WebKit::WebFrame* web_frame,
      const base::Closure& destroy_plugin_cb,
#elif defined(OS_ANDROID)
      // TODO(scherkus): Revert the ProxyDecryptor changes from r208040 so that
      // this class always creates the MediaKeys.
      // A ProxyMediaKeys to be used if |key_system| is not Clear Key.
      scoped_ptr<media::MediaKeys> media_keys,
#endif  // defined(ENABLE_PEPPER_CDMS)
      const media::KeyAddedCB& key_added_cb,
      const media::KeyErrorCB& key_error_cb,
      const media::KeyMessageCB& key_message_cb);

 private:
#if defined(ENABLE_PEPPER_CDMS)
  static scoped_ptr<media::MediaKeys> CreatePpapiDecryptor(
      const std::string& key_system,
      const media::KeyAddedCB& key_added_cb,
      const media::KeyErrorCB& key_error_cb,
      const media::KeyMessageCB& key_message_cb,
      const base::Closure& destroy_plugin_cb,
      // TODO(ddorwin): We need different pointers for the WD API.
      WebKit::WebMediaPlayerClient* web_media_player_client,
      WebKit::WebFrame* web_frame);
#endif  // defined(ENABLE_PEPPER_CDMS)
};

// ProxyDecryptor is for EME v0.1b only. It should not be used for the WD API.
// A decryptor proxy that creates a real decryptor object on demand and
// forwards decryptor calls to it.
// TODO(xhwang): Currently we don't support run-time switching among decryptor
// objects. Fix this when needed.
// TODO(xhwang): The ProxyDecryptor is not a Decryptor. Find a better name!
class ProxyDecryptor : public media::MediaKeys {
 public:
  ProxyDecryptor(
#if defined(ENABLE_PEPPER_CDMS)
      WebKit::WebMediaPlayerClient* web_media_player_client,
      WebKit::WebFrame* web_frame,
#elif defined(OS_ANDROID)
    scoped_ptr<media::MediaKeys> media_keys,
#endif  // defined(ENABLE_PEPPER_CDMS)
      const media::KeyAddedCB& key_added_cb,
      const media::KeyErrorCB& key_error_cb,
      const media::KeyMessageCB& key_message_cb);
  virtual ~ProxyDecryptor();

  // Only call this once.
  bool InitializeCDM(const std::string& key_system);

  // Requests the ProxyDecryptor to notify the decryptor when it's ready through
  // the |decryptor_ready_cb| provided.
  // If |decryptor_ready_cb| is null, the existing callback will be fired with
  // NULL immediately and reset.
  void SetDecryptorReadyCB(const media::DecryptorReadyCB& decryptor_ready_cb);

  // MediaKeys implementation.
  // May only be called after InitializeCDM() succeeds.
  virtual bool GenerateKeyRequest(const std::string& type,
                                  const uint8* init_data,
                                  int init_data_length) OVERRIDE;
  virtual void AddKey(const uint8* key, int key_length,
                      const uint8* init_data, int init_data_length,
                      const std::string& session_id) OVERRIDE;
  virtual void CancelKeyRequest(const std::string& session_id) OVERRIDE;

 private:
  // Helper function to create MediaKeys to handle the given |key_system|.
  scoped_ptr<media::MediaKeys> CreateMediaKeys(const std::string& key_system);

  // Callbacks for firing key events.
  void KeyAdded(const std::string& session_id);
  void KeyError(const std::string& session_id,
                media::MediaKeys::KeyError error_code,
                int system_code);
  void KeyMessage(const std::string& session_id,
                  const std::string& message,
                  const std::string& default_url);

  base::WeakPtrFactory<ProxyDecryptor> weak_ptr_factory_;

#if defined(ENABLE_PEPPER_CDMS)
  // Callback for cleaning up a Pepper-based CDM.
  void DestroyHelperPlugin();

  // Needed to create the PpapiDecryptor.
  WebKit::WebMediaPlayerClient* web_media_player_client_;
  WebKit::WebFrame* web_frame_;
#endif  // defined(ENABLE_PEPPER_CDMS)

  // The real MediaKeys that manages key operations for the ProxyDecryptor.
  // This pointer is protected by the |lock_|.
  scoped_ptr<media::MediaKeys> media_keys_;

  // Callbacks for firing key events.
  media::KeyAddedCB key_added_cb_;
  media::KeyErrorCB key_error_cb_;
  media::KeyMessageCB key_message_cb_;

  // Protects the |decryptor_|. Note that |decryptor_| itself should be thread
  // safe as per the Decryptor interface.
  base::Lock lock_;

  media::DecryptorReadyCB decryptor_ready_cb_;

  DISALLOW_COPY_AND_ASSIGN(ProxyDecryptor);
};

}  // namespace webkit_media

#endif  // WEBKIT_RENDERER_MEDIA_CRYPTO_PROXY_DECRYPTOR_H_
