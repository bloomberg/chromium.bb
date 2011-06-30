// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/cpp/dev/surface_3d_dev.h"

#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/module_impl.h"

namespace pp {

namespace {

template <> const char* interface_name<PPB_Surface3D_Dev>() {
  return PPB_SURFACE_3D_DEV_INTERFACE;
}

}  // namespace

Surface3D_Dev Surface3D_Dev::FromResource(PP_Resource resource_id) {
  if (has_interface<PPB_Surface3D_Dev>() &&
      get_interface<PPB_Surface3D_Dev>()->IsSurface3D(resource_id))
    return Surface3D_Dev(resource_id);

  return Surface3D_Dev();
}

Surface3D_Dev::Surface3D_Dev(const Instance& instance,
                             PP_Config3D_Dev config,
                             const int32_t* attrib_list) {
  if (has_interface<PPB_Surface3D_Dev>()) {
    PassRefFromConstructor(get_interface<PPB_Surface3D_Dev>()->Create(
        instance.pp_instance(),
        config,
        attrib_list));
  }
}

int32_t Surface3D_Dev::SwapBuffers(const CompletionCallback& cc) const {
  if (!has_interface<PPB_Surface3D_Dev>())
    return cc.MayForce(PP_ERROR_NOINTERFACE);

  return get_interface<PPB_Surface3D_Dev>()->SwapBuffers(
      pp_resource(),
      cc.pp_completion_callback());
}

}  // namespace pp
