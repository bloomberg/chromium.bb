// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_CPP_GRAPHICS_3D_H_
#define PPAPI_CPP_GRAPHICS_3D_H_

#include "ppapi/c/ppb_graphics_3d.h"
#include "ppapi/cpp/resource.h"

namespace pp {

class CompletionCallback;
class Instance;
class Var;

class Graphics3D : public Resource {
 public:
  // Creates an is_null() Graphics3D_Dev object.
  Graphics3D();

  Graphics3D(const Instance& instance,
             const Graphics3D& share_context,
             const int32_t* attrib_list);

  ~Graphics3D();

  int32_t GetAttribs(int32_t* attrib_list) const;
  int32_t SetAttribs(int32_t* attrib_list);

  int32_t ResizeBuffers(int32_t width, int32_t height);

  int32_t SwapBuffers(const CompletionCallback& cc);
};

}  // namespace pp

#endif  // PPAPI_CPP_GRAPHICS_3D_H_

