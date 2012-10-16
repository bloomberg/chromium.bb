// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/media/crypto/ppapi/clear_key_cdm.h"

#include <vector>

#include "base/bind.h"
#include "base/logging.h"
#include "base/time.h"
#include "media/base/decoder_buffer.h"

static const char kClearKeyCdmVersion[] = "0.1.0.0";

static scoped_refptr<media::DecoderBuffer> CopyDecoderBufferFrom(
    const cdm::InputBuffer& input_buffer) {
  // TODO(tomfinegan): Get rid of this copy.
  scoped_refptr<media::DecoderBuffer> output_buffer =
      media::DecoderBuffer::CopyFrom(input_buffer.data, input_buffer.data_size);

  std::vector<media::SubsampleEntry> subsamples;
  for (int32_t i = 0; i < input_buffer.num_subsamples; ++i) {
    media::SubsampleEntry subsample;
    subsample.clear_bytes = input_buffer.subsamples[i].clear_bytes;
    subsample.cypher_bytes = input_buffer.subsamples[i].cipher_bytes;
    subsamples.push_back(subsample);
  }

  scoped_ptr<media::DecryptConfig> decrypt_config(new media::DecryptConfig(
      std::string(reinterpret_cast<const char*>(input_buffer.key_id),
                  input_buffer.key_id_size),
      std::string(reinterpret_cast<const char*>(input_buffer.iv),
                  input_buffer.iv_size),
      input_buffer.data_offset,
      subsamples));

  output_buffer->SetDecryptConfig(decrypt_config.Pass());
  output_buffer->SetTimestamp(
      base::TimeDelta::FromMicroseconds(input_buffer.timestamp));

  return output_buffer;
}

template<typename Type>
class ScopedResetter {
 public:
  explicit ScopedResetter(Type* object) : object_(object) {}
  ~ScopedResetter() {
    object_->Reset();
  }

 private:
  Type* const object_;
};

template<typename Type>
static Type* AllocateAndCopy(const Type* data, int size) {
  COMPILE_ASSERT(sizeof(Type) == 1, type_size_is_not_one);
  Type* copy = new Type[size];
  memcpy(copy, data, size);
  return copy;
}

cdm::ContentDecryptionModule* CreateCdmInstance(
    cdm::Allocator* allocator, cdm::CdmHost* host) {
  return new webkit_media::ClearKeyCdm(allocator, host);
}

void DestroyCdmInstance(cdm::ContentDecryptionModule* instance) {
  delete instance;
}

const char* GetCdmVersion() {
  return kClearKeyCdmVersion;
}

namespace webkit_media {

ClearKeyCdm::Client::Client() : status_(kKeyError), key_message_length_(0) {}

ClearKeyCdm::Client::~Client() {}

void ClearKeyCdm::Client::Reset() {
  status_ = kKeyError;
  session_id_.clear();
  key_message_.reset();
  key_message_length_ = 0;
  default_url_.clear();
}

void ClearKeyCdm::Client::KeyAdded(const std::string& key_system,
                                   const std::string& session_id) {
  status_ = kKeyAdded;
  session_id_ = session_id;
}

void ClearKeyCdm::Client::KeyError(const std::string& key_system,
                                   const std::string& session_id,
                                   media::Decryptor::KeyError error_code,
                                   int system_code) {
  status_ = kKeyError;
  session_id_ = session_id;
}

void ClearKeyCdm::Client::KeyMessage(const std::string& key_system,
                                     const std::string& session_id,
                                     scoped_array<uint8> message,
                                     int message_length,
                                     const std::string& default_url) {
  status_ = kKeyMessage;
  session_id_ = session_id;
  key_message_ = message.Pass();
  key_message_length_ = message_length;
}

void ClearKeyCdm::Client::NeedKey(const std::string& key_system,
                                  const std::string& session_id,
                                  scoped_array<uint8> init_data,
                                  int init_data_length) {
  // In the current implementation of AesDecryptor, NeedKey is not used.
  // If no key is available to decrypt an input buffer, it returns kNoKey to
  // the caller instead of firing NeedKey.
  NOTREACHED();
}

ClearKeyCdm::ClearKeyCdm(cdm::Allocator* allocator, cdm::CdmHost*)
    : decryptor_(&client_), allocator_(allocator) {
  DCHECK(allocator_);
}

ClearKeyCdm::~ClearKeyCdm() {}

cdm::Status ClearKeyCdm::GenerateKeyRequest(const uint8_t* init_data,
                                            int init_data_size,
                                            cdm::KeyMessage* key_request) {
  DVLOG(1) << "GenerateKeyRequest()";
  base::AutoLock auto_lock(client_lock_);
  ScopedResetter<Client> auto_resetter(&client_);
  decryptor_.GenerateKeyRequest("", init_data, init_data_size);

  if (client_.status() != Client::kKeyMessage)
    return cdm::kSessionError;

  DCHECK(key_request);
  key_request->set_session_id(client_.session_id().data(),
                              client_.session_id().size());

  // TODO(tomfinegan): Get rid of this copy.
  key_request->set_message(allocator_->Allocate(client_.key_message_length()));

  DCHECK(key_request->message());
  DCHECK_EQ(key_request->message()->size(), client_.key_message_length());
  memcpy(reinterpret_cast<void*>(key_request->message()->data()),
         reinterpret_cast<const void*>(client_.key_message()),
         client_.key_message_length());

  key_request->set_default_url(client_.default_url().data(),
                               client_.default_url().size());
  return cdm::kSuccess;
}

cdm::Status ClearKeyCdm::AddKey(const char* session_id,
                                int session_id_size,
                                const uint8_t* key,
                                int key_size,
                                const uint8_t* key_id,
                                int key_id_size) {
  DVLOG(1) << "AddKey()";
  base::AutoLock auto_lock(client_lock_);
  ScopedResetter<Client> auto_resetter(&client_);
  decryptor_.AddKey("", key, key_size, key_id, key_id_size,
                    std::string(session_id, session_id_size));

  if (client_.status() != Client::kKeyAdded)
    return cdm::kSessionError;

  return cdm::kSuccess;
}

cdm::Status ClearKeyCdm::CancelKeyRequest(const char* session_id,
                                          int session_id_size) {
  DVLOG(1) << "CancelKeyRequest()";
  base::AutoLock auto_lock(client_lock_);
  ScopedResetter<Client> auto_resetter(&client_);
  decryptor_.CancelKeyRequest("", std::string(session_id, session_id_size));
  return cdm::kSuccess;
}

void ClearKeyCdm::TimerExpired(cdm::KeyMessage* msg, bool* populated) {
  // TODO(xhwang): do something with this?
  NOTREACHED() << "Wouldn't it be nice if CdmHost::SetTimer() was used?";
}

static void CopyDecryptResults(
    media::Decryptor::Status* status_copy,
    scoped_refptr<media::DecoderBuffer>* buffer_copy,
    media::Decryptor::Status status,
    const scoped_refptr<media::DecoderBuffer>& buffer) {
  *status_copy = status;
  *buffer_copy = buffer;
}

cdm::Status ClearKeyCdm::Decrypt(
    const cdm::InputBuffer& encrypted_buffer,
    cdm::DecryptedBlock* decrypted_block) {
  DVLOG(1) << "Decrypt()";

  scoped_refptr<media::DecoderBuffer> decoder_buffer =
      CopyDecoderBufferFrom(encrypted_buffer);

  // Callback is called synchronously, so we can use variables on the stack.
  media::Decryptor::Status status;
  scoped_refptr<media::DecoderBuffer> buffer;
  decryptor_.Decrypt(decoder_buffer,
                     base::Bind(&CopyDecryptResults, &status, &buffer));

  if (status == media::Decryptor::kError)
    return cdm::kDecryptError;

  if (status == media::Decryptor::kNoKey)
    return cdm::kNoKey;

  DCHECK(buffer);
  int data_size = buffer->GetDataSize();

  decrypted_block->set_buffer(allocator_->Allocate(data_size));
  memcpy(reinterpret_cast<void*>(decrypted_block->buffer()->data()),
         buffer->GetData(),
         data_size);

  decrypted_block->set_timestamp(buffer->GetTimestamp().InMicroseconds());
  return cdm::kSuccess;
}

cdm::Status ClearKeyCdm::InitializeVideoDecoder(
    const cdm::VideoDecoderConfig& video_decoder_config) {
#if !defined(CLEAR_KEY_CDM_USE_FAKE_VIDEO_DECODER)
  NOTIMPLEMENTED();
  return cdm::kSessionError;
#else
  video_size_ = video_decoder_config.coded_size;
  return cdm::kSuccess;
#endif  // CLEAR_KEY_CDM_USE_FAKE_VIDEO_DECODER
}

void ClearKeyCdm::ResetDecoder(cdm::StreamType) {
  NOTIMPLEMENTED();
}

void ClearKeyCdm::DeinitializeDecoder(cdm::StreamType) {
  NOTIMPLEMENTED();
}

cdm::Status ClearKeyCdm::DecryptAndDecodeFrame(
    const cdm::InputBuffer& encrypted_buffer,
    cdm::VideoFrame* video_frame) {
  if (!encrypted_buffer.data) {
    video_frame->set_format(cdm::kEmptyVideoFrame);
    return cdm::kSuccess;
  }

  scoped_refptr<media::DecoderBuffer> decoder_buffer =
      CopyDecoderBufferFrom(encrypted_buffer);

  // Callback is called synchronously, so we can use variables on the stack.
  media::Decryptor::Status status;
  scoped_refptr<media::DecoderBuffer> buffer;
  decryptor_.Decrypt(decoder_buffer,
                     base::Bind(&CopyDecryptResults, &status, &buffer));

  if (status == media::Decryptor::kError)
    return cdm::kDecryptError;

  if (status == media::Decryptor::kNoKey)
    return cdm::kNoKey;

#if !defined(CLEAR_KEY_CDM_USE_FAKE_VIDEO_DECODER)
  NOTIMPLEMENTED();
  return cdm::kDecodeError;
#else
  GenerateFakeVideoFrame(decoder_buffer->GetTimestamp(), video_frame);
  return cdm::kSuccess;
#endif  // CLEAR_KEY_CDM_USE_FAKE_VIDEO_DECODER
}

#if defined(CLEAR_KEY_CDM_USE_FAKE_VIDEO_DECODER)
void ClearKeyCdm::GenerateFakeVideoFrame(base::TimeDelta timestamp,
                                         cdm::VideoFrame* video_frame) {
  // Choose non-zero alignment and padding on purpose for testing.
  const int kAlignment = 8;
  const int kPadding = 16;
  const int kPlanePadding = 128;

  int width = video_size_.width;
  int height = video_size_.height;
  DCHECK(width % 2 == 0);
  DCHECK(height % 2 == 0);

  int y_stride = (width + kAlignment - 1) / kAlignment * kAlignment + kPadding;
  int uv_stride =
      (width / 2 + kAlignment - 1) / kAlignment * kAlignment + kPadding;
  int y_rows = height;
  int uv_rows = height / 2;
  int y_offset = 0;
  int v_offset = y_stride * y_rows + kPlanePadding;
  int u_offset = v_offset + uv_stride * uv_rows + kPlanePadding;
  int frame_size = u_offset + uv_stride * uv_rows + kPlanePadding;

  video_frame->set_format(cdm::kYv12);
  video_frame->set_size(video_size_);
  video_frame->set_frame_buffer(allocator_->Allocate(frame_size));
  video_frame->set_plane_offset(cdm::VideoFrame::kYPlane, y_offset);
  video_frame->set_plane_offset(cdm::VideoFrame::kVPlane, v_offset);
  video_frame->set_plane_offset(cdm::VideoFrame::kUPlane, u_offset);
  video_frame->set_stride(cdm::VideoFrame::kYPlane, y_stride);
  video_frame->set_stride(cdm::VideoFrame::kVPlane, uv_stride);
  video_frame->set_stride(cdm::VideoFrame::kUPlane, uv_stride);
  video_frame->set_timestamp(timestamp.InMicroseconds());

  static unsigned char color = 0;
  color += 10;

  memset(reinterpret_cast<void*>(video_frame->frame_buffer()->data()),
         color, frame_size);
}
#endif  // CLEAR_KEY_CDM_USE_FAKE_VIDEO_DECODER

}  // namespace webkit_media
