// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_CPP_DEV_FULLSCREEN_DEV_H_
#define PPAPI_CPP_DEV_FULLSCREEN_DEV_H_

#include "ppapi/cpp/instance_handle.h"

namespace pp {

class Size;

class Fullscreen_Dev {
 public:
  Fullscreen_Dev(const InstanceHandle& instance);
  virtual ~Fullscreen_Dev();

  // PPB_Fullscreen_Dev methods.
  bool IsFullscreen();
  bool SetFullscreen(bool fullscreen);
  bool GetScreenSize(Size* size);

 private:
  InstanceHandle instance_;
};

}  // namespace pp

#endif  // PPAPI_CPP_DEV_FULLSCREEN_DEV_H_
