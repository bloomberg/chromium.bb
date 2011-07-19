// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/plugins/ppapi/ppb_graphics_3d_impl.h"

#include "base/logging.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_errors.h"
#include "webkit/plugins/ppapi/ppapi_plugin_instance.h"
#include "webkit/plugins/ppapi/resource_tracker.h"

using ppapi::thunk::PPB_Graphics3D_API;

namespace webkit {
namespace ppapi {

PPB_Graphics3D_Impl::PPB_Graphics3D_Impl(PluginInstance* instance)
    : Resource(instance) {
}

PPB_Graphics3D_Impl::~PPB_Graphics3D_Impl() {
}

// static
PP_Resource PPB_Graphics3D_Impl::Create(PluginInstance* instance,
                                        PP_Config3D_Dev config,
                                        PP_Resource share_context,
                                        const int32_t* attrib_list) {
  scoped_refptr<PPB_Graphics3D_Impl> t(new PPB_Graphics3D_Impl(instance));
  if (!t->Init(config, share_context, attrib_list))
    return 0;
  return t->GetReference();
}

PPB_Graphics3D_API* PPB_Graphics3D_Impl::AsPPB_Graphics3D_API() {
  return this;
}

int32_t PPB_Graphics3D_Impl::GetAttribs(int32_t* attrib_list) {
  // TODO(alokp): Implement me.
  return PP_ERROR_FAILED;
}

int32_t PPB_Graphics3D_Impl::SetAttribs(int32_t* attrib_list) {
  // TODO(alokp): Implement me.
  return PP_ERROR_FAILED;
}

int32_t PPB_Graphics3D_Impl::SwapBuffers(PP_CompletionCallback callback) {
  // TODO(alokp): Implement me.
  return PP_ERROR_FAILED;
}

bool PPB_Graphics3D_Impl::Init(PP_Config3D_Dev config,
                               PP_Resource share_context,
                               const int32_t* attrib_list) {
  // TODO(alokp): Implement me.
  return false;
}

}  // namespace ppapi
}  // namespace webkit

