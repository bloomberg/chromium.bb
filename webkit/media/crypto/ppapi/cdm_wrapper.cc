// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstring>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/pp_stdint.h"
#include "ppapi/c/private/pp_content_decryptor.h"
#include "ppapi/cpp/completion_callback.h"
#include "ppapi/cpp/core.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/logging.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/pass_ref.h"
#include "ppapi/cpp/resource.h"
#include "ppapi/cpp/var.h"
#include "ppapi/cpp/var_array_buffer.h"
#include "ppapi/cpp/dev/buffer_dev.h"
#include "ppapi/cpp/private/content_decryptor_private.h"
#include "ppapi/utility/completion_callback_factory.h"
#include "webkit/media/crypto/ppapi/cdm/content_decryption_module.h"
#include "webkit/media/crypto/ppapi/linked_ptr.h"

namespace {

bool IsMainThread() {
  return pp::Module::Get()->core()->IsMainThread();
}

// Posts a task to run |cb| on the main thread. The task is posted even if the
// current thread is the main thread.
void PostOnMain(pp::CompletionCallback cb) {
  pp::Module::Get()->core()->CallOnMainThread(0, cb, PP_OK);
}

// Ensures |cb| is called on the main thread, either because the current thread
// is the main thread or by posting it to the main thread.
void CallOnMain(pp::CompletionCallback cb) {
  // TODO(tomfinegan): This is only necessary because PPAPI doesn't allow calls
  // off the main thread yet. Remove this once the change lands.
  if (IsMainThread())
    cb.Run(PP_OK);
  else
    PostOnMain(cb);
}

// Configures a cdm::InputBuffer. |subsamples| must exist as long as
// |input_buffer| is in use.
void ConfigureInputBuffer(
    const pp::Buffer_Dev& encrypted_buffer,
    const PP_EncryptedBlockInfo& encrypted_block_info,
    std::vector<cdm::SubsampleEntry>* subsamples,
    cdm::InputBuffer* input_buffer) {
  PP_DCHECK(subsamples);
  PP_DCHECK(!encrypted_buffer.is_null());

  input_buffer->data = static_cast<uint8_t*>(encrypted_buffer.data());
  input_buffer->data_size = encrypted_block_info.data_size;
  PP_DCHECK(encrypted_buffer.size() >=
            static_cast<uint32_t>(input_buffer->data_size));
  input_buffer->data_offset = encrypted_block_info.data_offset;
  input_buffer->key_id = encrypted_block_info.key_id;
  input_buffer->key_id_size = encrypted_block_info.key_id_size;
  input_buffer->iv = encrypted_block_info.iv;
  input_buffer->iv_size = encrypted_block_info.iv_size;
  input_buffer->num_subsamples = encrypted_block_info.num_subsamples;

  if (encrypted_block_info.num_subsamples > 0) {
    subsamples->reserve(encrypted_block_info.num_subsamples);

    for (uint32_t i = 0; i < encrypted_block_info.num_subsamples; ++i) {
      subsamples->push_back(cdm::SubsampleEntry(
          encrypted_block_info.subsamples[i].clear_bytes,
          encrypted_block_info.subsamples[i].cipher_bytes));
    }

    input_buffer->subsamples = &(*subsamples)[0];
  }

  input_buffer->timestamp = encrypted_block_info.tracking_info.timestamp;
}

PP_DecryptResult CdmStatusToPpDecryptResult(cdm::Status status) {
  switch (status) {
    case cdm::kSuccess:
      return PP_DECRYPTRESULT_SUCCESS;
    case cdm::kNoKey:
      return PP_DECRYPTRESULT_DECRYPT_NOKEY;
    case cdm::kNeedMoreData:
      return PP_DECRYPTRESULT_NEEDMOREDATA;
    case cdm::kDecryptError:
      return PP_DECRYPTRESULT_DECRYPT_ERROR;
    case cdm::kDecodeError:
      return PP_DECRYPTRESULT_DECODE_ERROR;
    default:
      PP_NOTREACHED();
      return PP_DECRYPTRESULT_DECODE_ERROR;
  }
}

PP_DecryptedFrameFormat CdmVideoFormatToPpDecryptedFrameFormat(
    cdm::VideoFormat format) {
  switch (format) {
    case cdm::kYv12:
      return PP_DECRYPTEDFRAMEFORMAT_YV12;
    case cdm::kI420:
      return PP_DECRYPTEDFRAMEFORMAT_I420;
    default:
      return PP_DECRYPTEDFRAMEFORMAT_UNKNOWN;
  }
}

cdm::AudioDecoderConfig::AudioCodec PpAudioCodecToCdmAudioCodec(
    PP_AudioCodec codec) {
  switch (codec) {
    case PP_AUDIOCODEC_VORBIS:
      return cdm::AudioDecoderConfig::kCodecVorbis;
    case PP_AUDIOCODEC_AAC:
      return cdm::AudioDecoderConfig::kCodecAac;
    default:
      return cdm::AudioDecoderConfig::kUnknownAudioCodec;
  }
}

cdm::VideoDecoderConfig::VideoCodec PpVideoCodecToCdmVideoCodec(
    PP_VideoCodec codec) {
  switch (codec) {
    case PP_VIDEOCODEC_VP8:
      return cdm::VideoDecoderConfig::kCodecVp8;
    case PP_VIDEOCODEC_H264:
      return cdm::VideoDecoderConfig::kCodecH264;
    default:
      return cdm::VideoDecoderConfig::kUnknownVideoCodec;
  }
}

cdm::VideoDecoderConfig::VideoCodecProfile PpVCProfileToCdmVCProfile(
    PP_VideoCodecProfile profile) {
  switch (profile) {
    case PP_VIDEOCODECPROFILE_VP8_MAIN:
      return cdm::VideoDecoderConfig::kVp8ProfileMain;
    case PP_VIDEOCODECPROFILE_H264_BASELINE:
      return cdm::VideoDecoderConfig::kH264ProfileBaseline;
    case PP_VIDEOCODECPROFILE_H264_MAIN:
      return cdm::VideoDecoderConfig::kH264ProfileMain;
    case PP_VIDEOCODECPROFILE_H264_EXTENDED:
      return cdm::VideoDecoderConfig::kH264ProfileExtended;
    case PP_VIDEOCODECPROFILE_H264_HIGH:
      return cdm::VideoDecoderConfig::kH264ProfileHigh;
    case PP_VIDEOCODECPROFILE_H264_HIGH_10:
      return cdm::VideoDecoderConfig::kH264ProfileHigh10;
    case PP_VIDEOCODECPROFILE_H264_HIGH_422:
      return cdm::VideoDecoderConfig::kH264ProfileHigh422;
    case PP_VIDEOCODECPROFILE_H264_HIGH_444_PREDICTIVE:
      return cdm::VideoDecoderConfig::kH264ProfileHigh444Predictive;
    default:
      return cdm::VideoDecoderConfig::kUnknownVideoCodecProfile;
  }
}

cdm::VideoFormat PpDecryptedFrameFormatToCdmVideoFormat(
    PP_DecryptedFrameFormat format) {
  switch (format) {
    case PP_DECRYPTEDFRAMEFORMAT_YV12:
      return cdm::kYv12;
    case PP_DECRYPTEDFRAMEFORMAT_I420:
      return cdm::kI420;
    default:
      return cdm::kUnknownVideoFormat;
  }
}

cdm::StreamType PpDecryptorStreamTypeToCdmStreamType(
    PP_DecryptorStreamType stream_type) {
  switch (stream_type) {
    case PP_DECRYPTORSTREAMTYPE_AUDIO:
      return cdm::kStreamTypeAudio;
    case PP_DECRYPTORSTREAMTYPE_VIDEO:
      return cdm::kStreamTypeVideo;
  }

  PP_NOTREACHED();
  return cdm::kStreamTypeVideo;
}

}  // namespace

namespace webkit_media {

// cdm::Buffer implementation that provides access to memory owned by a
// pp::Buffer_Dev.
// This class holds a reference to the Buffer_Dev throughout its lifetime.
// TODO(xhwang): Find a better name. It's confusing to have PpbBuffer,
// pp::Buffer_Dev and PPB_Buffer_Dev.
class PpbBuffer : public cdm::Buffer {
 public:
  static PpbBuffer* Create(const pp::Buffer_Dev& buffer, uint32_t buffer_id) {
    PP_DCHECK(buffer.data());
    PP_DCHECK(buffer.size());
    PP_DCHECK(buffer_id);
    return new PpbBuffer(buffer, buffer_id);
  }

  // cdm::Buffer implementation.
  virtual void Destroy() OVERRIDE { delete this; }

  virtual int32_t Capacity() const OVERRIDE { return buffer_.size(); }

  virtual uint8_t* Data() OVERRIDE {
    return static_cast<uint8_t*>(buffer_.data());
  }

  virtual void SetSize(int32_t size) OVERRIDE {
    PP_DCHECK(size >= 0);
    PP_DCHECK(size < Capacity());
    if (size < 0 || size > Capacity()) {
      size_ = 0;
      return;
    }

    size_ = size;
  }

  virtual int32_t Size() const OVERRIDE { return size_; }

  pp::Buffer_Dev buffer_dev() const { return buffer_; }

  uint32_t buffer_id() const { return buffer_id_; }

 private:
  PpbBuffer(pp::Buffer_Dev buffer, uint32_t buffer_id)
      : buffer_(buffer),
        buffer_id_(buffer_id),
        size_(0) {}
  virtual ~PpbBuffer() {}

  pp::Buffer_Dev buffer_;
  uint32_t buffer_id_;
  int32_t size_;

  DISALLOW_COPY_AND_ASSIGN(PpbBuffer);
};

class PpbBufferAllocator : public cdm::Allocator {
 public:
  explicit PpbBufferAllocator(pp::Instance* instance)
      : instance_(instance),
        next_buffer_id_(1) {}
  virtual ~PpbBufferAllocator() {}

  // cdm::Allocator implementation.
  virtual cdm::Buffer* Allocate(int32_t capacity) OVERRIDE;

  // Releases the buffer with |buffer_id|. A buffer can be recycled after
  // it is released.
  void Release(uint32_t buffer_id);

 private:
  typedef std::map<uint32_t, pp::Buffer_Dev> AllocatedBufferMap;
  typedef std::multimap<int, std::pair<uint32_t, pp::Buffer_Dev> >
      FreeBufferMap;

  // Always pad new allocated buffer so that we don't need to reallocate
  // buffers frequently if requested sizes fluctuate slightly.
  static const int kBufferPadding = 512;

  // Maximum number of free buffers we can keep when allocating new buffers.
  static const int kFreeLimit = 3;

  pp::Buffer_Dev AllocateNewBuffer(int capacity);

  pp::Instance* const instance_;
  uint32_t next_buffer_id_;
  AllocatedBufferMap allocated_buffers_;
  FreeBufferMap free_buffers_;

  DISALLOW_COPY_AND_ASSIGN(PpbBufferAllocator);
};

cdm::Buffer* PpbBufferAllocator::Allocate(int32_t capacity) {
  PP_DCHECK(IsMainThread());

  if (capacity <= 0)
    return NULL;

  pp::Buffer_Dev buffer;
  uint32_t buffer_id = 0;

  // Reuse a buffer in the free list if there is one that fits |capacity|.
  // Otherwise, create a new one.
  FreeBufferMap::iterator found = free_buffers_.lower_bound(capacity);
  if (found == free_buffers_.end()) {
    // TODO(xhwang): Report statistics about how many new buffers are allocated.
    buffer = AllocateNewBuffer(capacity);
    if (buffer.is_null())
      return NULL;
    buffer_id = next_buffer_id_++;
  } else {
    buffer = found->second.second;
    buffer_id = found->second.first;
    free_buffers_.erase(found);
  }

  allocated_buffers_.insert(std::make_pair(buffer_id, buffer));

  return PpbBuffer::Create(buffer, buffer_id);
}

void PpbBufferAllocator::Release(uint32_t buffer_id) {
  if (!buffer_id)
    return;

  AllocatedBufferMap::iterator found = allocated_buffers_.find(buffer_id);
  if (found == allocated_buffers_.end())
    return;

  pp::Buffer_Dev& buffer = found->second;
  free_buffers_.insert(
      std::make_pair(buffer.size(), std::make_pair(buffer_id, buffer)));

  allocated_buffers_.erase(found);
}

pp::Buffer_Dev PpbBufferAllocator::AllocateNewBuffer(int32_t capacity) {
  // Destroy the smallest buffer before allocating a new bigger buffer if the
  // number of free buffers exceeds a limit. This mechanism helps avoid ending
  // up with too many small buffers, which could happen if the size to be
  // allocated keeps increasing.
  if (free_buffers_.size() >= static_cast<uint32_t>(kFreeLimit))
    free_buffers_.erase(free_buffers_.begin());

  // Creation of pp::Buffer_Dev is expensive! It involves synchronous IPC calls.
  // That's why we try to avoid AllocateNewBuffer() as much as we can.
  return pp::Buffer_Dev(instance_, capacity + kBufferPadding);
}

class DecryptedBlockImpl : public cdm::DecryptedBlock {
 public:
  DecryptedBlockImpl() : buffer_(NULL), timestamp_(0) {}
  virtual ~DecryptedBlockImpl() { if (buffer_) buffer_->Destroy(); }

  virtual void SetDecryptedBuffer(cdm::Buffer* buffer) OVERRIDE {
    buffer_ = static_cast<PpbBuffer*>(buffer);
  }
  virtual cdm::Buffer* DecryptedBuffer() OVERRIDE { return buffer_; }

  virtual void SetTimestamp(int64_t timestamp) OVERRIDE {
    timestamp_ = timestamp;
  }
  virtual int64_t Timestamp() const OVERRIDE { return timestamp_; }

 private:
  PpbBuffer* buffer_;
  int64_t timestamp_;

  DISALLOW_COPY_AND_ASSIGN(DecryptedBlockImpl);
};

class VideoFrameImpl : public cdm::VideoFrame {
 public:
  VideoFrameImpl();
  virtual ~VideoFrameImpl();

  virtual void SetFormat(cdm::VideoFormat format) OVERRIDE {
    format_ = format;
  }
  virtual cdm::VideoFormat Format() const OVERRIDE { return format_; }

  virtual void SetSize(cdm::Size size) OVERRIDE { size_ = size; }
  virtual cdm::Size Size() const OVERRIDE { return size_; }

  virtual void SetFrameBuffer(cdm::Buffer* frame_buffer) OVERRIDE {
    frame_buffer_ = static_cast<PpbBuffer*>(frame_buffer);
  }
  virtual cdm::Buffer* FrameBuffer() OVERRIDE { return frame_buffer_; }

  virtual void SetPlaneOffset(cdm::VideoFrame::VideoPlane plane,
                              int32_t offset) OVERRIDE {
    PP_DCHECK(0 <= plane && plane < kMaxPlanes);
    PP_DCHECK(offset >= 0);
    plane_offsets_[plane] = offset;
  }
  virtual int32_t PlaneOffset(VideoPlane plane) OVERRIDE {
    PP_DCHECK(0 <= plane && plane < kMaxPlanes);
    return plane_offsets_[plane];
  }

  virtual void SetStride(VideoPlane plane, int32_t stride) OVERRIDE {
    PP_DCHECK(0 <= plane && plane < kMaxPlanes);
    strides_[plane] = stride;
  }
  virtual int32_t Stride(VideoPlane plane) OVERRIDE {
    PP_DCHECK(0 <= plane && plane < kMaxPlanes);
    return strides_[plane];
  }

  virtual void SetTimestamp(int64_t timestamp) OVERRIDE {
    timestamp_ = timestamp;
  }
  virtual int64_t Timestamp() const OVERRIDE { return timestamp_; }

 private:
  // The video buffer format.
  cdm::VideoFormat format_;

  // Width and height of the video frame.
  cdm::Size size_;

  // The video frame buffer.
  PpbBuffer* frame_buffer_;

  // Array of data pointers to each plane in the video frame buffer.
  int32_t plane_offsets_[kMaxPlanes];

  // Array of strides for each plane, typically greater or equal to the width
  // of the surface divided by the horizontal sampling period.  Note that
  // strides can be negative.
  int32_t strides_[kMaxPlanes];

  // Presentation timestamp in microseconds.
  int64_t timestamp_;

  DISALLOW_COPY_AND_ASSIGN(VideoFrameImpl);
};

VideoFrameImpl::VideoFrameImpl()
    : format_(cdm::kUnknownVideoFormat),
      frame_buffer_(NULL),
      timestamp_(0) {
  for (int32_t i = 0; i < kMaxPlanes; ++i) {
    plane_offsets_[i] = 0;
    strides_[i] = 0;
  }
}

VideoFrameImpl::~VideoFrameImpl() {
  if (frame_buffer_)
    frame_buffer_->Destroy();
}

class AudioFramesImpl : public cdm::AudioFrames {
 public:
  AudioFramesImpl() : buffer_(NULL) {}
  virtual ~AudioFramesImpl() {
    if (buffer_)
      buffer_->Destroy();
  }

  // AudioFrames implementation.
  virtual void SetFrameBuffer(cdm::Buffer* buffer) OVERRIDE {
    buffer_ = static_cast<PpbBuffer*>(buffer);
  }
  virtual cdm::Buffer* FrameBuffer() OVERRIDE {
    return buffer_;
  }

 private:
  PpbBuffer* buffer_;

  DISALLOW_COPY_AND_ASSIGN(AudioFramesImpl);
};

// A wrapper class for abstracting away PPAPI interaction and threading for a
// Content Decryption Module (CDM).
class CdmWrapper : public pp::Instance,
                   public pp::ContentDecryptor_Private,
                   public cdm::Host {
 public:
  CdmWrapper(PP_Instance instance, pp::Module* module);
  virtual ~CdmWrapper();

  // pp::Instance implementation.
  virtual bool Init(uint32_t argc, const char* argn[], const char* argv[]) {
    return true;
  }

  // PPP_ContentDecryptor_Private implementation.
  // Note: Results of calls to these methods must be reported through the
  // PPB_ContentDecryptor_Private interface.
  virtual void GenerateKeyRequest(const std::string& key_system,
                                  const std::string& type,
                                  pp::VarArrayBuffer init_data) OVERRIDE;
  virtual void AddKey(const std::string& session_id,
                      pp::VarArrayBuffer key,
                      pp::VarArrayBuffer init_data) OVERRIDE;
  virtual void CancelKeyRequest(const std::string& session_id) OVERRIDE;
  virtual void Decrypt(
      pp::Buffer_Dev encrypted_buffer,
      const PP_EncryptedBlockInfo& encrypted_block_info) OVERRIDE;
  virtual void InitializeAudioDecoder(
      const PP_AudioDecoderConfig& decoder_config,
      pp::Buffer_Dev extra_data_buffer) OVERRIDE;
  virtual void InitializeVideoDecoder(
      const PP_VideoDecoderConfig& decoder_config,
      pp::Buffer_Dev extra_data_buffer) OVERRIDE;
  virtual void DeinitializeDecoder(PP_DecryptorStreamType decoder_type,
                                   uint32_t request_id) OVERRIDE;
  virtual void ResetDecoder(PP_DecryptorStreamType decoder_type,
                            uint32_t request_id) OVERRIDE;
  virtual void DecryptAndDecode(
      PP_DecryptorStreamType decoder_type,
      pp::Buffer_Dev encrypted_buffer,
      const PP_EncryptedBlockInfo& encrypted_block_info) OVERRIDE;

  // cdm::Host implementation.
  virtual void SetTimer(int64_t delay_ms, void* context) OVERRIDE;
  virtual double GetCurrentWallTimeInSeconds() OVERRIDE;
  virtual void SendKeyMessage(
      const char* session_id, int32_t session_id_length,
      const char* message, int32_t message_length,
      const char* default_url, int32_t default_url_length) OVERRIDE;
  virtual void SendKeyError(const char* session_id,
                            int32_t session_id_length,
                            cdm::MediaKeyError error_code,
                            uint32_t system_code) OVERRIDE;

 private:
  struct SessionInfo {
    SessionInfo(const std::string& key_system_in,
                const std::string& session_id_in)
        : key_system(key_system_in),
          session_id(session_id_in) {}
    const std::string key_system;
    const std::string session_id;
  };

  typedef linked_ptr<DecryptedBlockImpl> LinkedDecryptedBlock;
  typedef linked_ptr<VideoFrameImpl> LinkedVideoFrame;
  typedef linked_ptr<AudioFramesImpl> LinkedAudioFrames;

  void SendUnknownKeyError(const std::string& key_system,
                           const std::string& session_id);

  void SendKeyAdded(const std::string& key_system,
                    const std::string& session_id);

  void SendKeyErrorInternal(const std::string& key_system,
                            const std::string& session_id,
                            cdm::MediaKeyError error_code,
                            uint32_t system_code);

  // <code>PPB_ContentDecryptor_Private</code> dispatchers. These are passed to
  // <code>callback_factory_</code> to ensure that calls into
  // <code>PPP_ContentDecryptor_Private</code> are asynchronous.
  void KeyAdded(int32_t result, const SessionInfo& session_info);
  void KeyMessage(int32_t result,
                  const SessionInfo& session_info,
                  const std::string& message,
                  const std::string& default_url);
  void KeyError(int32_t result,
                const SessionInfo& session_info,
                cdm::MediaKeyError error_code,
                uint32_t system_code);
  void DeliverBlock(int32_t result,
                    const cdm::Status& status,
                    const LinkedDecryptedBlock& decrypted_block,
                    const PP_DecryptTrackingInfo& tracking_info);
  void DecoderInitializeDone(int32_t result,
                             PP_DecryptorStreamType decoder_type,
                             uint32_t request_id,
                             bool success);
  void DecoderDeinitializeDone(int32_t result,
                               PP_DecryptorStreamType decoder_type,
                               uint32_t request_id);
  void DecoderResetDone(int32_t result,
                        PP_DecryptorStreamType decoder_type,
                        uint32_t request_id);
  void DeliverFrame(int32_t result,
                    const cdm::Status& status,
                    const LinkedVideoFrame& video_frame,
                    const PP_DecryptTrackingInfo& tracking_info);
  void DeliverSamples(int32_t result,
                      const cdm::Status& status,
                      const LinkedAudioFrames& audio_frames,
                      const PP_DecryptTrackingInfo& tracking_info);

  // Helper for SetTimer().
  void TimerExpired(int32_t result, void* context);

  bool IsValidVideoFrame(const LinkedVideoFrame& video_frame);

  PpbBufferAllocator allocator_;
  pp::CompletionCallbackFactory<CdmWrapper> callback_factory_;
  cdm::ContentDecryptionModule* cdm_;
  std::string key_system_;

  DISALLOW_COPY_AND_ASSIGN(CdmWrapper);
};

CdmWrapper::CdmWrapper(PP_Instance instance, pp::Module* module)
    : pp::Instance(instance),
      pp::ContentDecryptor_Private(this),
      allocator_(this),
      cdm_(NULL) {
  callback_factory_.Initialize(this);
}

CdmWrapper::~CdmWrapper() {
  if (cdm_)
    DestroyCdmInstance(cdm_);
}

void CdmWrapper::GenerateKeyRequest(const std::string& key_system,
                                    const std::string& type,
                                    pp::VarArrayBuffer init_data) {
  PP_DCHECK(!key_system.empty());
  PP_DCHECK(key_system_.empty() || key_system_ == key_system);

  if (!cdm_) {
    cdm_ = CreateCdmInstance(key_system.data(), key_system.size(),
                             &allocator_, this);
    PP_DCHECK(cdm_);
    if (!cdm_) {
      SendUnknownKeyError(key_system, "");
      return;
    }
  }

  // Must be set here in case the CDM synchronously calls a cdm::Host method.
  // Clear below on error.
  // TODO(ddorwin): Remove this when key_system is added to cdm::Host methods.
  key_system_ = key_system;
  cdm::Status status = cdm_->GenerateKeyRequest(
      type.data(), type.size(),
      static_cast<const uint8_t*>(init_data.Map()),
      init_data.ByteLength());
  PP_DCHECK(status == cdm::kSuccess || status == cdm::kSessionError);
  if (status != cdm::kSuccess) {
    key_system_.clear();  // See comment above.
    return;
  }

  key_system_ = key_system;
}

void CdmWrapper::AddKey(const std::string& session_id,
                        pp::VarArrayBuffer key,
                        pp::VarArrayBuffer init_data) {
  PP_DCHECK(cdm_);  // GenerateKeyRequest() should have succeeded.
  if (!cdm_) {
    SendUnknownKeyError(key_system_, session_id);
    return;
  }

  const uint8_t* key_ptr = static_cast<const uint8_t*>(key.Map());
  int key_size = key.ByteLength();
  const uint8_t* init_data_ptr = static_cast<const uint8_t*>(init_data.Map());
  int init_data_size = init_data.ByteLength();

  if (!key_ptr || key_size <= 0 || !init_data_ptr || init_data_size <= 0) {
    SendUnknownKeyError(key_system_, session_id);
    return;
  }

  cdm::Status status = cdm_->AddKey(session_id.data(), session_id.size(),
                                    key_ptr, key_size,
                                    init_data_ptr, init_data_size);
  PP_DCHECK(status == cdm::kSuccess || status == cdm::kSessionError);
  if (status != cdm::kSuccess) {
    SendUnknownKeyError(key_system_, session_id);
    return;
  }

  SendKeyAdded(key_system_, session_id);
}

void CdmWrapper::CancelKeyRequest(const std::string& session_id) {
  PP_DCHECK(cdm_);  // GenerateKeyRequest() should have succeeded.
  if (!cdm_) {
    SendUnknownKeyError(key_system_, session_id);
    return;
  }

  cdm::Status status = cdm_->CancelKeyRequest(session_id.data(),
                                              session_id.size());
  PP_DCHECK(status == cdm::kSuccess || status == cdm::kSessionError);
  if (status != cdm::kSuccess)
    SendUnknownKeyError(key_system_, session_id);
}

// Note: In the following decryption/decoding related functions, errors are NOT
// reported via KeyError, but are reported via corresponding PPB calls.

void CdmWrapper::Decrypt(pp::Buffer_Dev encrypted_buffer,
                         const PP_EncryptedBlockInfo& encrypted_block_info) {
  PP_DCHECK(cdm_);  // GenerateKeyRequest() should have succeeded.
  PP_DCHECK(!encrypted_buffer.is_null());

  // Release a buffer that the caller indicated it is finished with.
  allocator_.Release(encrypted_block_info.tracking_info.buffer_id);

  cdm::Status status = cdm::kDecryptError;
  LinkedDecryptedBlock decrypted_block(new DecryptedBlockImpl());

  if (cdm_) {
    cdm::InputBuffer input_buffer;
    std::vector<cdm::SubsampleEntry> subsamples;
    ConfigureInputBuffer(encrypted_buffer, encrypted_block_info, &subsamples,
                         &input_buffer);
    status = cdm_->Decrypt(input_buffer, decrypted_block.get());
    PP_DCHECK(status != cdm::kSuccess ||
              (decrypted_block->DecryptedBuffer() &&
               decrypted_block->DecryptedBuffer()->Size()));
  }

  CallOnMain(callback_factory_.NewCallback(
      &CdmWrapper::DeliverBlock,
      status,
      decrypted_block,
      encrypted_block_info.tracking_info));
}

void CdmWrapper::InitializeAudioDecoder(
    const PP_AudioDecoderConfig& decoder_config,
    pp::Buffer_Dev extra_data_buffer) {
  PP_DCHECK(cdm_);  // GenerateKeyRequest() should have succeeded.

  cdm::Status status = cdm::kSessionError;
  if (cdm_) {
    cdm::AudioDecoderConfig cdm_decoder_config;
    cdm_decoder_config.codec =
        PpAudioCodecToCdmAudioCodec(decoder_config.codec);
    cdm_decoder_config.channel_count = decoder_config.channel_count;
    cdm_decoder_config.bits_per_channel = decoder_config.bits_per_channel;
    cdm_decoder_config.samples_per_second = decoder_config.samples_per_second;
    cdm_decoder_config.extra_data =
        static_cast<uint8_t*>(extra_data_buffer.data());
    cdm_decoder_config.extra_data_size =
        static_cast<int32_t>(extra_data_buffer.size());
    status = cdm_->InitializeAudioDecoder(cdm_decoder_config);
  }

  CallOnMain(callback_factory_.NewCallback(
      &CdmWrapper::DecoderInitializeDone,
      PP_DECRYPTORSTREAMTYPE_AUDIO,
      decoder_config.request_id,
      status == cdm::kSuccess));
}

void CdmWrapper::InitializeVideoDecoder(
    const PP_VideoDecoderConfig& decoder_config,
    pp::Buffer_Dev extra_data_buffer) {
  PP_DCHECK(cdm_);  // GenerateKeyRequest() should have succeeded.

  cdm::Status status = cdm::kSessionError;
  if (cdm_) {
    cdm::VideoDecoderConfig cdm_decoder_config;
    cdm_decoder_config.codec =
        PpVideoCodecToCdmVideoCodec(decoder_config.codec);
    cdm_decoder_config.profile =
        PpVCProfileToCdmVCProfile(decoder_config.profile);
    cdm_decoder_config.format =
        PpDecryptedFrameFormatToCdmVideoFormat(decoder_config.format);
    cdm_decoder_config.coded_size.width = decoder_config.width;
    cdm_decoder_config.coded_size.height = decoder_config.height;
    cdm_decoder_config.extra_data =
        static_cast<uint8_t*>(extra_data_buffer.data());
    cdm_decoder_config.extra_data_size =
        static_cast<int32_t>(extra_data_buffer.size());
    status = cdm_->InitializeVideoDecoder(cdm_decoder_config);
  }

  CallOnMain(callback_factory_.NewCallback(
      &CdmWrapper::DecoderInitializeDone,
      PP_DECRYPTORSTREAMTYPE_VIDEO,
      decoder_config.request_id,
      status == cdm::kSuccess));
}

void CdmWrapper::DeinitializeDecoder(PP_DecryptorStreamType decoder_type,
                                     uint32_t request_id) {
  PP_DCHECK(cdm_);  // GenerateKeyRequest() should have succeeded.
  if (cdm_) {
    cdm_->DeinitializeDecoder(
        PpDecryptorStreamTypeToCdmStreamType(decoder_type));
  }

  CallOnMain(callback_factory_.NewCallback(
      &CdmWrapper::DecoderDeinitializeDone,
      decoder_type,
      request_id));
}

void CdmWrapper::ResetDecoder(PP_DecryptorStreamType decoder_type,
                              uint32_t request_id) {
  PP_DCHECK(cdm_);  // GenerateKeyRequest() should have succeeded.
  if (cdm_)
    cdm_->ResetDecoder(PpDecryptorStreamTypeToCdmStreamType(decoder_type));

  CallOnMain(callback_factory_.NewCallback(&CdmWrapper::DecoderResetDone,
                                           decoder_type,
                                           request_id));
}

void CdmWrapper::DecryptAndDecode(
    PP_DecryptorStreamType decoder_type,
    pp::Buffer_Dev encrypted_buffer,
    const PP_EncryptedBlockInfo& encrypted_block_info) {
  PP_DCHECK(cdm_);  // GenerateKeyRequest() should have succeeded.

  // Release a buffer that the caller indicated it is finished with.
  allocator_.Release(encrypted_block_info.tracking_info.buffer_id);

  cdm::InputBuffer input_buffer;
  std::vector<cdm::SubsampleEntry> subsamples;
  if (cdm_ && !encrypted_buffer.is_null()) {
    ConfigureInputBuffer(encrypted_buffer,
                         encrypted_block_info,
                         &subsamples,
                         &input_buffer);
  }

  cdm::Status status = cdm::kDecodeError;

  switch (decoder_type) {
    case PP_DECRYPTORSTREAMTYPE_VIDEO: {
      LinkedVideoFrame video_frame(new VideoFrameImpl());
      if (cdm_)
        status = cdm_->DecryptAndDecodeFrame(input_buffer, video_frame.get());
      CallOnMain(callback_factory_.NewCallback(
          &CdmWrapper::DeliverFrame,
          status,
          video_frame,
          encrypted_block_info.tracking_info));
      return;
    }

    case PP_DECRYPTORSTREAMTYPE_AUDIO: {
      LinkedAudioFrames audio_frames(new AudioFramesImpl());
      if (cdm_) {
        status = cdm_->DecryptAndDecodeSamples(input_buffer,
                                               audio_frames.get());
      }
      CallOnMain(callback_factory_.NewCallback(
          &CdmWrapper::DeliverSamples,
          status,
          audio_frames,
          encrypted_block_info.tracking_info));
      return;
    }

    default:
      PP_NOTREACHED();
      return;
  }
}

void CdmWrapper::SetTimer(int64_t delay_ms, void* context) {
  // NOTE: doesn't really need to run on the main thread; could just as well run
  // on a helper thread if |cdm_| were thread-friendly and care was taken.  We
  // only use CallOnMainThread() here to get delayed-execution behavior.
  pp::Module::Get()->core()->CallOnMainThread(
      delay_ms,
      callback_factory_.NewCallback(&CdmWrapper::TimerExpired, context),
      PP_OK);
}

void CdmWrapper::TimerExpired(int32_t result, void* context) {
  PP_DCHECK(result == PP_OK);
  cdm_->TimerExpired(context);
}

double CdmWrapper::GetCurrentWallTimeInSeconds() {
  return pp::Module::Get()->core()->GetTime();
}

void CdmWrapper::SendKeyMessage(
    const char* session_id, int32_t session_id_length,
    const char* message, int32_t message_length,
    const char* default_url, int32_t default_url_length) {
  PP_DCHECK(!key_system_.empty());
  PostOnMain(callback_factory_.NewCallback(
      &CdmWrapper::KeyMessage,
      SessionInfo(key_system_,
                  std::string(session_id, session_id_length)),
      std::string(message, message_length),
      std::string(default_url, default_url_length)));
}

void CdmWrapper::SendKeyError(const char* session_id,
                              int32_t session_id_length,
                              cdm::MediaKeyError error_code,
                              uint32_t system_code) {
  SendKeyErrorInternal(key_system_,
                       std::string(session_id, session_id_length),
                       error_code,
                       system_code);
}

void CdmWrapper::SendUnknownKeyError(const std::string& key_system,
                                     const std::string& session_id) {
  SendKeyErrorInternal(key_system, session_id, cdm::kUnknownError, 0);
}


void CdmWrapper::SendKeyAdded(const std::string& key_system,
                              const std::string& session_id) {
  PostOnMain(callback_factory_.NewCallback(
      &CdmWrapper::KeyAdded,
      SessionInfo(key_system_, session_id)));
}

void CdmWrapper::SendKeyErrorInternal(const std::string& key_system,
                                      const std::string& session_id,
                                      cdm::MediaKeyError error_code,
                                      uint32_t system_code) {
  PP_DCHECK(!key_system.empty());
  PostOnMain(callback_factory_.NewCallback(&CdmWrapper::KeyError,
                                           SessionInfo(key_system_, session_id),
                                           error_code,
                                           system_code));
}

void CdmWrapper::KeyAdded(int32_t result, const SessionInfo& session_info) {
  PP_DCHECK(result == PP_OK);
  PP_DCHECK(!session_info.key_system.empty());
  pp::ContentDecryptor_Private::KeyAdded(session_info.key_system,
                                         session_info.session_id);
}

void CdmWrapper::KeyMessage(int32_t result,
                            const SessionInfo& session_info,
                            const std::string& message,
                            const std::string& default_url) {
  PP_DCHECK(result == PP_OK);
  PP_DCHECK(!session_info.key_system.empty());

  pp::VarArrayBuffer message_array_buffer(message.size());
  if (message.size() > 0) {
    memcpy(message_array_buffer.Map(), message.data(), message.size());
  }

  pp::ContentDecryptor_Private::KeyMessage(
      session_info.key_system, session_info.session_id,
      message_array_buffer, default_url);
}

void CdmWrapper::KeyError(int32_t result,
                          const SessionInfo& session_info,
                          cdm::MediaKeyError error_code,
                          uint32_t system_code) {
  PP_DCHECK(result == PP_OK);
  PP_DCHECK(!session_info.key_system.empty());
  pp::ContentDecryptor_Private::KeyError(
      session_info.key_system, session_info.session_id,
      error_code, system_code);
}

void CdmWrapper::DeliverBlock(int32_t result,
                              const cdm::Status& status,
                              const LinkedDecryptedBlock& decrypted_block,
                              const PP_DecryptTrackingInfo& tracking_info) {
  PP_DCHECK(result == PP_OK);
  PP_DecryptedBlockInfo decrypted_block_info;
  decrypted_block_info.tracking_info = tracking_info;
  decrypted_block_info.tracking_info.timestamp = decrypted_block->Timestamp();
  decrypted_block_info.tracking_info.buffer_id = 0;
  decrypted_block_info.data_size = 0;
  decrypted_block_info.result = CdmStatusToPpDecryptResult(status);

  pp::Buffer_Dev buffer;

  if (decrypted_block_info.result == PP_DECRYPTRESULT_SUCCESS) {
    PP_DCHECK(decrypted_block.get() && decrypted_block->DecryptedBuffer());
    if (!decrypted_block.get() || !decrypted_block->DecryptedBuffer()) {
      PP_NOTREACHED();
      decrypted_block_info.result = PP_DECRYPTRESULT_DECRYPT_ERROR;
    } else {
      PpbBuffer* ppb_buffer =
          static_cast<PpbBuffer*>(decrypted_block->DecryptedBuffer());
      buffer = ppb_buffer->buffer_dev();
      decrypted_block_info.tracking_info.buffer_id = ppb_buffer->buffer_id();
      decrypted_block_info.data_size = ppb_buffer->Size();
    }
  }

  pp::ContentDecryptor_Private::DeliverBlock(buffer, decrypted_block_info);
}

void CdmWrapper::DecoderInitializeDone(int32_t result,
                                       PP_DecryptorStreamType decoder_type,
                                       uint32_t request_id,
                                       bool success) {
  PP_DCHECK(result == PP_OK);
  pp::ContentDecryptor_Private::DecoderInitializeDone(decoder_type,
                                                      request_id,
                                                      success);
}

void CdmWrapper::DecoderDeinitializeDone(int32_t result,
                                         PP_DecryptorStreamType decoder_type,
                                         uint32_t request_id) {
  pp::ContentDecryptor_Private::DecoderDeinitializeDone(decoder_type,
                                                        request_id);
}

void CdmWrapper::DecoderResetDone(int32_t result,
                                  PP_DecryptorStreamType decoder_type,
                                  uint32_t request_id) {
  pp::ContentDecryptor_Private::DecoderResetDone(decoder_type, request_id);
}

void CdmWrapper::DeliverFrame(
    int32_t result,
    const cdm::Status& status,
    const LinkedVideoFrame& video_frame,
    const PP_DecryptTrackingInfo& tracking_info) {
  PP_DCHECK(result == PP_OK);
  PP_DecryptedFrameInfo decrypted_frame_info;
  decrypted_frame_info.tracking_info.request_id = tracking_info.request_id;
  decrypted_frame_info.tracking_info.buffer_id = 0;
  decrypted_frame_info.result = CdmStatusToPpDecryptResult(status);

  pp::Buffer_Dev buffer;

  if (decrypted_frame_info.result == PP_DECRYPTRESULT_SUCCESS) {
    if (!IsValidVideoFrame(video_frame)) {
      PP_NOTREACHED();
      decrypted_frame_info.result = PP_DECRYPTRESULT_DECODE_ERROR;
    } else {
      PpbBuffer* ppb_buffer =
          static_cast<PpbBuffer*>(video_frame->FrameBuffer());

      buffer = ppb_buffer->buffer_dev();

      decrypted_frame_info.tracking_info.timestamp = video_frame->Timestamp();
      decrypted_frame_info.tracking_info.buffer_id = ppb_buffer->buffer_id();
      decrypted_frame_info.format =
          CdmVideoFormatToPpDecryptedFrameFormat(video_frame->Format());
      decrypted_frame_info.width = video_frame->Size().width;
      decrypted_frame_info.height = video_frame->Size().height;
      decrypted_frame_info.plane_offsets[PP_DECRYPTEDFRAMEPLANES_Y] =
          video_frame->PlaneOffset(cdm::VideoFrame::kYPlane);
      decrypted_frame_info.plane_offsets[PP_DECRYPTEDFRAMEPLANES_U] =
          video_frame->PlaneOffset(cdm::VideoFrame::kUPlane);
      decrypted_frame_info.plane_offsets[PP_DECRYPTEDFRAMEPLANES_V] =
          video_frame->PlaneOffset(cdm::VideoFrame::kVPlane);
      decrypted_frame_info.strides[PP_DECRYPTEDFRAMEPLANES_Y] =
          video_frame->Stride(cdm::VideoFrame::kYPlane);
      decrypted_frame_info.strides[PP_DECRYPTEDFRAMEPLANES_U] =
          video_frame->Stride(cdm::VideoFrame::kUPlane);
      decrypted_frame_info.strides[PP_DECRYPTEDFRAMEPLANES_V] =
          video_frame->Stride(cdm::VideoFrame::kVPlane);
    }
  }
  pp::ContentDecryptor_Private::DeliverFrame(buffer, decrypted_frame_info);
}

void CdmWrapper::DeliverSamples(int32_t result,
                                const cdm::Status& status,
                                const LinkedAudioFrames& audio_frames,
                                const PP_DecryptTrackingInfo& tracking_info) {
  PP_DCHECK(result == PP_OK);

  PP_DecryptedBlockInfo decrypted_block_info;
  decrypted_block_info.tracking_info = tracking_info;
  decrypted_block_info.tracking_info.timestamp = 0;
  decrypted_block_info.tracking_info.buffer_id = 0;
  decrypted_block_info.data_size = 0;
  decrypted_block_info.result = CdmStatusToPpDecryptResult(status);

  pp::Buffer_Dev buffer;

  if (decrypted_block_info.result == PP_DECRYPTRESULT_SUCCESS) {
    PP_DCHECK(audio_frames.get() && audio_frames->FrameBuffer());
    if (!audio_frames.get() || !audio_frames->FrameBuffer()) {
      PP_NOTREACHED();
      decrypted_block_info.result = PP_DECRYPTRESULT_DECRYPT_ERROR;
    } else {
      PpbBuffer* ppb_buffer =
          static_cast<PpbBuffer*>(audio_frames->FrameBuffer());
      buffer = ppb_buffer->buffer_dev();
      decrypted_block_info.tracking_info.buffer_id = ppb_buffer->buffer_id();
      decrypted_block_info.data_size = ppb_buffer->Size();
    }
  }

  pp::ContentDecryptor_Private::DeliverSamples(buffer, decrypted_block_info);
}

bool CdmWrapper::IsValidVideoFrame(const LinkedVideoFrame& video_frame) {
  if (!video_frame.get() ||
      !video_frame->FrameBuffer() ||
      (video_frame->Format() != cdm::kI420 &&
       video_frame->Format() != cdm::kYv12)) {
    return false;
  }

  PpbBuffer* ppb_buffer = static_cast<PpbBuffer*>(video_frame->FrameBuffer());

  for (int i = 0; i < cdm::VideoFrame::kMaxPlanes; ++i) {
    int plane_height = (i == cdm::VideoFrame::kYPlane) ?
        video_frame->Size().height : (video_frame->Size().height + 1) / 2;
    cdm::VideoFrame::VideoPlane plane =
        static_cast<cdm::VideoFrame::VideoPlane>(i);
    if (ppb_buffer->Size() < video_frame->PlaneOffset(plane) +
                             plane_height * video_frame->Stride(plane)) {
      return false;
    }
  }

  return true;
}

// This object is the global object representing this plugin library as long
// as it is loaded.
class CdmWrapperModule : public pp::Module {
 public:
  CdmWrapperModule() : pp::Module() {
    // This function blocks the renderer thread (PluginInstance::Initialize()).
    // Move this call to other places if this may be a concern in the future.
    INITIALIZE_CDM_MODULE();
  }
  virtual ~CdmWrapperModule() {
    DeinitializeCdmModule();
  }

  virtual pp::Instance* CreateInstance(PP_Instance instance) {
    return new CdmWrapper(instance, this);
  }
};

}  // namespace webkit_media

namespace pp {

// Factory function for your specialization of the Module object.
Module* CreateModule() {
  return new webkit_media::CdmWrapperModule();
}

}  // namespace pp
