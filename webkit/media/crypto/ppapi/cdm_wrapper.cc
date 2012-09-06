// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstring>  // For memcpy.
#include <vector>

#include "base/compiler_specific.h"  // For OVERRIDE.
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
  virtual bool GenerateKeyRequest(const std::string& key_system,
                                  pp::VarArrayBuffer init_data) OVERRIDE;
  virtual bool AddKey(const std::string& session_id,
                      pp::VarArrayBuffer key,
                      pp::VarArrayBuffer init_data) OVERRIDE;
  virtual bool CancelKeyRequest(const std::string& session_id) OVERRIDE;
  virtual bool Decrypt(
      pp::Buffer_Dev encrypted_buffer,
      const PP_EncryptedBlockInfo& encrypted_block_info) OVERRIDE;
  virtual bool DecryptAndDecode(
      pp::Buffer_Dev encrypted_buffer,
      const PP_EncryptedBlockInfo& encrypted_block_info) OVERRIDE;

 private:
  // Creates a PP_Resource containing a PPB_Buffer_Impl, copies |data| into the
  // buffer resource, and returns it. Returns a an invalid PP_Resource with an
  // ID of 0 on failure. Upon success, the returned Buffer resource has a
  // reference count of 1.
  pp::Buffer_Dev MakeBufferResource(const uint8_t* data, uint32_t data_size);

  // <code>PPB_ContentDecryptor_Private</code> dispatchers. These are passed to
  // <code>callback_factory_</code> to ensure that calls into
  // <code>PPP_ContentDecryptor_Private</code> are asynchronous.
  void KeyAdded(int32_t result, const std::string& session_id);
  void KeyMessage(int32_t result, cdm::KeyMessage& key_message);
  void KeyError(int32_t result, const std::string& session_id);
  void DeliverBlock(int32_t result,
                    const cdm::Status& status,
                    cdm::OutputBuffer& output_buffer,
                    const PP_DecryptTrackingInfo& tracking_info);

  pp::CompletionCallbackFactory<CdmWrapper> callback_factory_;
  cdm::ContentDecryptionModule* cdm_;
  std::string key_system_;
};

CdmWrapper::CdmWrapper(PP_Instance instance, pp::Module* module)
    : pp::Instance(instance),
      pp::ContentDecryptor_Private(this),
      cdm_(NULL) {
  callback_factory_.Initialize(this);
}

CdmWrapper::~CdmWrapper() {
  if (cdm_)
    DestroyCdmInstance(cdm_);
}

bool CdmWrapper::GenerateKeyRequest(const std::string& key_system,
                                    pp::VarArrayBuffer init_data) {
  PP_DCHECK(!key_system.empty());

  if (!cdm_) {
    cdm_ = CreateCdmInstance();
    if (!cdm_)
      return false;
  }

  cdm::KeyMessage key_request;
  cdm::Status status = cdm_->GenerateKeyRequest(
      reinterpret_cast<const uint8_t*>(init_data.Map()),
      init_data.ByteLength(),
      &key_request);

  if (status != cdm::kSuccess ||
      !key_request.message ||
      key_request.message_size == 0) {
    CallOnMain(callback_factory_.NewCallback(&CdmWrapper::KeyError,
                                             std::string()));
    return true;
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

  return true;
}

bool CdmWrapper::AddKey(const std::string& session_id,
                        pp::VarArrayBuffer key,
                        pp::VarArrayBuffer init_data) {
  const uint8_t* key_ptr = reinterpret_cast<const uint8_t*>(key.Map());
  int key_size = key.ByteLength();
  const uint8_t* init_data_ptr =
      reinterpret_cast<const uint8_t*>(init_data.Map());
  int init_data_size = init_data.ByteLength();

  if (!key_ptr || key_size <= 0 || !init_data_ptr || init_data_size <= 0)
    return false;

  PP_DCHECK(cdm_);
  cdm::Status status = cdm_->AddKey(session_id.data(), session_id.size(),
                                    key_ptr, key_size,
                                    init_data_ptr, init_data_size);

  if (status != cdm::kSuccess) {
    CallOnMain(callback_factory_.NewCallback(&CdmWrapper::KeyError,
                                             session_id));
    return true;
  }

  CallOnMain(callback_factory_.NewCallback(&CdmWrapper::KeyAdded, session_id));
  return true;
}

bool CdmWrapper::CancelKeyRequest(const std::string& session_id) {
  PP_DCHECK(cdm_);

  cdm::Status status = cdm_->CancelKeyRequest(session_id.data(),
                                              session_id.size());

  if (status != cdm::kSuccess) {
    CallOnMain(callback_factory_.NewCallback(&CdmWrapper::KeyError,
                                             session_id));
    return true;
  }

  return true;
}

bool CdmWrapper::Decrypt(pp::Buffer_Dev encrypted_buffer,
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
  input_buffer.checksum = encrypted_block_info.checksum;
  input_buffer.checksum_size = encrypted_block_info.checksum_size;
  input_buffer.num_subsamples = encrypted_block_info.num_subsamples;
  std::vector<cdm::SubsampleEntry> subsamples;
  for (uint32_t i = 0; i < encrypted_block_info.num_subsamples; ++i) {
    subsamples.push_back(cdm::SubsampleEntry(
        encrypted_block_info.subsamples[i].clear_bytes,
        encrypted_block_info.subsamples[i].cipher_bytes));
  }
  input_buffer.subsamples = &subsamples[0];
  input_buffer.timestamp = encrypted_block_info.tracking_info.timestamp;

  cdm::OutputBuffer output_buffer;
  cdm::Status status = cdm_->Decrypt(input_buffer, &output_buffer);

  CallOnMain(callback_factory_.NewCallback(
      &CdmWrapper::DeliverBlock,
      status,
      output_buffer,
      encrypted_block_info.tracking_info));
  return true;
}

bool CdmWrapper::DecryptAndDecode(
    pp::Buffer_Dev encrypted_buffer,
    const PP_EncryptedBlockInfo& encrypted_block_info) {
  return false;
}

pp::Buffer_Dev CdmWrapper::MakeBufferResource(const uint8_t* data,
                                              uint32_t data_size) {
  if (!data || !data_size)
    return pp::Buffer_Dev();

  pp::Buffer_Dev buffer(this, data_size);
  if (!buffer.data())
    return pp::Buffer_Dev();

  memcpy(buffer.data(), data, data_size);
  return buffer;
}

void CdmWrapper::KeyAdded(int32_t result, const std::string& session_id) {
  pp::ContentDecryptor_Private::KeyAdded(key_system_, session_id);
}

void CdmWrapper::KeyMessage(int32_t result,
                            cdm::KeyMessage& key_message) {
  pp::Buffer_Dev message_buffer(MakeBufferResource(key_message.message,
                                                   key_message.message_size));
  pp::ContentDecryptor_Private::KeyMessage(
      key_system_,
      std::string(key_message.session_id, key_message.session_id_size),
      message_buffer,
      std::string(key_message.default_url, key_message.default_url_size));

  // TODO(xhwang): Fix this. This is not always safe as the memory is allocated
  // in another shared object.
  delete [] key_message.session_id;
  key_message.session_id = NULL;
  delete [] key_message.message;
  key_message.message = NULL;
  delete [] key_message.default_url;
  key_message.default_url = NULL;
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
                              cdm::OutputBuffer& output_buffer,
                              const PP_DecryptTrackingInfo& tracking_info) {
  pp::Buffer_Dev decrypted_buffer(MakeBufferResource(output_buffer.data,
                                                     output_buffer.data_size));
  PP_DecryptedBlockInfo decrypted_block_info;
  decrypted_block_info.tracking_info.request_id = tracking_info.request_id;
  decrypted_block_info.tracking_info.timestamp = output_buffer.timestamp;

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

  pp::ContentDecryptor_Private::DeliverBlock(decrypted_buffer,
                                             decrypted_block_info);

  // TODO(xhwang): Fix this. This is not always safe as the memory is allocated
  // in another shared object.
  delete [] output_buffer.data;
  output_buffer.data = NULL;
}

// This object is the global object representing this plugin library as long
// as it is loaded.
class MyModule : public pp::Module {
 public:
  MyModule() : pp::Module() {}
  virtual ~MyModule() {}

  virtual pp::Instance* CreateInstance(PP_Instance instance) {
    return new CdmWrapper(instance, this);
  }
};

}  // namespace webkit_media

namespace pp {

// Factory function for your specialization of the Module object.
Module* CreateModule() {
  return new webkit_media::MyModule();
}

}  // namespace pp
