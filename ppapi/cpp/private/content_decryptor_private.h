// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_CPP_PRIVATE_CONTENT_DECRYPTOR_PRIVATE_H_
#define PPAPI_CPP_PRIVATE_CONTENT_DECRYPTOR_PRIVATE_H_

#include "ppapi/c/private/ppb_content_decryptor_private.h"
#include "ppapi/c/private/ppp_content_decryptor_private.h"

#include "ppapi/cpp/dev/buffer_dev.h"
#include "ppapi/cpp/instance_handle.h"
#include "ppapi/cpp/var.h"
#include "ppapi/cpp/var_array_buffer.h"

namespace pp {

class Instance;

class ContentDecryptor_Private {
 public:
  explicit ContentDecryptor_Private(Instance* instance);
  virtual ~ContentDecryptor_Private();

  // PPP_ContentDecryptor_Private functions exposed as virtual functions
  // for you to override.
  // TODO(tomfinegan): This could be optimized to pass pp::Var instead of
  // strings. The change would allow the CDM wrapper to reuse vars when
  // replying to the browser.
  virtual bool GenerateKeyRequest(const std::string& key_system,
                                  pp::VarArrayBuffer init_data) = 0;
  virtual bool AddKey(const std::string& session_id,
                      pp::VarArrayBuffer key) = 0;
  virtual bool CancelKeyRequest(const std::string& session_id) = 0;
  virtual bool Decrypt(pp::Buffer_Dev encrypted_buffer,
                       int32_t request_id) = 0;
  virtual bool DecryptAndDecode(pp::Buffer_Dev encrypted_buffer,
                                int32_t request_id) = 0;

  // PPB_ContentDecryptor_Private methods for passing data from the decryptor
  // to the browser.
  void NeedKey(const std::string& key_system,
               const std::string& session_id,
               pp::VarArrayBuffer init_data);
  void KeyAdded(const std::string& key_system,
                const std::string& session_id);
  void KeyMessage(const std::string& key_system,
                  const std::string& session_id,
                  pp::Buffer_Dev message,
                  const std::string& default_url);
  void KeyError(const std::string& key_system,
                const std::string& session_id,
                int32_t media_error,
                int32_t system_code);
  void DeliverBlock(pp::Buffer_Dev decrypted_block,
                    int32_t request_id);
  void DeliverFrame(pp::Buffer_Dev decrypted_frame,
                    int32_t request_id);
  void DeliverSamples(pp::Buffer_Dev decrypted_samples,
                      int32_t request_id);

 private:
  InstanceHandle associated_instance_;
};

}  // namespace pp

#endif  // PPAPI_CPP_PRIVATE_CONTENT_DECRYPTOR_PRIVATE_H_
