// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_CPP_DEV_SURFACE_3D_DEV_H_
#define PPAPI_CPP_DEV_SURFACE_3D_DEV_H_

#include "ppapi/c/dev/ppb_surface_3d_dev.h"

#include "ppapi/cpp/completion_callback.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/resource.h"

namespace pp {

class CompletionCallback;

class Surface3D_Dev : public Resource {
 public:
  // Creates an is_null() Surface3D object.
  Surface3D_Dev() {}

  Surface3D_Dev(const Instance& instance,
                PP_Config3D_Dev config,
                const int32_t* attrib_list);

  int32_t SwapBuffers(const CompletionCallback& cc) const;

 protected:
  explicit Surface3D_Dev(PP_Resource resource_id) : Resource(resource_id) {}
  static Surface3D_Dev FromResource(PP_Resource resource_id);
};

}  // namespace pp

#endif  // PPAPI_CPP_DEV_SURFACE_3D_DEV_H_

