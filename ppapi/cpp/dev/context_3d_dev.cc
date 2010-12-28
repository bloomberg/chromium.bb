// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/cpp/dev/context_3d_dev.h"

#include "ppapi/c/pp_errors.h"
#include "ppapi/cpp/dev/surface_3d_dev.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/module_impl.h"

namespace pp {

namespace {

template <> const char* interface_name<PPB_Context3D_Dev>() {
  return PPB_CONTEXT_3D_DEV_INTERFACE;
}

}  // namespace

Context3D_Dev Context3D_Dev::FromResource(PP_Resource resource_id) {
  if (has_interface<PPB_Context3D_Dev>() &&
      get_interface<PPB_Context3D_Dev>()->IsContext3D(resource_id))
    return Context3D_Dev(resource_id);

  return Context3D_Dev();
}

Context3D_Dev::Context3D_Dev(const Instance& instance,
                             PP_Config3D_Dev config,
                             const Context3D_Dev& share_context,
                             const int32_t* attrib_list) {
  if (has_interface<PPB_Context3D_Dev>()) {
    PassRefFromConstructor(get_interface<PPB_Context3D_Dev>()->Create(
        instance.pp_instance(),
        config,
        share_context.pp_resource(),
        attrib_list));
  }
}

int32_t Context3D_Dev::BindSurfaces(const Surface3D_Dev& draw,
                                    const Surface3D_Dev& read) {
  if (!has_interface<PPB_Context3D_Dev>())
    return PP_ERROR_NOINTERFACE;

  return get_interface<PPB_Context3D_Dev>()->BindSurfaces(
      pp_resource(), draw.pp_resource(), read.pp_resource());
}

}  // namespace pp
