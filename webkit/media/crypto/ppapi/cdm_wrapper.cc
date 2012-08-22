// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstring>  // For std::memcpy.

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

namespace {

struct DecryptorMessage {
  DecryptorMessage() : media_error(0), system_code(0) {}
  std::string key_system;
  std::string session_id;
  std::string default_url;
  std::string message_data;
  int32_t media_error;
  int32_t system_code;
};

struct DecryptedBlock {
  DecryptedBlock() {
    std::memset(reinterpret_cast<void*>(&decrypted_block_info),
                0,
                sizeof(decrypted_block_info));
  }
  std::string decrypted_data;
  PP_DecryptedBlockInfo decrypted_block_info;
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


// A wrapper class for abstracting away PPAPI interaction and threading for a
// Content Decryption Module (CDM).
class CDMWrapper : public pp::Instance,
                   public pp::ContentDecryptor_Private {
 public:
  CDMWrapper(PP_Instance instance, pp::Module* module);
  virtual ~CDMWrapper() {}

  // PPP_ContentDecryptor_Private methods
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
      const PP_EncryptedBlockInfo& encrypted_block_info) OVERRIDE {
    return false;
  }

  virtual bool Init(uint32_t argc, const char* argn[], const char* argv[]) {
    return true;
  }

 private:
  PP_Resource StringToBufferResource(const std::string& str);

  // <code>PPB_ContentDecryptor_Private</code> dispatchers. These are passed to
  // <code>callback_factory_</code> to ensure that calls into
  // <code>PPP_ContentDecryptor_Private</code> are asynchronous.
  void NeedKey(int32_t result, const DecryptorMessage& decryptor_message);
  void KeyAdded(int32_t result, const DecryptorMessage& decryptor_message);
  void KeyMessage(int32_t result, const DecryptorMessage& decryptor_message);
  void KeyError(int32_t result, const DecryptorMessage& decryptor_message);
  void DeliverBlock(int32_t result, const DecryptedBlock& decrypted_block);

  pp::CompletionCallbackFactory<CDMWrapper> callback_factory_;
};

CDMWrapper::CDMWrapper(PP_Instance instance,
                       pp::Module* module)
    : pp::Instance(instance),
      pp::ContentDecryptor_Private(this) {
  callback_factory_.Initialize(this);
}

bool CDMWrapper::GenerateKeyRequest(const std::string& key_system,
                                    pp::VarArrayBuffer init_data) {
  PP_DCHECK(!key_system.empty() && init_data.ByteLength());

  DecryptorMessage decryptor_message;
  decryptor_message.key_system = key_system;
  decryptor_message.session_id = "0";
  decryptor_message.default_url = "http://www.google.com";
  decryptor_message.message_data = "GenerateKeyRequest";

  CallOnMain(callback_factory_.NewCallback(&CDMWrapper::KeyMessage,
                                           decryptor_message));
  return true;
}

bool CDMWrapper::AddKey(const std::string& session_id,
                        pp::VarArrayBuffer key,
                        pp::VarArrayBuffer init_data) {
  const std::string key_string(reinterpret_cast<char*>(key.Map()),
                               key.ByteLength());
  const std::string init_data_string(reinterpret_cast<char*>(init_data.Map()),
                                     init_data.ByteLength());

  PP_DCHECK(!session_id.empty() && !key_string.empty());

  DecryptorMessage decryptor_message;
  decryptor_message.key_system = "AddKey";
  decryptor_message.session_id = "0";
  decryptor_message.default_url = "http://www.google.com";
  decryptor_message.message_data = "AddKey";
  CallOnMain(callback_factory_.NewCallback(&CDMWrapper::KeyAdded,
                                           decryptor_message));
  return true;
}

bool CDMWrapper::CancelKeyRequest(const std::string& session_id) {
  // TODO(tomfinegan): cancel pending key request in CDM.

  PP_DCHECK(!session_id.empty());

  DecryptorMessage decryptor_message;
  decryptor_message.key_system = "CancelKeyRequest";
  decryptor_message.session_id = "0";
  decryptor_message.default_url = "http://www.google.com";
  decryptor_message.message_data = "CancelKeyRequest";

  CallOnMain(callback_factory_.NewCallback(&CDMWrapper::KeyMessage,
                                           decryptor_message));
  return true;
}

bool CDMWrapper::Decrypt(pp::Buffer_Dev encrypted_buffer,
                         const PP_EncryptedBlockInfo& encrypted_block_info) {
  PP_DCHECK(!encrypted_buffer.is_null());

  DecryptedBlock decrypted_block;
  decrypted_block.decrypted_data = "Pretend I'm decrypted data!";
  decrypted_block.decrypted_block_info.result = PP_DECRYPTRESULT_SUCCESS;
  decrypted_block.decrypted_block_info.tracking_info =
      encrypted_block_info.tracking_info;

  // TODO(tomfinegan): This would end up copying a lot of data in the real
  // implementation if we continue passing std::strings around. It *might* not
  // be such a big deal w/a real CDM. We may be able to simply pass a pointer
  // into the CDM. Otherwise we could look into using std::tr1::shared_ptr
  // instead of passing a giant std::string filled with encrypted data.
  CallOnMain(callback_factory_.NewCallback(&CDMWrapper::DeliverBlock,
                                           decrypted_block));
  return true;
}

PP_Resource CDMWrapper::StringToBufferResource(const std::string& str) {
  if (str.empty())
    return 0;

  pp::Buffer_Dev buffer(this, str.size());
  if (!buffer.data())
    return 0;

  std::memcpy(buffer.data(), str.data(), str.size());
  return buffer.detach();
}

void CDMWrapper::NeedKey(int32_t result,
                         const DecryptorMessage& decryptor_message) {
  const std::string& message_data = decryptor_message.message_data;
  pp::VarArrayBuffer init_data(message_data.size());
  std::memcpy(init_data.Map(), message_data.data(), message_data.size());
  pp::ContentDecryptor_Private::NeedKey(decryptor_message.key_system,
                                        decryptor_message.session_id,
                                        init_data);
}

void CDMWrapper::KeyAdded(int32_t result,
                          const DecryptorMessage& decryptor_message) {
  pp::ContentDecryptor_Private::KeyAdded(decryptor_message.key_system,
                                         decryptor_message.session_id);
}

void CDMWrapper::KeyMessage(int32_t result,
                            const DecryptorMessage& decryptor_message) {
  pp::Buffer_Dev message_buffer(
      StringToBufferResource(decryptor_message.message_data));
  pp::ContentDecryptor_Private::KeyMessage(decryptor_message.key_system,
                                           decryptor_message.session_id,
                                           message_buffer,
                                           decryptor_message.default_url);
}

void CDMWrapper::KeyError(int32_t result,
                          const DecryptorMessage& decryptor_message) {
  pp::ContentDecryptor_Private::KeyError(decryptor_message.key_system,
                                         decryptor_message.session_id,
                                         decryptor_message.media_error,
                                         decryptor_message.system_code);
}

void CDMWrapper::DeliverBlock(int32_t result,
                              const DecryptedBlock& decrypted_block) {
  pp::Buffer_Dev decrypted_buffer(
      StringToBufferResource(decrypted_block.decrypted_data));
  pp::ContentDecryptor_Private::DeliverBlock(
      decrypted_buffer,
      decrypted_block.decrypted_block_info);
}

// This object is the global object representing this plugin library as long
// as it is loaded.
class MyModule : public pp::Module {
 public:
  MyModule() : pp::Module() {}
  virtual ~MyModule() {}

  virtual pp::Instance* CreateInstance(PP_Instance instance) {
    return new CDMWrapper(instance, this);
  }
};

namespace pp {

// Factory function for your specialization of the Module object.
Module* CreateModule() {
  return new MyModule();
}

}  // namespace pp
