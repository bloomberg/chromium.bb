// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/thunk/thunk.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppb_audio_api.h"
#include "ppapi/thunk/resource_creation_api.h"

namespace ppapi {
namespace thunk {

namespace {

PP_Resource Create(PP_Instance instance,
                   PP_Resource config_id,
                   PPB_Audio_Callback callback,
                   void* user_data) {
  EnterFunction<ResourceCreationAPI> enter(instance, true);
  if (enter.failed())
    return 0;
  return enter.functions()->CreateAudio(instance, config_id,
                                        callback, user_data);
}

PP_Bool IsAudio(PP_Resource resource) {
  EnterResource<PPB_Audio_API> enter(resource, false);
  return enter.succeeded() ? PP_TRUE : PP_FALSE;
}

PP_Resource GetCurrentConfiguration(PP_Resource audio_id) {
  EnterResource<PPB_Audio_API> enter(audio_id, true);
  if (enter.failed())
    return 0;
  return enter.object()->GetCurrentConfig();
}

PP_Bool StartPlayback(PP_Resource audio_id) {
  EnterResource<PPB_Audio_API> enter(audio_id, true);
  if (enter.failed())
    return PP_FALSE;
  return enter.object()->StartPlayback();
}

PP_Bool StopPlayback(PP_Resource audio_id) {
  EnterResource<PPB_Audio_API> enter(audio_id, true);
  if (enter.failed())
    return PP_FALSE;
  return enter.object()->StopPlayback();
}

const PPB_Audio g_ppb_audio_thunk = {
  &Create,
  &IsAudio,
  &GetCurrentConfiguration,
  &StartPlayback,
  &StopPlayback
};

}  // namespace

const PPB_Audio* GetPPB_Audio_Thunk() {
  return &g_ppb_audio_thunk;
}

}  // namespace thunk
}  // namespace ppapi
