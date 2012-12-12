// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/c/private/ppb_content_decryptor_private.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppb_instance_api.h"
#include "ppapi/thunk/thunk.h"

namespace ppapi {
namespace thunk {

namespace {

void NeedKey(PP_Instance instance,
             PP_Var key_system,
             PP_Var session_id,
             PP_Var init_data) {
  EnterInstance enter(instance);
  if (enter.succeeded())
    enter.functions()->NeedKey(instance, key_system, session_id, init_data);
}

void KeyAdded(PP_Instance instance,
              PP_Var key_system,
              PP_Var session_id) {
  EnterInstance enter(instance);
  if (enter.succeeded())
    enter.functions()->KeyAdded(instance, key_system, session_id);
}

void KeyMessage(PP_Instance instance,
                PP_Var key_system,
                PP_Var session_id,
                PP_Var message,
                PP_Var default_url) {
  EnterInstance enter(instance);
  if (enter.succeeded()) {
    enter.functions()->KeyMessage(instance, key_system, session_id, message,
                                  default_url);
  }
}

void KeyError(PP_Instance instance,
              PP_Var key_system,
              PP_Var session_id,
              int32_t media_error,
              int32_t system_code) {
  EnterInstance enter(instance);
  if (enter.succeeded()) {
    enter.functions()->KeyError(instance, key_system, session_id, media_error,
                                system_code);
  }
}

void DeliverBlock(PP_Instance instance,
                  PP_Resource decrypted_block,
                  const PP_DecryptedBlockInfo* block_info) {
  EnterInstance enter(instance);
  if (enter.succeeded())
    enter.functions()->DeliverBlock(instance, decrypted_block, block_info);
}

void DecoderInitializeDone(PP_Instance instance,
                           PP_DecryptorStreamType decoder_type,
                           uint32_t request_id,
                           PP_Bool success) {
  EnterInstance enter(instance);
  if (enter.succeeded()) {
    enter.functions()->DecoderInitializeDone(instance,
                                             decoder_type,
                                             request_id,
                                             success);
  }
}

void DecoderDeinitializeDone(PP_Instance instance,
                             PP_DecryptorStreamType decoder_type,
                             uint32_t request_id) {
  EnterInstance enter(instance);
  if (enter.succeeded()) {
    enter.functions()->DecoderDeinitializeDone(instance,
                                               decoder_type,
                                               request_id);
  }
}

void DecoderResetDone(PP_Instance instance,
                      PP_DecryptorStreamType decoder_type,
                      uint32_t request_id) {
  EnterInstance enter(instance);
  if (enter.succeeded())
    enter.functions()->DecoderResetDone(instance, decoder_type, request_id);
}

void DeliverFrame(PP_Instance instance,
                  PP_Resource decrypted_frame,
                  const PP_DecryptedFrameInfo* frame_info) {
  EnterInstance enter(instance);
  if (enter.succeeded())
    enter.functions()->DeliverFrame(instance, decrypted_frame, frame_info);
}

void DeliverSamples(PP_Instance instance,
                    PP_Resource audio_frames,
                    const PP_DecryptedBlockInfo* block_info) {
  EnterInstance enter(instance);
  if (enter.succeeded())
    enter.functions()->DeliverSamples(instance, audio_frames, block_info);
}

const PPB_ContentDecryptor_Private g_ppb_decryption_thunk = {
  &NeedKey,
  &KeyAdded,
  &KeyMessage,
  &KeyError,
  &DeliverBlock,
  &DecoderInitializeDone,
  &DecoderDeinitializeDone,
  &DecoderResetDone,
  &DeliverFrame,
  &DeliverSamples
};

}  // namespace

const PPB_ContentDecryptor_Private*
    GetPPB_ContentDecryptor_Private_0_6_Thunk() {
  return &g_ppb_decryption_thunk;
}

}  // namespace thunk
}  // namespace ppapi
