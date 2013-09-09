// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// From private/ppb_platform_verification_private.idl,
//   modified Thu Sep  5 17:37:17 2013.

#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/private/ppb_platform_verification_private.h"
#include "ppapi/shared_impl/tracked_callback.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppb_instance_api.h"
#include "ppapi/thunk/ppb_platform_verification_api.h"
#include "ppapi/thunk/resource_creation_api.h"
#include "ppapi/thunk/thunk.h"

namespace ppapi {
namespace thunk {

namespace {

PP_Resource Create(PP_Instance instance) {
  VLOG(4) << "PPB_PlatformVerification_Private::Create()";
  EnterResourceCreation enter(instance);
  if (enter.failed())
    return 0;
  return enter.functions()->CreatePlatformVerificationPrivate(instance);
}

PP_Bool IsPlatformVerification(PP_Resource resource) {
  VLOG(4) << "PPB_PlatformVerification_Private::IsPlatformVerification()";
  EnterResource<PPB_PlatformVerification_API> enter(resource, false);
  return PP_FromBool(enter.succeeded());
}

PP_Bool CanChallengePlatform(PP_Resource instance) {
  VLOG(4) << "PPB_PlatformVerification_Private::CanChallengePlatform()";
  EnterResource<PPB_PlatformVerification_API> enter(instance, true);
  if (enter.failed())
    return PP_FALSE;
  return enter.object()->CanChallengePlatform();
}

int32_t ChallengePlatform(PP_Resource instance,
                          struct PP_Var service_id,
                          struct PP_Var challenge,
                          struct PP_Var* signed_data,
                          struct PP_Var* signed_data_signature,
                          struct PP_Var* platform_key_certificate,
                          struct PP_CompletionCallback callback) {
  VLOG(4) << "PPB_PlatformVerification_Private::ChallengePlatform()";
  EnterResource<PPB_PlatformVerification_API> enter(instance, callback, true);
  if (enter.failed())
    return enter.retval();
  return enter.SetResult(enter.object()->ChallengePlatform(
      service_id,
      challenge,
      signed_data,
      signed_data_signature,
      platform_key_certificate,
      enter.callback()));
}

const PPB_PlatformVerification_Private_0_1
    g_ppb_platformverification_private_thunk_0_1 = {
  &Create,
  &IsPlatformVerification,
  &CanChallengePlatform,
  &ChallengePlatform
};

}  // namespace

const PPB_PlatformVerification_Private_0_1*
    GetPPB_PlatformVerification_Private_0_1_Thunk() {
  return &g_ppb_platformverification_private_thunk_0_1;
}

}  // namespace thunk
}  // namespace ppapi
