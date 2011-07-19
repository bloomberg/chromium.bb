// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/c/pp_errors.h"
#include "ppapi/thunk/common.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/thunk.h"
#include "ppapi/thunk/ppb_surface_3d_api.h"
#include "ppapi/thunk/resource_creation_api.h"

namespace ppapi {
namespace thunk {

namespace {

PP_Resource Create(PP_Instance instance,
                   PP_Config3D_Dev config,
                   const int32_t* attrib_list) {
  EnterFunction<ResourceCreationAPI> enter(instance, true);
  if (enter.failed())
    return 0;
  return enter.functions()->CreateSurface3D(instance, config, attrib_list);
}

PP_Bool IsSurface3D(PP_Resource resource) {
  EnterResource<PPB_Surface3D_API> enter(resource, true);
  return PP_FromBool(enter.succeeded());
}

int32_t SetAttrib(PP_Resource surface, int32_t attribute, int32_t value) {
  EnterResource<PPB_Surface3D_API> enter(surface, true);
  if (enter.failed())
    return PP_ERROR_BADRESOURCE;
  return enter.object()->SetAttrib(attribute, value);
}

int32_t GetAttrib(PP_Resource surface,
                  int32_t attribute,
                  int32_t* value) {
  EnterResource<PPB_Surface3D_API> enter(surface, true);
  if (enter.failed())
    return PP_ERROR_BADRESOURCE;
  return enter.object()->GetAttrib(attribute, value);
}

int32_t SwapBuffers(PP_Resource surface,
                    PP_CompletionCallback callback) {
  EnterResource<PPB_Surface3D_API> enter(surface, true);
  if (enter.failed())
    return MayForceCallback(callback, PP_ERROR_BADRESOURCE);
  int32_t result = enter.object()->SwapBuffers(callback);
  return MayForceCallback(callback, result);
}

const PPB_Surface3D_Dev g_ppb_surface_3d_thunk = {
  &Create,
  &IsSurface3D,
  &SetAttrib,
  &GetAttrib,
  &SwapBuffers
};

}  // namespace

const PPB_Surface3D_Dev* GetPPB_Surface3D_Thunk() {
  return &g_ppb_surface_3d_thunk;
}

}  // namespace thunk
}  // namespace ppapi
