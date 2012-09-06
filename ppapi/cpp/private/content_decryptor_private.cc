// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/cpp/private/content_decryptor_private.h"

#include <cstring>  // memcpy

#include "ppapi/c/ppb_var.h"
#include "ppapi/c/private/ppb_content_decryptor_private.h"
#include "ppapi/c/private/ppp_content_decryptor_private.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/instance_handle.h"
#include "ppapi/cpp/logging.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/module_impl.h"
#include "ppapi/cpp/var.h"

namespace pp {

namespace {

static const char kPPPContentDecryptorInterface[] =
    PPP_CONTENTDECRYPTOR_PRIVATE_INTERFACE;

PP_Bool GenerateKeyRequest(PP_Instance instance,
                           PP_Var key_system_arg,
                           PP_Var init_data_arg) {
  void* object =
      Instance::GetPerInstanceObject(instance, kPPPContentDecryptorInterface);
  if (!object)
    return PP_FALSE;

  pp::Var key_system_var(pp::PASS_REF, key_system_arg);
  if (key_system_var.is_string() == false)
    return PP_FALSE;

  pp::Var init_data_var(pp::PASS_REF, init_data_arg);
  if (init_data_var.is_array_buffer() == false)
    return PP_FALSE;
  pp::VarArrayBuffer init_data_array_buffer(init_data_var);

  return PP_FromBool(
      static_cast<ContentDecryptor_Private*>(object)->GenerateKeyRequest(
          key_system_var.AsString(),
          init_data_array_buffer));
}

PP_Bool AddKey(PP_Instance instance,
               PP_Var session_id_arg,
               PP_Var key_arg,
               PP_Var init_data_arg) {
  void* object =
      Instance::GetPerInstanceObject(instance, kPPPContentDecryptorInterface);
  if (!object)
    return PP_FALSE;

  pp::Var session_id_var(pp::PASS_REF, session_id_arg);
  if (session_id_var.is_string() == false)
    return PP_FALSE;

  pp::Var key_var(pp::PASS_REF, key_arg);
  if (key_var.is_array_buffer() == false)
    return PP_FALSE;
  pp::VarArrayBuffer key(key_var);

  pp::Var init_data_var(pp::PASS_REF, init_data_arg);
  if (init_data_var.is_array_buffer() == false)
    return PP_FALSE;
  pp::VarArrayBuffer init_data(init_data_var);

  return PP_FromBool(
      static_cast<ContentDecryptor_Private*>(object)->AddKey(
          session_id_var.AsString(),
          key,
          init_data));
}

PP_Bool CancelKeyRequest(PP_Instance instance,
                         PP_Var session_id_arg) {
  void* object =
      Instance::GetPerInstanceObject(instance, kPPPContentDecryptorInterface);
  if (!object)
    return PP_FALSE;

  pp::Var session_id_var(pp::PASS_REF, session_id_arg);
  if (session_id_var.is_string() == false)
    return PP_FALSE;

  return PP_FromBool(
      static_cast<ContentDecryptor_Private*>(object)->
          CancelKeyRequest(session_id_var.AsString()));
}


PP_Bool Decrypt(PP_Instance instance,
                PP_Resource encrypted_resource,
                const PP_EncryptedBlockInfo* encrypted_block_info) {
  void* object =
      Instance::GetPerInstanceObject(instance, kPPPContentDecryptorInterface);
  if (!object)
    return PP_FALSE;

  pp::Buffer_Dev encrypted_block(pp::PassRef(), encrypted_resource);

  return PP_FromBool(
      static_cast<ContentDecryptor_Private*>(object)->Decrypt(
          encrypted_block,
          *encrypted_block_info));
}

PP_Bool DecryptAndDecode(PP_Instance instance,
                         PP_Resource encrypted_resource,
                         const PP_EncryptedBlockInfo* encrypted_block_info) {
  void* object =
      Instance::GetPerInstanceObject(instance, kPPPContentDecryptorInterface);
  if (!object)
    return PP_FALSE;

  pp::Buffer_Dev encrypted_block(pp::PassRef(), encrypted_resource);

  return PP_FromBool(
      static_cast<ContentDecryptor_Private*>(object)->DecryptAndDecode(
          encrypted_block,
          *encrypted_block_info));
}

const PPP_ContentDecryptor_Private ppp_content_decryptor = {
  &GenerateKeyRequest,
  &AddKey,
  &CancelKeyRequest,
  &Decrypt,
  &DecryptAndDecode
};

template <> const char* interface_name<PPB_ContentDecryptor_Private>() {
  return PPB_CONTENTDECRYPTOR_PRIVATE_INTERFACE;
}

}  // namespace

ContentDecryptor_Private::ContentDecryptor_Private(Instance* instance)
    : associated_instance_(instance) {
  Module::Get()->AddPluginInterface(kPPPContentDecryptorInterface,
                                    &ppp_content_decryptor);
  instance->AddPerInstanceObject(kPPPContentDecryptorInterface, this);
}

ContentDecryptor_Private::~ContentDecryptor_Private() {
  Instance::RemovePerInstanceObject(associated_instance_,
                                    kPPPContentDecryptorInterface,
                                    this);
}

void ContentDecryptor_Private::NeedKey(const std::string& key_system,
                                       const std::string& session_id,
                                       pp::VarArrayBuffer init_data) {
  // session_id can be empty here.
  if (has_interface<PPB_ContentDecryptor_Private>()) {
    pp::Var key_system_var(key_system);
    pp::Var session_id_var(session_id);

    get_interface<PPB_ContentDecryptor_Private>()->NeedKey(
        associated_instance_.pp_instance(),
        key_system_var.pp_var(),
        session_id_var.pp_var(),
        init_data.pp_var());
  }
}

void ContentDecryptor_Private::KeyAdded(const std::string& key_system,
                                        const std::string& session_id) {
  if (has_interface<PPB_ContentDecryptor_Private>()) {
    pp::Var key_system_var(key_system);
    pp::Var session_id_var(session_id);
    get_interface<PPB_ContentDecryptor_Private>()->KeyAdded(
        associated_instance_.pp_instance(),
        key_system_var.pp_var(),
        session_id_var.pp_var());
  }
}

void ContentDecryptor_Private::KeyMessage(const std::string& key_system,
                                          const std::string& session_id,
                                          pp::Buffer_Dev message,
                                          const std::string& default_url) {
  if (has_interface<PPB_ContentDecryptor_Private>()) {
    pp::Var key_system_var(key_system);
    pp::Var session_id_var(session_id);
    pp::Var default_url_var(default_url);
    get_interface<PPB_ContentDecryptor_Private>()->KeyMessage(
        associated_instance_.pp_instance(),
        key_system_var.pp_var(),
        session_id_var.pp_var(),
        message.pp_resource(),
        default_url_var.pp_var());
  }
}

void ContentDecryptor_Private::KeyError(const std::string& key_system,
                                        const std::string& session_id,
                                        int32_t media_error,
                                        int32_t system_code) {
  if (has_interface<PPB_ContentDecryptor_Private>()) {
    pp::Var key_system_var(key_system);
    pp::Var session_id_var(session_id);
    get_interface<PPB_ContentDecryptor_Private>()->KeyError(
        associated_instance_.pp_instance(),
        key_system_var.pp_var(),
        session_id_var.pp_var(),
        media_error,
        system_code);
  }
}

void ContentDecryptor_Private::DeliverBlock(
    pp::Buffer_Dev decrypted_block,
    const PP_DecryptedBlockInfo& decrypted_block_info) {
  if (has_interface<PPB_ContentDecryptor_Private>()) {
    get_interface<PPB_ContentDecryptor_Private>()->DeliverBlock(
        associated_instance_.pp_instance(),
        decrypted_block.pp_resource(),
        &decrypted_block_info);
  }
}

void ContentDecryptor_Private::DeliverFrame(
    pp::Buffer_Dev decrypted_frame,
    const PP_DecryptedBlockInfo& decrypted_block_info) {
  if (has_interface<PPB_ContentDecryptor_Private>()) {
    get_interface<PPB_ContentDecryptor_Private>()->DeliverFrame(
        associated_instance_.pp_instance(),
        decrypted_frame.pp_resource(),
        &decrypted_block_info);
  }
}

void ContentDecryptor_Private::DeliverSamples(
    pp::Buffer_Dev decrypted_samples,
    const PP_DecryptedBlockInfo& decrypted_block_info) {
  if (has_interface<PPB_ContentDecryptor_Private>()) {
    get_interface<PPB_ContentDecryptor_Private>()->DeliverSamples(
        associated_instance_.pp_instance(),
        decrypted_samples.pp_resource(),
        &decrypted_block_info);
  }
}

}  // namespace pp
