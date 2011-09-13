// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/c/pp_errors.h"
#include "ppapi/thunk/thunk.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppb_context_3d_api.h"
#include "ppapi/thunk/resource_creation_api.h"

namespace ppapi {
namespace thunk {

namespace {

typedef EnterResource<PPB_Context3D_API> EnterContext3D;

PP_Resource Create(PP_Instance instance,
                   PP_Config3D_Dev config,
                   PP_Resource share_context,
                   const int32_t* attrib_list) {
  EnterFunction<ResourceCreationAPI> enter(instance, true);
  if (enter.failed())
    return 0;
  return enter.functions()->CreateContext3D(instance, config, share_context,
                                            attrib_list);
}

PP_Bool IsContext3D(PP_Resource resource) {
  EnterContext3D enter(resource, false);
  return PP_FromBool(enter.succeeded());
}

int32_t GetAttrib(PP_Resource context, int32_t attribute, int32_t* value) {
  EnterContext3D enter(context, true);
  if (enter.failed())
    return PP_ERROR_BADRESOURCE;
  return enter.object()->GetAttrib(attribute, value);
}

int32_t BindSurfaces(PP_Resource context, PP_Resource draw, PP_Resource read) {
  EnterContext3D enter(context, true);
  if (enter.failed())
    return PP_ERROR_BADRESOURCE;
  return enter.object()->BindSurfaces(draw, read);
}

int32_t GetBoundSurfaces(PP_Resource context,
                         PP_Resource* draw,
                         PP_Resource* read) {
  EnterContext3D enter(context, true);
  if (enter.failed())
    return PP_ERROR_BADRESOURCE;
  return enter.object()->GetBoundSurfaces(draw, read);
}

const PPB_Context3D_Dev g_ppb_context_3d_thunk = {
  &Create,
  &IsContext3D,
  &GetAttrib,
  &BindSurfaces,
  &GetBoundSurfaces
};

}  // namespace

const PPB_Context3D_Dev* GetPPB_Context3D_Dev_Thunk() {
  return &g_ppb_context_3d_thunk;
}

}  // namespace thunk
}  // namespace ppapi
