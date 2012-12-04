// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/plugins/ppapi/content_decryptor_delegate.h"

#include "base/callback_helpers.h"
#include "base/debug/trace_event.h"
#include "base/message_loop_proxy.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/bind_to_loop.h"
#include "media/base/channel_layout.h"
#include "media/base/data_buffer.h"
#include "media/base/decoder_buffer.h"
#include "media/base/decryptor_client.h"
#include "media/base/video_decoder_config.h"
#include "media/base/video_frame.h"
#include "media/base/video_util.h"
#include "ppapi/shared_impl/var.h"
#include "ppapi/shared_impl/var_tracker.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppb_buffer_api.h"
#include "ui/gfx/rect.h"
#include "webkit/plugins/ppapi/ppb_buffer_impl.h"

using ppapi::PpapiGlobals;
using ppapi::ScopedPPResource;
using ppapi::StringVar;
using ppapi::thunk::EnterResourceNoLock;
using ppapi::thunk::PPB_Buffer_API;

namespace webkit {
namespace ppapi {

namespace {

// Fills |resource| with a PP_Resource containing a PPB_Buffer_Impl and copies
// |data| into the buffer resource. The |resource| has a reference count of 1.
// If |data| is NULL, fills |resource| with a PP_Resource with ID of 0.
// Returns true upon success and false if any error happened.
bool MakeBufferResource(PP_Instance instance,
                        const uint8* data, int size,
                        ScopedPPResource* resource) {
  TRACE_EVENT0("eme", "ContentDecryptorDelegate - MakeBufferResource");
  DCHECK(resource);

  if (!data || !size) {
    DCHECK(!data && !size);
    resource->Release();
    return true;
  }

  ScopedPPResource scoped_resource(ScopedPPResource::PassRef(),
                                   PPB_Buffer_Impl::Create(instance, size));
  if (!scoped_resource.get())
    return false;

  EnterResourceNoLock<PPB_Buffer_API> enter(scoped_resource, true);
  if (enter.failed())
    return false;

  BufferAutoMapper mapper(enter.object());
  if (!mapper.data() || mapper.size() < static_cast<size_t>(size))
    return false;
  memcpy(mapper.data(), data, size);

  *resource = scoped_resource;
  return true;
}

// Copies the content of |str| into |array|.
// Returns true if copy succeeded. Returns false if copy failed, e.g. if the
// |array_size| is smaller than the |str| length.
template <uint32_t array_size>
bool CopyStringToArray(const std::string& str, uint8 (&array)[array_size]) {
  if (array_size < str.size())
    return false;

  memcpy(array, str.data(), str.size());
  return true;
}

// Fills the |block_info| with information from |decrypt_config|, |timestamp|
// and |request_id|. |decrypt_config| can be NULL if the block is not encrypted.
// This is useful for end-of-stream blocks.
// Returns true if |block_info| is successfully filled. Returns false
// otherwise.
bool MakeEncryptedBlockInfo(int data_size,
                            const media::DecryptConfig* decrypt_config,
                            int64_t timestamp,
                            uint32_t request_id,
                            PP_EncryptedBlockInfo* block_info) {
  DCHECK(block_info);

  // TODO(xhwang): Fix initialization of PP_EncryptedBlockInfo here and
  // anywhere else.
  memset(block_info, 0, sizeof(*block_info));

  block_info->tracking_info.request_id = request_id;
  block_info->tracking_info.timestamp = timestamp;

  if (!decrypt_config)
    return true;

  DCHECK(data_size) << "DecryptConfig is set on an empty buffer";
  block_info->data_size = data_size;
  block_info->data_offset = decrypt_config->data_offset();

  if (!CopyStringToArray(decrypt_config->key_id(), block_info->key_id) ||
      !CopyStringToArray(decrypt_config->iv(), block_info->iv))
    return false;

  block_info->key_id_size = decrypt_config->key_id().size();
  block_info->iv_size = decrypt_config->iv().size();

  if (decrypt_config->subsamples().size() > arraysize(block_info->subsamples))
    return false;

  block_info->num_subsamples = decrypt_config->subsamples().size();
  for (uint32_t i = 0; i < block_info->num_subsamples; ++i) {
    block_info->subsamples[i].clear_bytes =
        decrypt_config->subsamples()[i].clear_bytes;
    block_info->subsamples[i].cipher_bytes =
        decrypt_config->subsamples()[i].cypher_bytes;
  }

  return true;
}

// Deserializes audio data stored in |audio_frames| into individual audio
// buffers in |frames|. Returns true upon success.
bool DeserializeAudioFrames(PP_Resource audio_frames,
                            media::Decryptor::AudioBuffers* frames) {
  DCHECK(frames);
  EnterResourceNoLock<PPB_Buffer_API> enter(audio_frames, true);
  if (!enter.succeeded())
    return false;

  BufferAutoMapper mapper(enter.object());
  if (!mapper.data() || !mapper.size())
    return false;

  const uint8* cur = static_cast<uint8*>(mapper.data());
  int bytes_left = mapper.size();

  do {
    int64 timestamp = 0;
    int64 frame_size = -1;
    const int kHeaderSize = sizeof(timestamp) + sizeof(frame_size);

    if (bytes_left < kHeaderSize)
      return false;

    memcpy(&timestamp, cur, sizeof(timestamp));
    cur += sizeof(timestamp);
    bytes_left -= sizeof(timestamp);

    memcpy(&frame_size, cur, sizeof(frame_size));
    cur += sizeof(frame_size);
    bytes_left -= sizeof(frame_size);

    // We should *not* have empty frame in the list.
    if (frame_size <= 0 || bytes_left < frame_size)
      return false;

    scoped_refptr<media::DataBuffer> frame(new media::DataBuffer(cur,
                                                                 frame_size));
    frame->SetTimestamp(base::TimeDelta::FromMicroseconds(timestamp));
    frames->push_back(frame);

    cur += frame_size;
    bytes_left -= frame_size;
  } while (bytes_left > 0);

  return true;
}

PP_AudioCodec MediaAudioCodecToPpAudioCodec(media::AudioCodec codec) {
  switch (codec) {
    case media::kCodecVorbis:
      return PP_AUDIOCODEC_VORBIS;
    case media::kCodecAAC:
      return PP_AUDIOCODEC_AAC;
    default:
      return PP_AUDIOCODEC_UNKNOWN;
  }
}

PP_VideoCodec MediaVideoCodecToPpVideoCodec(media::VideoCodec codec) {
  switch (codec) {
    case media::kCodecVP8:
      return PP_VIDEOCODEC_VP8;
    case media::kCodecH264:
      return PP_VIDEOCODEC_H264;
    default:
      return PP_VIDEOCODEC_UNKNOWN;
  }
}

PP_VideoCodecProfile MediaVideoCodecProfileToPpVideoCodecProfile(
    media::VideoCodecProfile profile) {
  switch (profile) {
    case media::VP8PROFILE_MAIN:
      return PP_VIDEOCODECPROFILE_VP8_MAIN;
    case media::H264PROFILE_BASELINE:
      return PP_VIDEOCODECPROFILE_H264_BASELINE;
    case media::H264PROFILE_MAIN:
      return PP_VIDEOCODECPROFILE_H264_MAIN;
    case media::H264PROFILE_EXTENDED:
      return PP_VIDEOCODECPROFILE_H264_EXTENDED;
    case media::H264PROFILE_HIGH:
      return PP_VIDEOCODECPROFILE_H264_HIGH;
    case media::H264PROFILE_HIGH10PROFILE:
      return PP_VIDEOCODECPROFILE_H264_HIGH_10;
    case media::H264PROFILE_HIGH422PROFILE:
      return PP_VIDEOCODECPROFILE_H264_HIGH_422;
    case media::H264PROFILE_HIGH444PREDICTIVEPROFILE:
      return PP_VIDEOCODECPROFILE_H264_HIGH_444_PREDICTIVE;
    default:
      return PP_VIDEOCODECPROFILE_UNKNOWN;
  }
}

PP_DecryptedFrameFormat MediaVideoFormatToPpDecryptedFrameFormat(
    media::VideoFrame::Format format) {
  switch (format) {
    case media::VideoFrame::YV12:
      return PP_DECRYPTEDFRAMEFORMAT_YV12;
    case media::VideoFrame::I420:
      return PP_DECRYPTEDFRAMEFORMAT_I420;
    default:
      return PP_DECRYPTEDFRAMEFORMAT_UNKNOWN;
  }
}

media::Decryptor::Status PpDecryptResultToMediaDecryptorStatus(
    PP_DecryptResult result) {
  switch (result) {
    case PP_DECRYPTRESULT_SUCCESS:
      return media::Decryptor::kSuccess;
    case PP_DECRYPTRESULT_DECRYPT_NOKEY:
      return media::Decryptor::kNoKey;
    case PP_DECRYPTRESULT_NEEDMOREDATA:
      return media::Decryptor::kNeedMoreData;
    case PP_DECRYPTRESULT_DECRYPT_ERROR:
      return media::Decryptor::kError;
    case PP_DECRYPTRESULT_DECODE_ERROR:
      return media::Decryptor::kError;
    default:
      NOTREACHED();
      return media::Decryptor::kError;
  }
}

PP_DecryptorStreamType MediaDecryptorStreamTypeToPpStreamType(
    media::Decryptor::StreamType stream_type) {
  switch (stream_type) {
    case media::Decryptor::kAudio:
      return PP_DECRYPTORSTREAMTYPE_AUDIO;
    case media::Decryptor::kVideo:
      return PP_DECRYPTORSTREAMTYPE_VIDEO;
    default:
      NOTREACHED();
      return PP_DECRYPTORSTREAMTYPE_VIDEO;
  }
}

}  // namespace

ContentDecryptorDelegate::ContentDecryptorDelegate(
    PP_Instance pp_instance,
    const PPP_ContentDecryptor_Private* plugin_decryption_interface)
    : pp_instance_(pp_instance),
      plugin_decryption_interface_(plugin_decryption_interface),
      decryptor_client_(NULL),
      next_decryption_request_id_(1),
      pending_audio_decrypt_request_id_(0),
      pending_video_decrypt_request_id_(0),
      pending_audio_decoder_init_request_id_(0),
      pending_video_decoder_init_request_id_(0),
      pending_audio_decode_request_id_(0),
      pending_video_decode_request_id_(0),
      audio_input_resource_size_(0),
      video_input_resource_size_(0) {
}

void ContentDecryptorDelegate::set_decrypt_client(
    media::DecryptorClient* decryptor_client) {
  decryptor_client_ = decryptor_client;
}

bool ContentDecryptorDelegate::GenerateKeyRequest(const std::string& key_system,
                                                  const std::string& type,
                                                  const uint8* init_data,
                                                  int init_data_length) {
  if (key_system.empty())
    return false;

  PP_Var init_data_array =
      PpapiGlobals::Get()->GetVarTracker()->MakeArrayBufferPPVar(
          init_data_length, init_data);

  plugin_decryption_interface_->GenerateKeyRequest(
      pp_instance_,
      StringVar::StringToPPVar(key_system),
      StringVar::StringToPPVar(type),
      init_data_array);
  return true;
}

bool ContentDecryptorDelegate::AddKey(const std::string& session_id,
                                      const uint8* key,
                                      int key_length,
                                      const uint8* init_data,
                                      int init_data_length) {
  PP_Var key_array =
      PpapiGlobals::Get()->GetVarTracker()->MakeArrayBufferPPVar(key_length,
                                                                 key);
  PP_Var init_data_array =
      PpapiGlobals::Get()->GetVarTracker()->MakeArrayBufferPPVar(
          init_data_length, init_data);

  plugin_decryption_interface_->AddKey(
      pp_instance_,
      StringVar::StringToPPVar(session_id),
      key_array,
      init_data_array);
  return true;
}

bool ContentDecryptorDelegate::CancelKeyRequest(const std::string& session_id) {
  plugin_decryption_interface_->CancelKeyRequest(
      pp_instance_,
      StringVar::StringToPPVar(session_id));
  return true;
}

// TODO(xhwang): Remove duplication of code in Decrypt(),
// DecryptAndDecodeAudio() and DecryptAndDecodeVideo().
bool ContentDecryptorDelegate::Decrypt(
    media::Decryptor::StreamType stream_type,
    const scoped_refptr<media::DecoderBuffer>& encrypted_buffer,
    const media::Decryptor::DecryptCB& decrypt_cb) {
  DVLOG(3) << "Decrypt() - stream_type: " << stream_type;
  // |{audio|video}_input_resource_[size_]| is not being used by the plugin
  // now because there is only one pending audio/video decrypt request at any
  // time. This is enforced by the media pipeline.
  ScopedPPResource encrypted_resource;
  if (!MakeMediaBufferResource(stream_type,
                               encrypted_buffer->GetData(),
                               encrypted_buffer->GetDataSize(),
                               &encrypted_resource) ||
      !encrypted_resource.get()) {
    return false;
  }

  const uint32_t request_id = next_decryption_request_id_++;
  DVLOG(2) << "Decrypt() - request_id " << request_id;

  PP_EncryptedBlockInfo block_info;
  DCHECK(encrypted_buffer->GetDecryptConfig());
  if (!MakeEncryptedBlockInfo(encrypted_buffer->GetDataSize(),
                              encrypted_buffer->GetDecryptConfig(),
                              encrypted_buffer->GetTimestamp().InMicroseconds(),
                              request_id,
                              &block_info)) {
    return false;
  }

  // There is only one pending decrypt request at any time per stream. This is
  // enforced by the media pipeline.
  switch (stream_type) {
    case media::Decryptor::kAudio:
      DCHECK_EQ(pending_audio_decrypt_request_id_, 0u);
      DCHECK(pending_audio_decrypt_cb_.is_null());
      pending_audio_decrypt_request_id_ = request_id;
      pending_audio_decrypt_cb_ = decrypt_cb;
      break;
    case media::Decryptor::kVideo:
      DCHECK_EQ(pending_video_decrypt_request_id_, 0u);
      DCHECK(pending_video_decrypt_cb_.is_null());
      pending_video_decrypt_request_id_ = request_id;
      pending_video_decrypt_cb_ = decrypt_cb;
      break;
    default:
      NOTREACHED();
      return false;
  }

  plugin_decryption_interface_->Decrypt(pp_instance_,
                                        encrypted_resource,
                                        &block_info);
  return true;
}

bool ContentDecryptorDelegate::CancelDecrypt(
    media::Decryptor::StreamType stream_type) {
  DVLOG(3) << "CancelDecrypt() - stream_type: " << stream_type;

  media::Decryptor::DecryptCB decrypt_cb;
  switch (stream_type) {
    case media::Decryptor::kAudio:
      pending_audio_decrypt_request_id_ = 0;
      decrypt_cb = base::ResetAndReturn(&pending_audio_decrypt_cb_);
      break;
    case media::Decryptor::kVideo:
      pending_video_decrypt_request_id_ = 0;
      decrypt_cb = base::ResetAndReturn(&pending_video_decrypt_cb_);
      break;
    default:
      NOTREACHED();
      return false;
  }

  if (!decrypt_cb.is_null())
    decrypt_cb.Run(media::Decryptor::kSuccess, NULL);

  return true;
}

bool ContentDecryptorDelegate::InitializeAudioDecoder(
    const media::AudioDecoderConfig& decoder_config,
    const media::Decryptor::DecoderInitCB& init_cb) {
  PP_AudioDecoderConfig pp_decoder_config;
  pp_decoder_config.codec =
      MediaAudioCodecToPpAudioCodec(decoder_config.codec());
  pp_decoder_config.channel_count =
      media::ChannelLayoutToChannelCount(decoder_config.channel_layout());
  pp_decoder_config.bits_per_channel = decoder_config.bits_per_channel();
  pp_decoder_config.samples_per_second = decoder_config.samples_per_second();
  pp_decoder_config.request_id = next_decryption_request_id_++;

  ScopedPPResource extra_data_resource;
  if (!MakeBufferResource(pp_instance_,
                          decoder_config.extra_data(),
                          decoder_config.extra_data_size(),
                          &extra_data_resource)) {
    return false;
  }

  DCHECK_EQ(pending_audio_decoder_init_request_id_, 0u);
  DCHECK(pending_audio_decoder_init_cb_.is_null());
  pending_audio_decoder_init_request_id_ = pp_decoder_config.request_id;
  pending_audio_decoder_init_cb_ = init_cb;

  plugin_decryption_interface_->InitializeAudioDecoder(pp_instance_,
                                                       &pp_decoder_config,
                                                       extra_data_resource);
  return true;
}

bool ContentDecryptorDelegate::InitializeVideoDecoder(
    const media::VideoDecoderConfig& decoder_config,
    const media::Decryptor::DecoderInitCB& init_cb) {
  PP_VideoDecoderConfig pp_decoder_config;
  pp_decoder_config.codec =
      MediaVideoCodecToPpVideoCodec(decoder_config.codec());
  pp_decoder_config.profile =
      MediaVideoCodecProfileToPpVideoCodecProfile(decoder_config.profile());
  pp_decoder_config.format =
      MediaVideoFormatToPpDecryptedFrameFormat(decoder_config.format());
  pp_decoder_config.width = decoder_config.coded_size().width();
  pp_decoder_config.height = decoder_config.coded_size().height();
  pp_decoder_config.request_id = next_decryption_request_id_++;

  ScopedPPResource extra_data_resource;
  if (!MakeBufferResource(pp_instance_,
                          decoder_config.extra_data(),
                          decoder_config.extra_data_size(),
                          &extra_data_resource)) {
    return false;
  }

  DCHECK_EQ(pending_video_decoder_init_request_id_, 0u);
  DCHECK(pending_video_decoder_init_cb_.is_null());
  pending_video_decoder_init_request_id_ = pp_decoder_config.request_id;
  pending_video_decoder_init_cb_ = init_cb;

  natural_size_ = decoder_config.natural_size();

  plugin_decryption_interface_->InitializeVideoDecoder(pp_instance_,
                                                       &pp_decoder_config,
                                                       extra_data_resource);

  return true;
}

bool ContentDecryptorDelegate::DeinitializeDecoder(
    media::Decryptor::StreamType stream_type) {
  CancelDecode(stream_type);

  natural_size_ = gfx::Size();

  // TODO(tomfinegan): Add decoder deinitialize request tracking, and get
  // stream type from media stack.
  plugin_decryption_interface_->DeinitializeDecoder(
      pp_instance_, MediaDecryptorStreamTypeToPpStreamType(stream_type), 0);
  return true;
}

bool ContentDecryptorDelegate::ResetDecoder(
    media::Decryptor::StreamType stream_type) {
  CancelDecode(stream_type);

  // TODO(tomfinegan): Add decoder reset request tracking.
  plugin_decryption_interface_->ResetDecoder(
      pp_instance_, MediaDecryptorStreamTypeToPpStreamType(stream_type), 0);
  return true;
}

bool ContentDecryptorDelegate::DecryptAndDecodeAudio(
    const scoped_refptr<media::DecoderBuffer>& encrypted_buffer,
    const media::Decryptor::AudioDecodeCB& audio_decode_cb) {
  // If |encrypted_buffer| is end-of-stream buffer, GetData() and GetDataSize()
  // return NULL and 0 respectively. In that case, we'll just create a 0
  // resource.
  // |audio_input_resource_size_| is not being used by the plugin now
  // because there is only one pending audio decode request at any time.
  // This is enforced by the media pipeline.
  ScopedPPResource encrypted_resource;
  if (!MakeMediaBufferResource(media::Decryptor::kAudio,
                               encrypted_buffer->GetData(),
                               encrypted_buffer->GetDataSize(),
                               &encrypted_resource)) {
    return false;
  }

  // The resource should not be 0 for non-EOS buffer.
  if (!encrypted_buffer->IsEndOfStream() && !encrypted_resource.get())
    return false;

  const uint32_t request_id = next_decryption_request_id_++;
  DVLOG(2) << "DecryptAndDecodeAudio() - request_id " << request_id;

  PP_EncryptedBlockInfo block_info;
  if (!MakeEncryptedBlockInfo(
      encrypted_buffer->GetDataSize(),
      encrypted_buffer->GetDecryptConfig(),
      encrypted_buffer->GetTimestamp().InMicroseconds(),
      request_id,
      &block_info)) {
    return false;
  }

  // There is only one pending audio decode request at any time. This is
  // enforced by the media pipeline.
  DCHECK_EQ(pending_audio_decode_request_id_, 0u);
  DCHECK(pending_audio_decode_cb_.is_null());
  pending_audio_decode_request_id_ = request_id;
  pending_audio_decode_cb_ = audio_decode_cb;

  plugin_decryption_interface_->DecryptAndDecode(pp_instance_,
                                                 PP_DECRYPTORSTREAMTYPE_AUDIO,
                                                 encrypted_resource,
                                                 &block_info);
  return true;
}

bool ContentDecryptorDelegate::DecryptAndDecodeVideo(
    const scoped_refptr<media::DecoderBuffer>& encrypted_buffer,
    const media::Decryptor::VideoDecodeCB& video_decode_cb) {
  // If |encrypted_buffer| is end-of-stream buffer, GetData() and GetDataSize()
  // return NULL and 0 respectively. In that case, we'll just create a 0
  // resource.
  // |video_input_resource_size_| is not being used by the plugin now
  // because there is only one pending video decode request at any time.
  // This is enforced by the media pipeline.
  ScopedPPResource encrypted_resource;
  if (!MakeMediaBufferResource(media::Decryptor::kVideo,
                               encrypted_buffer->GetData(),
                               encrypted_buffer->GetDataSize(),
                               &encrypted_resource)) {
    return false;
  }

  // The resource should not be 0 for non-EOS buffer.
  if (!encrypted_buffer->IsEndOfStream() && !encrypted_resource.get())
    return false;

  const uint32_t request_id = next_decryption_request_id_++;
  DVLOG(2) << "DecryptAndDecodeVideo() - request_id " << request_id;
  TRACE_EVENT_ASYNC_BEGIN0(
      "eme", "ContentDecryptorDelegate::DecryptAndDecodeVideo", request_id);

  PP_EncryptedBlockInfo block_info;
  if (!MakeEncryptedBlockInfo(
      encrypted_buffer->GetDataSize(),
      encrypted_buffer->GetDecryptConfig(),
      encrypted_buffer->GetTimestamp().InMicroseconds(),
      request_id,
      &block_info)) {
    return false;
  }

  // Only one pending video decode request at any time. This is enforced by the
  // media pipeline.
  DCHECK_EQ(pending_video_decode_request_id_, 0u);
  DCHECK(pending_video_decode_cb_.is_null());
  pending_video_decode_request_id_ = request_id;
  pending_video_decode_cb_ = video_decode_cb;

  // TODO(tomfinegan): Need to get stream type from media stack.
  plugin_decryption_interface_->DecryptAndDecode(pp_instance_,
                                                 PP_DECRYPTORSTREAMTYPE_VIDEO,
                                                 encrypted_resource,
                                                 &block_info);
  return true;
}

void ContentDecryptorDelegate::NeedKey(PP_Var key_system_var,
                                       PP_Var session_id_var,
                                       PP_Var init_data_var) {
  // TODO(tomfinegan): send the data to media stack.
}

void ContentDecryptorDelegate::KeyAdded(PP_Var key_system_var,
                                        PP_Var session_id_var) {
  if (!decryptor_client_)
    return;

  StringVar* key_system_string = StringVar::FromPPVar(key_system_var);
  StringVar* session_id_string = StringVar::FromPPVar(session_id_var);
  if (!key_system_string || !session_id_string) {
    decryptor_client_->KeyError("", "", media::Decryptor::kUnknownError, 0);
    return;
  }

  decryptor_client_->KeyAdded(key_system_string->value(),
                              session_id_string->value());
}

void ContentDecryptorDelegate::KeyMessage(PP_Var key_system_var,
                                          PP_Var session_id_var,
                                          PP_Resource message_resource,
                                          PP_Var default_url_var) {
  if (!decryptor_client_)
    return;

  StringVar* key_system_string = StringVar::FromPPVar(key_system_var);
  StringVar* session_id_string = StringVar::FromPPVar(session_id_var);
  StringVar* default_url_string = StringVar::FromPPVar(default_url_var);

  if (!key_system_string || !session_id_string || !default_url_string) {
    decryptor_client_->KeyError("", "", media::Decryptor::kUnknownError, 0);
    return;
  }

  EnterResourceNoLock<PPB_Buffer_API> enter(message_resource, true);
  if (!enter.succeeded()) {
    decryptor_client_->KeyError(key_system_string->value(),
                                session_id_string->value(),
                                media::Decryptor::kUnknownError,
                                0);
    return;
  }

  BufferAutoMapper mapper(enter.object());
  scoped_array<uint8> message_array(new uint8[mapper.size()]);
  if (mapper.data() && mapper.size())
    memcpy(message_array.get(), mapper.data(), mapper.size());

  decryptor_client_->KeyMessage(key_system_string->value(),
                                session_id_string->value(),
                                message_array.Pass(),
                                mapper.size(),
                                default_url_string->value());
}

void ContentDecryptorDelegate::KeyError(PP_Var key_system_var,
                                        PP_Var session_id_var,
                                        int32_t media_error,
                                        int32_t system_code) {
  if (!decryptor_client_)
    return;

  StringVar* key_system_string = StringVar::FromPPVar(key_system_var);
  StringVar* session_id_string = StringVar::FromPPVar(session_id_var);
  if (!key_system_string || !session_id_string) {
    decryptor_client_->KeyError("", "", media::Decryptor::kUnknownError, 0);
    return;
  }

  decryptor_client_->KeyError(
      key_system_string->value(),
      session_id_string->value(),
      static_cast<media::Decryptor::KeyError>(media_error),
      system_code);
}

void ContentDecryptorDelegate::DecoderInitializeDone(
     PP_DecryptorStreamType decoder_type,
     uint32_t request_id,
     PP_Bool success) {
  if (decoder_type == PP_DECRYPTORSTREAMTYPE_AUDIO) {
    // If the request ID is not valid or does not match what's saved, do
    // nothing.
    if (request_id == 0 ||
        request_id != pending_audio_decoder_init_request_id_)
      return;

      DCHECK(!pending_audio_decoder_init_cb_.is_null());
      pending_audio_decoder_init_request_id_ = 0;
      base::ResetAndReturn(
          &pending_audio_decoder_init_cb_).Run(PP_ToBool(success));
  } else {
    if (request_id == 0 ||
        request_id != pending_video_decoder_init_request_id_)
      return;

      if (!success)
        natural_size_ = gfx::Size();

      DCHECK(!pending_video_decoder_init_cb_.is_null());
      pending_video_decoder_init_request_id_ = 0;
      base::ResetAndReturn(
          &pending_video_decoder_init_cb_).Run(PP_ToBool(success));
  }
}

void ContentDecryptorDelegate::DecoderDeinitializeDone(
    PP_DecryptorStreamType decoder_type,
    uint32_t request_id) {
  // TODO(tomfinegan): Add decoder stop completion handling.
}

void ContentDecryptorDelegate::DecoderResetDone(
    PP_DecryptorStreamType decoder_type,
    uint32_t request_id) {
  // TODO(tomfinegan): Add decoder reset completion handling.
}

void ContentDecryptorDelegate::DeliverBlock(
    PP_Resource decrypted_block,
    const PP_DecryptedBlockInfo* block_info) {
  DCHECK(block_info);
  const uint32_t request_id = block_info->tracking_info.request_id;
  DVLOG(2) << "DeliverBlock() - request_id: " << request_id;

  // If the request ID is not valid or does not match what's saved, do nothing.
  if (request_id == 0) {
    DVLOG(1) << "DeliverBlock() - invalid request_id " << request_id;
    return;
  }

  media::Decryptor::DecryptCB decrypt_cb;
  if (request_id == pending_audio_decrypt_request_id_) {
    DCHECK(!pending_audio_decrypt_cb_.is_null());
    pending_audio_decrypt_request_id_ = 0;
    decrypt_cb = base::ResetAndReturn(&pending_audio_decrypt_cb_);
  } else if (request_id == pending_video_decrypt_request_id_) {
    DCHECK(!pending_video_decrypt_cb_.is_null());
    pending_video_decrypt_request_id_ = 0;
    decrypt_cb = base::ResetAndReturn(&pending_video_decrypt_cb_);
  } else {
    DVLOG(1) << "DeliverBlock() - request_id " << request_id << " not found";
    return;
  }

  media::Decryptor::Status status =
      PpDecryptResultToMediaDecryptorStatus(block_info->result);
  if (status != media::Decryptor::kSuccess) {
    decrypt_cb.Run(status, NULL);
    return;
  }

  EnterResourceNoLock<PPB_Buffer_API> enter(decrypted_block, true);
  if (!enter.succeeded()) {
    decrypt_cb.Run(media::Decryptor::kError, NULL);
    return;
  }
  BufferAutoMapper mapper(enter.object());
  if (!mapper.data() || !mapper.size()) {
    decrypt_cb.Run(media::Decryptor::kError, NULL);
    return;
  }

  // TODO(tomfinegan): Find a way to take ownership of the shared memory
  // managed by the PPB_Buffer_Dev, and avoid the extra copy.
  scoped_refptr<media::DecoderBuffer> decrypted_buffer(
      media::DecoderBuffer::CopyFrom(
          static_cast<uint8*>(mapper.data()), mapper.size()));
  decrypted_buffer->SetTimestamp(base::TimeDelta::FromMicroseconds(
      block_info->tracking_info.timestamp));
  decrypt_cb.Run(media::Decryptor::kSuccess, decrypted_buffer);
}

void ContentDecryptorDelegate::DeliverFrame(
    PP_Resource decrypted_frame,
    const PP_DecryptedFrameInfo* frame_info) {
  DCHECK(frame_info);
  const uint32_t request_id = frame_info->tracking_info.request_id;
  DVLOG(2) << "DeliverFrame() - request_id: " << request_id;

  // If the request ID is not valid or does not match what's saved, do nothing.
  if (request_id == 0 || request_id != pending_video_decode_request_id_) {
    DVLOG(1) << "DeliverFrame() - request_id " << request_id << " not found";
    return;
  }

  TRACE_EVENT_ASYNC_END0(
      "eme", "ContentDecryptorDelegate::DecryptAndDecodeVideo", request_id);

  DCHECK(!pending_video_decode_cb_.is_null());
  pending_video_decode_request_id_ = 0;
  media::Decryptor::VideoDecodeCB video_decode_cb =
      base::ResetAndReturn(&pending_video_decode_cb_);

  media::Decryptor::Status status =
      PpDecryptResultToMediaDecryptorStatus(frame_info->result);
  if (status != media::Decryptor::kSuccess) {
    video_decode_cb.Run(status, NULL);
    return;
  }

  EnterResourceNoLock<PPB_Buffer_API> enter(decrypted_frame, true);
  if (!enter.succeeded()) {
    video_decode_cb.Run(media::Decryptor::kError, NULL);
    return;
  }

  scoped_refptr<PPB_Buffer_Impl> ppb_buffer =
      static_cast<PPB_Buffer_Impl*>(enter.object());

  uint8* frame_data = static_cast<uint8*>(ppb_buffer->Map());
  if (!ppb_buffer->IsMapped() || !frame_data) {
    video_decode_cb.Run(media::Decryptor::kError, NULL);
    return;
  }

  uint32_t mapped_size = 0;
  if (!ppb_buffer->Describe(&mapped_size) || !mapped_size) {
    video_decode_cb.Run(media::Decryptor::kError, NULL);
    return;
  }

  gfx::Size frame_size(frame_info->width, frame_info->height);
  DCHECK_EQ(frame_info->format, PP_DECRYPTEDFRAMEFORMAT_YV12);

  scoped_refptr<media::VideoFrame> decoded_frame =
      media::VideoFrame::WrapExternalYuvData(
          media::VideoFrame::YV12,
          frame_size, gfx::Rect(frame_size), natural_size_,
          frame_info->strides[PP_DECRYPTEDFRAMEPLANES_Y],
          frame_info->strides[PP_DECRYPTEDFRAMEPLANES_U],
          frame_info->strides[PP_DECRYPTEDFRAMEPLANES_V],
          frame_data + frame_info->plane_offsets[PP_DECRYPTEDFRAMEPLANES_Y],
          frame_data + frame_info->plane_offsets[PP_DECRYPTEDFRAMEPLANES_U],
          frame_data + frame_info->plane_offsets[PP_DECRYPTEDFRAMEPLANES_V],
          base::TimeDelta::FromMicroseconds(
              frame_info->tracking_info.timestamp),
          media::BindToLoop(base::MessageLoopProxy::current(),
                            base::Bind(&PPB_Buffer_Impl::Unmap, ppb_buffer)));

  video_decode_cb.Run(media::Decryptor::kSuccess, decoded_frame);
}

void ContentDecryptorDelegate::DeliverSamples(
    PP_Resource audio_frames,
    const PP_DecryptedBlockInfo* block_info) {
  DCHECK(block_info);
  const uint32_t request_id = block_info->tracking_info.request_id;
  DVLOG(2) << "DeliverSamples() - request_id: " << request_id;

  // If the request ID is not valid or does not match what's saved, do nothing.
  if (request_id == 0 || request_id != pending_audio_decode_request_id_) {
    DVLOG(1) << "DeliverSamples() - request_id " << request_id << " not found";
    return;
  }

  DCHECK(!pending_audio_decode_cb_.is_null());
  pending_audio_decode_request_id_ = 0;
  media::Decryptor::AudioDecodeCB audio_decode_cb =
      base::ResetAndReturn(&pending_audio_decode_cb_);

  const media::Decryptor::AudioBuffers empty_frames;

  media::Decryptor::Status status =
      PpDecryptResultToMediaDecryptorStatus(block_info->result);
  if (status != media::Decryptor::kSuccess) {
    audio_decode_cb.Run(status, empty_frames);
    return;
  }

  media::Decryptor::AudioBuffers audio_frame_list;
  if (!DeserializeAudioFrames(audio_frames, &audio_frame_list)) {
    NOTREACHED() << "CDM did not serialize the buffer correctly.";
    audio_decode_cb.Run(media::Decryptor::kError, empty_frames);
    return;
  }

  audio_decode_cb.Run(media::Decryptor::kSuccess, audio_frame_list);
}

// TODO(xhwang): Try to remove duplicate logic here and in CancelDecrypt().
void ContentDecryptorDelegate::CancelDecode(
    media::Decryptor::StreamType stream_type) {
  switch (stream_type) {
    case media::Decryptor::kAudio:
      pending_audio_decode_request_id_ = 0;
      if (!pending_audio_decode_cb_.is_null())
        base::ResetAndReturn(&pending_audio_decode_cb_).Run(
            media::Decryptor::kSuccess, media::Decryptor::AudioBuffers());
      break;
    case media::Decryptor::kVideo:
      pending_video_decode_request_id_ = 0;
      if (!pending_video_decode_cb_.is_null())
        base::ResetAndReturn(&pending_video_decode_cb_).Run(
            media::Decryptor::kSuccess, NULL);
      break;
    default:
      NOTREACHED();
  }
}

bool ContentDecryptorDelegate::MakeMediaBufferResource(
    media::Decryptor::StreamType stream_type,
    const uint8* data, int size,
    ScopedPPResource* resource) {
  TRACE_EVENT0("eme", "ContentDecryptorDelegate::MakeMediaBufferResource");

  DCHECK(resource);

  if (!data || !size) {
    DCHECK(!data && !size);
    resource->Release();
    return true;
  }

  DCHECK(stream_type == media::Decryptor::kAudio ||
         stream_type == media::Decryptor::kVideo);
  ScopedPPResource& media_resource = (stream_type == media::Decryptor::kAudio) ?
      audio_input_resource_ : video_input_resource_;
  int& media_resource_size = (stream_type == media::Decryptor::kAudio) ?
      audio_input_resource_size_ : video_input_resource_size_;

  if (media_resource_size < size) {
    // Media resource size starts from |kMinimumMediaBufferSize| and grows
    // exponentially to avoid frequent re-allocation of PPB_Buffer_Impl,
    // which is usually expensive. Since input media buffers are compressed,
    // they are usually small (compared to outputs). The over-allocated memory
    // should be negligible.

    if (media_resource_size == 0) {
      const int kMinimumMediaBufferSize = 1024;
      media_resource_size = kMinimumMediaBufferSize;
    }

    while (media_resource_size < size)
      media_resource_size *= 2;

    DVLOG(2) << "Size of media buffer for "
             << ((stream_type == media::Decryptor::kAudio) ? "audio" : "video")
             << " stream bumped to " << media_resource_size
             << " bytes to fit input.";
    media_resource = ScopedPPResource(
        ScopedPPResource::PassRef(),
        PPB_Buffer_Impl::Create(pp_instance_, media_resource_size));
    if (!media_resource.get()) {
      media_resource_size = 0;
      return false;
    }
  }

  EnterResourceNoLock<PPB_Buffer_API> enter(media_resource, true);
  if (enter.failed()) {
    media_resource.Release();
    media_resource_size = 0;
    return false;
  }

  BufferAutoMapper mapper(enter.object());
  if (!mapper.data() || mapper.size() < static_cast<size_t>(size)) {
    media_resource.Release();
    media_resource_size = 0;
    return false;
  }
  memcpy(mapper.data(), data, size);

  *resource = media_resource;
  return true;
}

}  // namespace ppapi
}  // namespace webkit
