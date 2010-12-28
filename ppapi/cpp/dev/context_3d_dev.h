// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_CPP_DEV_GRAPHICS_3D_DEV_H_
#define PPAPI_CPP_DEV_GRAPHICS_3D_DEV_H_

#include "ppapi/c/dev/ppb_context_3d_dev.h"

#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/resource.h"

namespace pp {

class Surface3D_Dev;

class Context3D_Dev : public Resource {
 public:
  // Creates an is_null() Context3D object.
  Context3D_Dev() {}

  Context3D_Dev(const Instance& instance,
                PP_Config3D_Dev config,
                const Context3D_Dev& share_context,
                const int32_t* attrib_list);

  int32_t BindSurfaces(const Surface3D_Dev& draw,
                       const Surface3D_Dev& read);

 protected:
  explicit Context3D_Dev(PP_Resource resource_id) : Resource(resource_id) {}
  static Context3D_Dev FromResource(PP_Resource resource_id);
};

}  // namespace pp

#endif  // PPAPI_CPP_DEV_GRAPHICS_3D_DEV_H_

