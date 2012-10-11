// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstring>
#include <string>
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
#include "webkit/media/crypto/ppapi/linked_ptr.h"
#include "webkit/media/crypto/ppapi/content_decryption_module.h"

namespace {

// This must be consistent with MediaKeyError defined in the spec:
// http://goo.gl/rbdnR
// TODO(xhwang): Add PP_MediaKeyError enum to avoid later static_cast in
// PluginInstance.
enum MediaKeyError {
  kUnknownError = 1,
  kClientError,
  kServiceError,
  kOutputError,
  kHardwareChangeError,
  kDomainError
};

bool IsMainThread() {
  return pp::Module::Get()->core()->IsMainThread();
}

void CallOnMain(pp::CompletionCallback cb) {
  // TODO(tomfinegan): This is only necessary because PPAPI doesn't allow calls
  // off the main thread yet. Remove this once the change lands.
  if (IsMainThread())
    cb.Run(PP_OK);
  else
    pp::Module::Get()->core()->CallOnMainThread(0, cb, PP_OK);
}

// Configures a cdm::InputBuffer. |subsamples| must exist as long as
// |input_buffer| is in use.
void ConfigureInputBuffer(
    const pp::Buffer_Dev& encrypted_buffer,
    const PP_EncryptedBlockInfo& encrypted_block_info,
    std::vector<cdm::SubsampleEntry>* subsamples,
    cdm::InputBuffer* input_buffer) {
  PP_DCHECK(subsamples);
  input_buffer->data = reinterpret_cast<uint8_t*>(encrypted_buffer.data());
  input_buffer->data_size = encrypted_buffer.size();
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

PP_DecryptedFrameFormat CdmVideoFormatToPpDecryptedFrameFormat(
    cdm::VideoFormat format) {
  if (format == cdm::kEmptyVideoFrame)
    return PP_DECRYPTEDFRAMEFORMAT_EMPTY;
  else if (format == cdm::kYv12)
    return PP_DECRYPTEDFRAMEFORMAT_YV12;
  else if (format == cdm::kI420)
    return PP_DECRYPTEDFRAMEFORMAT_I420;

  return PP_DECRYPTEDFRAMEFORMAT_UNKNOWN;
}

cdm::VideoDecoderConfig::VideoCodec PpVideoCodecToCdmVideoCodec(
    PP_VideoCodec codec) {
  if (codec == PP_VIDEOCODEC_VP8)
    return cdm::VideoDecoderConfig::kCodecVP8;

  return cdm::VideoDecoderConfig::kUnknownVideoCodec;
}

cdm::VideoDecoderConfig::VideoCodecProfile PpVCProfileToCdmVCProfile(
    PP_VideoCodecProfile profile) {
  if (profile == PP_VIDEOCODECPROFILE_VP8_MAIN)
    return cdm::VideoDecoderConfig::kVp8ProfileMain;

  return cdm::VideoDecoderConfig::kUnknownVideoCodecProfile;
}

cdm::VideoFormat PpDecryptedFrameFormatToCdmVideoFormat(
    PP_DecryptedFrameFormat format) {
  if (format == PP_DECRYPTEDFRAMEFORMAT_YV12)
    return cdm::kYv12;
  else if (format == PP_DECRYPTEDFRAMEFORMAT_I420)
    return cdm::kI420;
  else if (format == PP_DECRYPTEDFRAMEFORMAT_EMPTY)
    return cdm::kEmptyVideoFrame;

  return cdm::kUnknownVideoFormat;
}

}  // namespace

namespace webkit_media {

// Provides access to memory owned by a pp::Buffer_Dev created by
// PpbBufferAllocator::Allocate(). This class holds a reference to the
// Buffer_Dev throughout its lifetime.
class PpbBuffer : public cdm::Buffer {
 public:
  // cdm::Buffer methods.
  virtual void Destroy() OVERRIDE { delete this; }

  virtual uint8_t* data() OVERRIDE {
    return static_cast<uint8_t*>(buffer_.data());
  }

  virtual int32_t size() const OVERRIDE { return buffer_.size(); }

  pp::Buffer_Dev buffer_dev() const { return buffer_; }

 private:
  explicit PpbBuffer(pp::Buffer_Dev buffer) : buffer_(buffer) {}
  virtual ~PpbBuffer() {}

  pp::Buffer_Dev buffer_;

  friend class PpbBufferAllocator;

  DISALLOW_COPY_AND_ASSIGN(PpbBuffer);
};

class PpbBufferAllocator : public cdm::Allocator {
 public:
  explicit PpbBufferAllocator(pp::Instance* instance);
  virtual ~PpbBufferAllocator();

  // cdm::Allocator methods.
  // Allocates a pp::Buffer_Dev of the specified size and wraps it in a
  // PpbBuffer, which it returns. The caller own the returned buffer and must
  // free it by calling ReleaseBuffer(). Returns NULL on failure.
  virtual cdm::Buffer* Allocate(int32_t size) OVERRIDE;

 private:
  pp::Instance* const instance_;

  DISALLOW_COPY_AND_ASSIGN(PpbBufferAllocator);
};

class DecryptedBlockImpl : public cdm::DecryptedBlock {
 public:
  DecryptedBlockImpl() : buffer_(NULL), timestamp_(0) {}
  virtual ~DecryptedBlockImpl();

  virtual void set_buffer(cdm::Buffer* buffer) OVERRIDE;
  virtual cdm::Buffer* buffer() OVERRIDE;

  virtual void set_timestamp(int64_t timestamp) OVERRIDE;
  virtual int64_t timestamp() const OVERRIDE;

 private:
  PpbBuffer* buffer_;
  int64_t timestamp_;

  DISALLOW_COPY_AND_ASSIGN(DecryptedBlockImpl);
};

class KeyMessageImpl : public cdm::KeyMessage {
 public:
  KeyMessageImpl() : message_(NULL) {}
  virtual ~KeyMessageImpl();

  // cdm::KeyMessage methods.
  virtual void set_session_id(const char* session_id, int32_t length) OVERRIDE;
  virtual const char* session_id() const OVERRIDE;
  virtual int32_t session_id_length() const OVERRIDE;

  virtual void set_message(cdm::Buffer* message) OVERRIDE;
  virtual cdm::Buffer* message() OVERRIDE;

  virtual void set_default_url(const char* default_url,
                               int32_t length) OVERRIDE;
  virtual const char* default_url() const OVERRIDE;
  virtual int32_t default_url_length() const OVERRIDE;

  std::string session_id_string() const { return session_id_; }
  std::string default_url_string() const { return default_url_; }

 private:
  PpbBuffer* message_;
  std::string session_id_;
  std::string default_url_;

  DISALLOW_COPY_AND_ASSIGN(KeyMessageImpl);
};

DecryptedBlockImpl::~DecryptedBlockImpl() {
  if (buffer_)
    buffer_->Destroy();
}

void DecryptedBlockImpl::set_buffer(cdm::Buffer* buffer) {
  buffer_ = static_cast<PpbBuffer*>(buffer);
}

cdm::Buffer* DecryptedBlockImpl::buffer() {
  return buffer_;
}

void DecryptedBlockImpl::set_timestamp(int64_t timestamp) {
  timestamp_ = timestamp;
}

int64_t DecryptedBlockImpl::timestamp() const {
  return timestamp_;
}

class VideoFrameImpl : public cdm::VideoFrame {
 public:
  VideoFrameImpl();
  virtual ~VideoFrameImpl();

  virtual void set_format(cdm::VideoFormat format) OVERRIDE;
  virtual cdm::VideoFormat format() const OVERRIDE;

  virtual void set_frame_buffer(cdm::Buffer* frame_buffer) OVERRIDE;
  virtual cdm::Buffer* frame_buffer() OVERRIDE;

  virtual void set_plane_offset(cdm::VideoFrame::VideoPlane plane,
                                int32_t offset) OVERRIDE;
  virtual int32_t plane_offset(VideoPlane plane) OVERRIDE;

  virtual void set_stride(VideoPlane plane, int32_t stride) OVERRIDE;
  virtual int32_t stride(VideoPlane plane) OVERRIDE;

  virtual void set_timestamp(int64_t timestamp) OVERRIDE;
  virtual int64_t timestamp() const OVERRIDE;

 private:
  // The video buffer format.
  cdm::VideoFormat format_;

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
};

KeyMessageImpl::~KeyMessageImpl() {
  if (message_)
    message_->Destroy();
}

void KeyMessageImpl::set_session_id(const char* session_id, int32_t length) {
  session_id_.assign(session_id, length);
}

const char* KeyMessageImpl::session_id() const {
  return session_id_.c_str();
}

int32_t KeyMessageImpl::session_id_length() const {
  return session_id_.length();
}

void KeyMessageImpl::set_message(cdm::Buffer* buffer) {
  message_ = static_cast<PpbBuffer*>(buffer);
}

cdm::Buffer* KeyMessageImpl::message() {
  return message_;
}

void KeyMessageImpl::set_default_url(const char* default_url, int32_t length) {
  default_url_.assign(default_url, length);
}

const char* KeyMessageImpl::default_url() const {
  return default_url_.c_str();
}

int32_t KeyMessageImpl::default_url_length() const {
  return default_url_.length();
}

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

void VideoFrameImpl::set_format(cdm::VideoFormat format) {
  format_ = format;
}

cdm::VideoFormat VideoFrameImpl::format() const {
  return format_;
}

void VideoFrameImpl::set_frame_buffer(cdm::Buffer* frame_buffer) {
  frame_buffer_ = static_cast<PpbBuffer*>(frame_buffer);
}

cdm::Buffer* VideoFrameImpl::frame_buffer() {
  return frame_buffer_;
}

void VideoFrameImpl::set_plane_offset(cdm::VideoFrame::VideoPlane plane,
                                      int32_t offset) {
  PP_DCHECK(0 < plane && plane < kMaxPlanes);
  PP_DCHECK(offset >= 0);
  plane_offsets_[plane] = offset;
}

int32_t VideoFrameImpl::plane_offset(VideoPlane plane) {
  PP_DCHECK(0 < plane && plane < kMaxPlanes);
  return plane_offsets_[plane];
}

void VideoFrameImpl::set_stride(VideoPlane plane, int32_t stride) {
  PP_DCHECK(0 < plane && plane < kMaxPlanes);
  strides_[plane] = stride;
}

int32_t VideoFrameImpl::stride(VideoPlane plane) {
  PP_DCHECK(0 < plane && plane < kMaxPlanes);
  return strides_[plane];
}

void VideoFrameImpl::set_timestamp(int64_t timestamp) {
  timestamp_ = timestamp;
}

int64_t VideoFrameImpl::timestamp() const {
  return timestamp_;
}

// A wrapper class for abstracting away PPAPI interaction and threading for a
// Content Decryption Module (CDM).
class CdmWrapper : public pp::Instance,
                   public pp::ContentDecryptor_Private,
                   public cdm::CdmHost {
 public:
  CdmWrapper(PP_Instance instance, pp::Module* module);
  virtual ~CdmWrapper();
  virtual bool Init(uint32_t argc, const char* argn[], const char* argv[]) {
    return true;
  }

  // PPP_ContentDecryptor_Private methods
  // Note: Results of calls to these methods must be reported through the
  // PPB_ContentDecryptor_Private interface.
  virtual void GenerateKeyRequest(const std::string& key_system,
                                  pp::VarArrayBuffer init_data) OVERRIDE;
  virtual void AddKey(const std::string& session_id,
                      pp::VarArrayBuffer key,
                      pp::VarArrayBuffer init_data) OVERRIDE;
  virtual void CancelKeyRequest(const std::string& session_id) OVERRIDE;
  virtual void Decrypt(
      pp::Buffer_Dev encrypted_buffer,
      const PP_EncryptedBlockInfo& encrypted_block_info) OVERRIDE;
  virtual void InitializeVideoDecoder(
      const PP_VideoDecoderConfig& decoder_config,
      pp::Buffer_Dev extra_data_buffer) OVERRIDE;
  virtual void DecryptAndDecodeFrame(
      pp::Buffer_Dev encrypted_frame,
      const PP_EncryptedVideoFrameInfo& encrypted_video_frame_info) OVERRIDE;

  // CdmHost methods.
  virtual void SetTimer(int64 delay_ms) OVERRIDE;
  virtual double GetCurrentWallTimeMs() OVERRIDE;

 private:
  typedef linked_ptr<DecryptedBlockImpl> LinkedDecryptedBlock;
  typedef linked_ptr<KeyMessageImpl> LinkedKeyMessage;
  typedef linked_ptr<VideoFrameImpl> LinkedVideoFrame;

  // <code>PPB_ContentDecryptor_Private</code> dispatchers. These are passed to
  // <code>callback_factory_</code> to ensure that calls into
  // <code>PPP_ContentDecryptor_Private</code> are asynchronous.
  void KeyAdded(int32_t result, const std::string& session_id);
  void KeyMessage(int32_t result, const LinkedKeyMessage& message);
  void KeyError(int32_t result, const std::string& session_id);
  void DeliverBlock(int32_t result,
                    const cdm::Status& status,
                    const LinkedDecryptedBlock& decrypted_block,
                    const PP_DecryptTrackingInfo& tracking_info);
  void DecoderInitialized(int32_t result,
                          bool success,
                          uint32_t request_id);
  void DeliverFrame(int32_t result,
                    const cdm::Status& status,
                    const LinkedVideoFrame& video_frame,
                    const PP_DecryptTrackingInfo& tracking_info);

  // Helper for SetTimer().
  void TimerExpired(int32 result);

  PpbBufferAllocator allocator_;
  pp::CompletionCallbackFactory<CdmWrapper> callback_factory_;
  cdm::ContentDecryptionModule* cdm_;
  std::string key_system_;
};

PpbBufferAllocator::PpbBufferAllocator(pp::Instance* instance)
    : instance_(instance) {
}

PpbBufferAllocator::~PpbBufferAllocator() {
}

cdm::Buffer* PpbBufferAllocator::Allocate(int32_t size) {
  PP_DCHECK(size > 0);

  pp::Buffer_Dev buffer(instance_, size);
  if (buffer.is_null())
    return NULL;

  return new PpbBuffer(buffer);
}

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
                                    pp::VarArrayBuffer init_data) {
  PP_DCHECK(!key_system.empty());

  if (!cdm_) {
    cdm_ = CreateCdmInstance(&allocator_, this);
    if (!cdm_)
      return;
  }

  LinkedKeyMessage key_request(new KeyMessageImpl());
  cdm::Status status = cdm_->GenerateKeyRequest(
      reinterpret_cast<const uint8_t*>(init_data.Map()),
      init_data.ByteLength(),
      key_request.get());
  PP_DCHECK(status == cdm::kSuccess || status == cdm::kSessionError);
  if (status != cdm::kSuccess ||
      !key_request->message() ||
      key_request->message()->size() == 0) {
    CallOnMain(callback_factory_.NewCallback(&CdmWrapper::KeyError,
                                             std::string()));
    return;
  }

  // TODO(xhwang): Remove unnecessary CallOnMain calls here and below once we
  // only support out-of-process.
  // If running out-of-process, PPB calls will always behave asynchronously
  // since IPC is involved. In that case, if we are already on main thread,
  // we don't need to use CallOnMain to help us call PPB call on main thread,
  // or to help call PPB asynchronously.
  key_system_ = key_system;
  CallOnMain(callback_factory_.NewCallback(&CdmWrapper::KeyMessage,
                                           key_request));
}

void CdmWrapper::AddKey(const std::string& session_id,
                        pp::VarArrayBuffer key,
                        pp::VarArrayBuffer init_data) {
  const uint8_t* key_ptr = reinterpret_cast<const uint8_t*>(key.Map());
  int key_size = key.ByteLength();
  const uint8_t* init_data_ptr =
      reinterpret_cast<const uint8_t*>(init_data.Map());
  int init_data_size = init_data.ByteLength();

  if (!key_ptr || key_size <= 0 || !init_data_ptr || init_data_size <= 0)
    return;

  PP_DCHECK(cdm_);
  cdm::Status status = cdm_->AddKey(session_id.data(), session_id.size(),
                                    key_ptr, key_size,
                                    init_data_ptr, init_data_size);
  PP_DCHECK(status == cdm::kSuccess || status == cdm::kSessionError);
  if (status != cdm::kSuccess) {
    CallOnMain(callback_factory_.NewCallback(&CdmWrapper::KeyError,
                                             session_id));
    return;
  }

  CallOnMain(callback_factory_.NewCallback(&CdmWrapper::KeyAdded, session_id));
}

void CdmWrapper::CancelKeyRequest(const std::string& session_id) {
  PP_DCHECK(cdm_);
  cdm::Status status = cdm_->CancelKeyRequest(session_id.data(),
                                              session_id.size());
  PP_DCHECK(status == cdm::kSuccess || status == cdm::kSessionError);
  if (status != cdm::kSuccess) {
    CallOnMain(callback_factory_.NewCallback(&CdmWrapper::KeyError,
                                             session_id));
  }
}

void CdmWrapper::Decrypt(pp::Buffer_Dev encrypted_buffer,
                         const PP_EncryptedBlockInfo& encrypted_block_info) {
  PP_DCHECK(!encrypted_buffer.is_null());
  PP_DCHECK(cdm_);

  cdm::InputBuffer input_buffer;
  std::vector<cdm::SubsampleEntry> subsamples;
  ConfigureInputBuffer(encrypted_buffer, encrypted_block_info, &subsamples,
                       &input_buffer);

  LinkedDecryptedBlock decrypted_block(new DecryptedBlockImpl());
  cdm::Status status = cdm_->Decrypt(input_buffer, decrypted_block.get());

  CallOnMain(callback_factory_.NewCallback(
      &CdmWrapper::DeliverBlock,
      status,
      decrypted_block,
      encrypted_block_info.tracking_info));
}

void CdmWrapper::InitializeVideoDecoder(
    const PP_VideoDecoderConfig& decoder_config,
    pp::Buffer_Dev extra_data_buffer) {
  PP_DCHECK(cdm_);
  cdm::VideoDecoderConfig cdm_decoder_config;
  cdm_decoder_config.codec = PpVideoCodecToCdmVideoCodec(decoder_config.codec);
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
  cdm::Status status = cdm_->InitializeVideoDecoder(cdm_decoder_config);

  CallOnMain(callback_factory_.NewCallback(
      &CdmWrapper::DecoderInitialized,
      status == cdm::kSuccess,
      decoder_config.request_id));

}

void CdmWrapper::DecryptAndDecodeFrame(
    pp::Buffer_Dev encrypted_frame,
    const PP_EncryptedVideoFrameInfo& encrypted_video_frame_info) {
  PP_DCHECK(!encrypted_frame.is_null());
  PP_DCHECK(cdm_);

  cdm::InputBuffer input_buffer;
  std::vector<cdm::SubsampleEntry> subsamples;
  ConfigureInputBuffer(encrypted_frame,
                       encrypted_video_frame_info.encryption_info,
                       &subsamples,
                       &input_buffer);

  LinkedVideoFrame video_frame(new VideoFrameImpl());
  cdm::Status status = cdm_->DecryptAndDecodeFrame(input_buffer,
                                                   video_frame.get());
  CallOnMain(callback_factory_.NewCallback(
      &CdmWrapper::DeliverFrame,
      status,
      video_frame,
      encrypted_video_frame_info.encryption_info.tracking_info));
}

void CdmWrapper::SetTimer(int64 delay_ms) {
  // NOTE: doesn't really need to run on the main thread; could just as well run
  // on a helper thread if |cdm_| were thread-friendly and care was taken.  We
  // only use CallOnMainThread() here to get delayed-execution behavior.
  pp::Module::Get()->core()->CallOnMainThread(
      delay_ms,
      callback_factory_.NewCallback(&CdmWrapper::TimerExpired),
      PP_OK);
}

void CdmWrapper::TimerExpired(int32 result) {
  PP_DCHECK(result == PP_OK);
  bool populated;
  LinkedKeyMessage key_message(new KeyMessageImpl());
  cdm_->TimerExpired(key_message.get(), &populated);
  if (!populated)
    return;
  CallOnMain(callback_factory_.NewCallback(&CdmWrapper::KeyMessage,
                                           key_message));
}

double CdmWrapper::GetCurrentWallTimeMs() {
  // TODO(fischman): figure out whether this requires an IPC round-trip per
  // call, and if that's a problem for the frequency of calls.  If it is,
  // optimize by proactively sending wall-time across the IPC boundary on some
  // existing calls, or add a periodic task to update a plugin-side clock.
  return pp::Module::Get()->core()->GetTime();
}

void CdmWrapper::KeyAdded(int32_t result, const std::string& session_id) {
  PP_DCHECK(result == PP_OK);
  pp::ContentDecryptor_Private::KeyAdded(key_system_, session_id);
}

void CdmWrapper::KeyMessage(int32_t result,
                            const LinkedKeyMessage& key_message) {
  PP_DCHECK(result == PP_OK);
  pp::Buffer_Dev message_buffer =
      static_cast<const PpbBuffer*>(key_message->message())->buffer_dev();
  pp::ContentDecryptor_Private::KeyMessage(
      key_system_,
      key_message->session_id_string(),
      message_buffer,
      key_message->default_url_string());
}

// TODO(xhwang): Support MediaKeyError (see spec: http://goo.gl/rbdnR) in CDM
// interface and in this function.
void CdmWrapper::KeyError(int32_t result, const std::string& session_id) {
  PP_DCHECK(result == PP_OK);
  pp::ContentDecryptor_Private::KeyError(key_system_,
                                         session_id,
                                         kUnknownError,
                                         0);
}

void CdmWrapper::DeliverBlock(int32_t result,
                              const cdm::Status& status,
                              const LinkedDecryptedBlock& decrypted_block,
                              const PP_DecryptTrackingInfo& tracking_info) {
  PP_DCHECK(result == PP_OK);
  PP_DecryptedBlockInfo decrypted_block_info;
  decrypted_block_info.tracking_info = tracking_info;
  decrypted_block_info.tracking_info.timestamp = decrypted_block->timestamp();

  switch (status) {
    case cdm::kSuccess:
      decrypted_block_info.result = PP_DECRYPTRESULT_SUCCESS;
      PP_DCHECK(decrypted_block.get() && decrypted_block->buffer());
      break;
    case cdm::kNoKey:
      decrypted_block_info.result = PP_DECRYPTRESULT_DECRYPT_NOKEY;
      break;
    case cdm::kDecryptError:
      decrypted_block_info.result = PP_DECRYPTRESULT_DECRYPT_ERROR;
      break;
    default:
      PP_DCHECK(false);
      decrypted_block_info.result = PP_DECRYPTRESULT_DECRYPT_ERROR;
  }

  const pp::Buffer_Dev& buffer =
      decrypted_block.get() && decrypted_block->buffer() ?
      static_cast<PpbBuffer*>(decrypted_block->buffer())->buffer_dev() :
          pp::Buffer_Dev();

  pp::ContentDecryptor_Private::DeliverBlock(buffer, decrypted_block_info);
}

void CdmWrapper::DecoderInitialized(int32_t result,
                                    bool success,
                                    uint32_t request_id) {
  pp::ContentDecryptor_Private::DecoderInitialized(success, request_id);
}

void CdmWrapper::DeliverFrame(
    int32_t result,
    const cdm::Status& status,
    const LinkedVideoFrame& video_frame,
    const PP_DecryptTrackingInfo& tracking_info) {
  PP_DCHECK(result == PP_OK);
  PP_DecryptedFrameInfo decrypted_frame_info;
  decrypted_frame_info.tracking_info = tracking_info;

  switch (status) {
    case cdm::kSuccess:
      PP_DCHECK(video_frame->format() == cdm::kI420 ||
                video_frame->format() == cdm::kYv12);
      PP_DCHECK(video_frame.get() && video_frame->frame_buffer());
      decrypted_frame_info.result = PP_DECRYPTRESULT_SUCCESS;
      decrypted_frame_info.format =
          CdmVideoFormatToPpDecryptedFrameFormat(video_frame->format());
      decrypted_frame_info.plane_offsets[PP_DECRYPTEDFRAMEPLANES_Y] =
        video_frame->plane_offset(cdm::VideoFrame::kYPlane);
      decrypted_frame_info.plane_offsets[PP_DECRYPTEDFRAMEPLANES_U] =
        video_frame->plane_offset(cdm::VideoFrame::kUPlane);
      decrypted_frame_info.plane_offsets[PP_DECRYPTEDFRAMEPLANES_V] =
        video_frame->plane_offset(cdm::VideoFrame::kVPlane);
      decrypted_frame_info.strides[PP_DECRYPTEDFRAMEPLANES_Y] =
        video_frame->stride(cdm::VideoFrame::kYPlane);
      decrypted_frame_info.strides[PP_DECRYPTEDFRAMEPLANES_U] =
        video_frame->stride(cdm::VideoFrame::kUPlane);
      decrypted_frame_info.strides[PP_DECRYPTEDFRAMEPLANES_V] =
        video_frame->stride(cdm::VideoFrame::kVPlane);
      break;
    case cdm::kNoKey:
      decrypted_frame_info.result = PP_DECRYPTRESULT_DECRYPT_NOKEY;
      break;
    case cdm::kDecryptError:
      decrypted_frame_info.result = PP_DECRYPTRESULT_DECRYPT_ERROR;
      break;
    case cdm::kDecodeError:
      decrypted_frame_info.result = PP_DECRYPTRESULT_DECODE_ERROR;
      break;
    case cdm::kSessionError:
    default:
      PP_DCHECK(false);
      decrypted_frame_info.result = PP_DECRYPTRESULT_DECRYPT_ERROR;
  }

  const pp::Buffer_Dev& buffer =
      video_frame.get() && video_frame->frame_buffer() ?
          static_cast<PpbBuffer*>(video_frame->frame_buffer())->buffer_dev() :
          pp::Buffer_Dev();

  pp::ContentDecryptor_Private::DeliverFrame(buffer, decrypted_frame_info);
}


// This object is the global object representing this plugin library as long
// as it is loaded.
class CdmWrapperModule : public pp::Module {
 public:
  CdmWrapperModule() : pp::Module() {}
  virtual ~CdmWrapperModule() {}

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
