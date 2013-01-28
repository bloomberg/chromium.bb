// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_PLUGINS_PPAPI_CONTENT_DECRYPTOR_DELEGATE_H_
#define WEBKIT_PLUGINS_PPAPI_CONTENT_DECRYPTOR_DELEGATE_H_

#include <queue>
#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "media/base/decryptor.h"
#include "ppapi/c/private/pp_content_decryptor.h"
#include "ppapi/c/private/ppp_content_decryptor_private.h"
#include "ui/gfx/size.h"
#include "webkit/plugins/webkit_plugins_export.h"

namespace media {
class AudioDecoderConfig;
class DecoderBuffer;
class VideoDecoderConfig;
}

namespace webkit {
namespace ppapi {

class PPB_Buffer_Impl;

class WEBKIT_PLUGINS_EXPORT ContentDecryptorDelegate {
 public:
  // ContentDecryptorDelegate does not take ownership of
  // |plugin_decryption_interface|. Therefore |plugin_decryption_interface|
  // must outlive this object.
  ContentDecryptorDelegate(
      PP_Instance pp_instance,
      const PPP_ContentDecryptor_Private* plugin_decryption_interface);

  void SetKeyEventCallbacks(const media::KeyAddedCB& key_added_cb,
                            const media::KeyErrorCB& key_error_cb,
                            const media::KeyMessageCB& key_message_cb,
                            const media::NeedKeyCB& need_key_cb);

  // Provides access to PPP_ContentDecryptor_Private.
  bool GenerateKeyRequest(const std::string& key_system,
                          const std::string& type,
                          const uint8* init_data,
                          int init_data_length);
  bool AddKey(const std::string& session_id,
              const uint8* key,
              int key_length,
              const uint8* init_data,
              int init_data_length);
  bool CancelKeyRequest(const std::string& session_id);
  bool Decrypt(media::Decryptor::StreamType stream_type,
               const scoped_refptr<media::DecoderBuffer>& encrypted_buffer,
               const media::Decryptor::DecryptCB& decrypt_cb);
  bool CancelDecrypt(media::Decryptor::StreamType stream_type);
  bool InitializeAudioDecoder(
      const media::AudioDecoderConfig& decoder_config,
      const media::Decryptor::DecoderInitCB& decoder_init_cb);
  bool InitializeVideoDecoder(
      const media::VideoDecoderConfig& decoder_config,
      const media::Decryptor::DecoderInitCB& decoder_init_cb);
  // TODO(tomfinegan): Add callback args for DeinitializeDecoder() and
  // ResetDecoder()
  bool DeinitializeDecoder(media::Decryptor::StreamType stream_type);
  bool ResetDecoder(media::Decryptor::StreamType stream_type);
  // Note: These methods can be used with unencrypted data.
  bool DecryptAndDecodeAudio(
      const scoped_refptr<media::DecoderBuffer>& encrypted_buffer,
      const media::Decryptor::AudioDecodeCB& audio_decode_cb);
  bool DecryptAndDecodeVideo(
      const scoped_refptr<media::DecoderBuffer>& encrypted_buffer,
      const media::Decryptor::VideoDecodeCB& video_decode_cb);

  // PPB_ContentDecryptor_Private dispatching methods.
  void NeedKey(PP_Var key_system, PP_Var session_id, PP_Var init_data);
  void KeyAdded(PP_Var key_system, PP_Var session_id);
  void KeyMessage(PP_Var key_system,
                  PP_Var session_id,
                  PP_Var message,
                  PP_Var default_url);
  void KeyError(PP_Var key_system,
                PP_Var session_id,
                int32_t media_error,
                int32_t system_code);
  void DeliverBlock(PP_Resource decrypted_block,
                    const PP_DecryptedBlockInfo* block_info);
  void DecoderInitializeDone(PP_DecryptorStreamType decoder_type,
                             uint32_t request_id,
                             PP_Bool success);
  void DecoderDeinitializeDone(PP_DecryptorStreamType decoder_type,
                               uint32_t request_id);
  void DecoderResetDone(PP_DecryptorStreamType decoder_type,
                        uint32_t request_id);
  void DeliverFrame(PP_Resource decrypted_frame,
                    const PP_DecryptedFrameInfo* frame_info);
  void DeliverSamples(PP_Resource audio_frames,
                      const PP_DecryptedBlockInfo* block_info);

 private:
  // Cancels the pending decrypt-and-decode callback for |stream_type|.
  void CancelDecode(media::Decryptor::StreamType stream_type);

  // Fills |resource| with a PPB_Buffer_Impl and copies the data from
  // |encrypted_buffer| into the buffer resource. This method reuses
  // |audio_input_resource_| and |video_input_resource_| to reduce the latency
  // in requesting new PPB_Buffer_Impl resources. The caller must make sure that
  // |audio_input_resource_| or |video_input_resource_| is available before
  // calling this method.
  //
  // An end of stream |encrypted_buffer| is represented as a null |resource|.
  //
  // Returns true upon success and false if any error happened.
  bool MakeMediaBufferResource(
      media::Decryptor::StreamType stream_type,
      const scoped_refptr<media::DecoderBuffer>& encrypted_buffer,
      scoped_refptr<PPB_Buffer_Impl>* resource);

  void FreeBuffer(uint32_t buffer_id);

  void SetBufferToFreeInTrackingInfo(PP_DecryptTrackingInfo* tracking_info);

  const PP_Instance pp_instance_;
  const PPP_ContentDecryptor_Private* const plugin_decryption_interface_;

  // Callbacks for firing key events.
  media::KeyAddedCB key_added_cb_;
  media::KeyErrorCB key_error_cb_;
  media::KeyMessageCB key_message_cb_;
  media::NeedKeyCB need_key_cb_;

  gfx::Size natural_size_;

  // Request ID for tracking pending content decryption callbacks.
  // Note that zero indicates an invalid request ID.
  // TODO(xhwang): Add completion callbacks for Reset/Stop and remove the use
  // of request IDs.
  uint32_t next_decryption_request_id_;

  uint32_t pending_audio_decrypt_request_id_;
  media::Decryptor::DecryptCB pending_audio_decrypt_cb_;

  uint32_t pending_video_decrypt_request_id_;
  media::Decryptor::DecryptCB pending_video_decrypt_cb_;

  uint32_t pending_audio_decoder_init_request_id_;
  media::Decryptor::DecoderInitCB pending_audio_decoder_init_cb_;

  uint32_t pending_video_decoder_init_request_id_;
  media::Decryptor::DecoderInitCB pending_video_decoder_init_cb_;

  uint32_t pending_audio_decode_request_id_;
  media::Decryptor::AudioDecodeCB pending_audio_decode_cb_;

  uint32_t pending_video_decode_request_id_;
  media::Decryptor::VideoDecodeCB pending_video_decode_cb_;

  // Cached audio and video input buffers. See MakeMediaBufferResource.
  scoped_refptr<PPB_Buffer_Impl> audio_input_resource_;
  scoped_refptr<PPB_Buffer_Impl> video_input_resource_;

  std::queue<uint32_t> free_buffers_;

  base::WeakPtrFactory<ContentDecryptorDelegate> weak_ptr_factory_;
  base::WeakPtr<ContentDecryptorDelegate> weak_this_;

  DISALLOW_COPY_AND_ASSIGN(ContentDecryptorDelegate);
};

}  // namespace ppapi
}  // namespace webkit

#endif  // WEBKIT_PLUGINS_PPAPI_CONTENT_DECRYPTOR_DELEGATE_H_
