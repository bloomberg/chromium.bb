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

}  // namespace

namespace webkit_media {

// Provides access to memory owned by a pp::Buffer_Dev created by
// PpbBufferAllocator::Allocate(). This class holds a reference to the
// Buffer_Dev throughout its lifetime.
class PpbBuffer : public cdm::Buffer {
 public:
  // cdm::Buffer methods.
  virtual void Destroy() OVERRIDE { delete this; }

  virtual uint8_t* buffer() OVERRIDE {
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

class KeyMessageImpl : public cdm::KeyMessage {
 public:
  KeyMessageImpl() : message_(NULL) {}
  virtual ~KeyMessageImpl();

  // cdm::KeyMessage methods.
  virtual void set_session_id(const char* session_id, int32_t length) OVERRIDE;
  virtual const char* session_id() const OVERRIDE;
  virtual int32_t session_id_length() const OVERRIDE;

  virtual void set_message(cdm::Buffer* message) OVERRIDE;
  virtual cdm::Buffer* message() const OVERRIDE;

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

class OutputBufferImpl : public cdm::OutputBuffer {
 public:
  OutputBufferImpl() : buffer_(NULL), timestamp_(0) {}
  virtual ~OutputBufferImpl();

  virtual void set_buffer(cdm::Buffer* buffer) OVERRIDE;
  virtual cdm::Buffer* buffer() const OVERRIDE;

  virtual void set_timestamp(int64_t timestamp) OVERRIDE;
  virtual int64_t timestamp() const OVERRIDE;

 private:
  PpbBuffer* buffer_;
  int64_t timestamp_;

  DISALLOW_COPY_AND_ASSIGN(OutputBufferImpl);
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

cdm::Buffer* KeyMessageImpl::message() const {
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

OutputBufferImpl::~OutputBufferImpl() {
  if (buffer_)
    buffer_->Destroy();
}

void OutputBufferImpl::set_buffer(cdm::Buffer* buffer) {
  buffer_ = static_cast<PpbBuffer*>(buffer);
}

cdm::Buffer* OutputBufferImpl::buffer() const {
  return buffer_;
}

void OutputBufferImpl::set_timestamp(int64_t timestamp) {
  timestamp_ = timestamp;
}

int64_t OutputBufferImpl::timestamp() const {
  return timestamp_;
}

// A wrapper class for abstracting away PPAPI interaction and threading for a
// Content Decryption Module (CDM).
class CdmWrapper : public pp::Instance,
                   public pp::ContentDecryptor_Private {
 public:
  CdmWrapper(PP_Instance instance, pp::Module* module);
  virtual ~CdmWrapper();
  virtual bool Init(uint32_t argc, const char* argn[], const char* argv[]) {
    return true;
  }

  // PPP_ContentDecryptor_Private methods
  // Note: As per comments in PPP_ContentDecryptor_Private, these calls should
  // return false if the call was not forwarded to the CDM and should return
  // true otherwise. Once the call reaches the CDM, the call result/status
  // should be reported through the PPB_ContentDecryptor_Private interface.
  virtual void GenerateKeyRequest(const std::string& key_system,
                                  pp::VarArrayBuffer init_data) OVERRIDE;
  virtual void AddKey(const std::string& session_id,
                      pp::VarArrayBuffer key,
                      pp::VarArrayBuffer init_data) OVERRIDE;
  virtual void CancelKeyRequest(const std::string& session_id) OVERRIDE;
  virtual void Decrypt(
      pp::Buffer_Dev encrypted_buffer,
      const PP_EncryptedBlockInfo& encrypted_block_info) OVERRIDE;
  virtual void DecryptAndDecode(
      pp::Buffer_Dev encrypted_buffer,
      const PP_EncryptedBlockInfo& encrypted_block_info) OVERRIDE;

 private:
  typedef linked_ptr<KeyMessageImpl> LinkedKeyMessage;
  typedef linked_ptr<OutputBufferImpl> LinkedOutputBuffer;

  // <code>PPB_ContentDecryptor_Private</code> dispatchers. These are passed to
  // <code>callback_factory_</code> to ensure that calls into
  // <code>PPP_ContentDecryptor_Private</code> are asynchronous.
  void KeyAdded(int32_t result, const std::string& session_id);
  void KeyMessage(int32_t result, const LinkedKeyMessage& message);
  void KeyError(int32_t result, const std::string& session_id);
  void DeliverBlock(int32_t result,
                    const cdm::Status& status,
                    const LinkedOutputBuffer& output_buffer,
                    const PP_DecryptTrackingInfo& tracking_info);

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
    cdm_ = CreateCdmInstance(&allocator_);
    if (!cdm_)
      return;
  }

  LinkedKeyMessage key_request(new KeyMessageImpl());
  cdm::Status status = cdm_->GenerateKeyRequest(
      reinterpret_cast<const uint8_t*>(init_data.Map()),
      init_data.ByteLength(),
      key_request.get());

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
  if (status != cdm::kSuccess) {
    CallOnMain(callback_factory_.NewCallback(&CdmWrapper::KeyError,
                                             session_id));
  }
}

void CdmWrapper::Decrypt(pp::Buffer_Dev encrypted_buffer,
                         const PP_EncryptedBlockInfo& encrypted_block_info) {
  PP_DCHECK(!encrypted_buffer.is_null());
  PP_DCHECK(cdm_);

  // TODO(xhwang): Simplify the following data conversion.
  cdm::InputBuffer input_buffer;
  input_buffer.data = reinterpret_cast<uint8_t*>(encrypted_buffer.data());
  input_buffer.data_size = encrypted_buffer.size();
  input_buffer.data_offset = encrypted_block_info.data_offset;
  input_buffer.key_id = encrypted_block_info.key_id;
  input_buffer.key_id_size = encrypted_block_info.key_id_size;
  input_buffer.iv = encrypted_block_info.iv;
  input_buffer.iv_size = encrypted_block_info.iv_size;
  input_buffer.num_subsamples = encrypted_block_info.num_subsamples;

  std::vector<cdm::SubsampleEntry> subsamples;
  if (encrypted_block_info.num_subsamples > 0) {
    subsamples.reserve(encrypted_block_info.num_subsamples);

    for (uint32_t i = 0; i < encrypted_block_info.num_subsamples; ++i) {
      subsamples.push_back(cdm::SubsampleEntry(
          encrypted_block_info.subsamples[i].clear_bytes,
          encrypted_block_info.subsamples[i].cipher_bytes));
    }

    input_buffer.subsamples = &subsamples[0];
  }

  input_buffer.timestamp = encrypted_block_info.tracking_info.timestamp;

  LinkedOutputBuffer output_buffer(new OutputBufferImpl());
  cdm::Status status = cdm_->Decrypt(input_buffer, output_buffer.get());

  if (status != cdm::kSuccess || !output_buffer->buffer()) {
    CallOnMain(callback_factory_.NewCallback(&CdmWrapper::KeyError,
                                             std::string()));
    return;
  }

  PP_DCHECK(output_buffer->buffer());
  CallOnMain(callback_factory_.NewCallback(
      &CdmWrapper::DeliverBlock,
      status,
      output_buffer,
      encrypted_block_info.tracking_info));
}

void CdmWrapper::DecryptAndDecode(
    pp::Buffer_Dev encrypted_buffer,
    const PP_EncryptedBlockInfo& encrypted_block_info) {
}

void CdmWrapper::KeyAdded(int32_t result, const std::string& session_id) {
  pp::ContentDecryptor_Private::KeyAdded(key_system_, session_id);
}

void CdmWrapper::KeyMessage(int32_t result,
                            const LinkedKeyMessage& key_message) {
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
  pp::ContentDecryptor_Private::KeyError(key_system_,
                                         session_id,
                                         kUnknownError,
                                         0);
}

void CdmWrapper::DeliverBlock(int32_t result,
                              const cdm::Status& status,
                              const LinkedOutputBuffer& output_buffer,
                              const PP_DecryptTrackingInfo& tracking_info) {
  PP_DecryptedBlockInfo decrypted_block_info;
  decrypted_block_info.tracking_info.request_id = tracking_info.request_id;
  decrypted_block_info.tracking_info.timestamp = output_buffer->timestamp();

  switch (status) {
    case cdm::kSuccess:
      decrypted_block_info.result = PP_DECRYPTRESULT_SUCCESS;
      break;
    case cdm::kNoKey:
      decrypted_block_info.result = PP_DECRYPTRESULT_DECRYPT_NOKEY;
      break;
    default:
      decrypted_block_info.result = PP_DECRYPTRESULT_DECRYPT_ERROR;
  }

  pp::ContentDecryptor_Private::DeliverBlock(
      static_cast<PpbBuffer*>(output_buffer->buffer())->buffer_dev(),
      decrypted_block_info);
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
