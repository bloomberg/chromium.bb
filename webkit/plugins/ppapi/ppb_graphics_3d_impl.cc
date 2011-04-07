// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/plugins/ppapi/ppb_graphics_3d_impl.h"

#include "base/logging.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_errors.h"
#include "webkit/plugins/ppapi/common.h"

namespace webkit {
namespace ppapi {

namespace {

int32_t GetConfigs(PP_Config3D_Dev* configs,
                   int32_t config_size,
                   int32_t* num_config) {
  // TODO(alokp): Implement me.
  return PP_ERROR_FAILED;
}

int32_t GetConfigAttribs(PP_Config3D_Dev config, int32_t* attrib_list) {
  // TODO(alokp): Implement me.
  return PP_ERROR_FAILED;
}

PP_Var GetString(int32_t name) {
  // TODO(alokp): Implement me.
  return PP_MakeUndefined();
}

PP_Resource Create(PP_Instance instance_id,
                   PP_Config3D_Dev config,
                   PP_Resource share_context,
                   const int32_t* attrib_list) {
  // TODO(alokp): Support shared context.
  DCHECK_EQ(0, share_context);
  if (share_context != 0)
    return 0;

  PluginInstance* instance = ResourceTracker::Get()->GetInstance(instance_id);
  if (!instance)
    return 0;

  scoped_refptr<PPB_Graphics3D_Impl> context(
      new PPB_Graphics3D_Impl(instance));
  if (!context->Init(config, share_context, attrib_list))
    return 0;

  return context->GetReference();
}

PP_Bool IsGraphics3D(PP_Resource resource) {
  return BoolToPPBool(!!Resource::GetAs<PPB_Graphics3D_Impl>(resource));
}

int32_t GetAttribs(PP_Resource context, int32_t* attrib_list) {
  // TODO(alokp): Implement me.
  return 0;
}

int32_t SetAttribs(PP_Resource context, int32_t* attrib_list) {
  // TODO(alokp): Implement me.
  return 0;
}

int32_t SwapBuffers(PP_Resource context, PP_CompletionCallback callback) {
  // TODO(alokp): Implement me.
  return 0;
}

const PPB_Graphics3D_Dev ppb_graphics3d = {
  &GetConfigs,
  &GetConfigAttribs,
  &GetString,
  &Create,
  &IsGraphics3D,
  &GetAttribs,
  &SetAttribs,
  &SwapBuffers
};

}  // namespace

PPB_Graphics3D_Impl::PPB_Graphics3D_Impl(PluginInstance* instance)
    : Resource(instance) {
}

PPB_Graphics3D_Impl::~PPB_Graphics3D_Impl() {
}

const PPB_Graphics3D_Dev* PPB_Graphics3D_Impl::GetInterface() {
  return &ppb_graphics3d;
}

PPB_Graphics3D_Impl* PPB_Graphics3D_Impl::AsPPB_Graphics3D_Impl() {
  return this;
}

bool PPB_Graphics3D_Impl::Init(PP_Config3D_Dev config,
                               PP_Resource share_context,
                               const int32_t* attrib_list) {
  // TODO(alokp): Implement me.
  return false;
}

}  // namespace ppapi
}  // namespace webkit

