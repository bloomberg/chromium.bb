// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_CPP_DEV_GRAPHICS_3D_DEV_H_
#define PPAPI_CPP_DEV_GRAPHICS_3D_DEV_H_

#include "ppapi/c/dev/ppb_graphics_3d_dev.h"
#include "ppapi/cpp/resource.h"

namespace pp {

class CompletionCallback;
class Instance;
class Var;

class Graphics3D_Dev : public Resource {
 public:
  // Creates an is_null() Graphics3D_Dev object.
  Graphics3D_Dev();

  Graphics3D_Dev(const Instance& instance,
                 const Graphics3D_Dev& share_context,
                 const int32_t* attrib_list);

  ~Graphics3D_Dev();

  int32_t GetAttribs(int32_t* attrib_list) const;
  int32_t SetAttribs(int32_t* attrib_list);

  int32_t ResizeBuffers(int32_t width, int32_t height);

  int32_t SwapBuffers(const CompletionCallback& cc);
};

}  // namespace pp

#endif  // PPAPI_CPP_DEV_GRAPHICS_3D_DEV_H_

