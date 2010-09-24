// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_PLUGINS_PPB_PRIVATE2_H_
#define WEBKIT_GLUE_PLUGINS_PPB_PRIVATE2_H_

#include "third_party/ppapi/c/pp_instance.h"
#include "third_party/ppapi/c/pp_module.h"
#include "third_party/ppapi/c/pp_var.h"

#define PPB_PRIVATE2_INTERFACE "PPB_Private2;1"

struct PPB_Private2 {
  // Sets or clears the rendering hint that the given plugin instance is always
  // on top of page content. Somewhat more optimized painting can be used in
  // this case.
  void (*SetInstanceAlwaysOnTop)(PP_Instance instance, bool on_top);
};

#endif  // WEBKIT_GLUE_PLUGINS_PPB_PRIVATE2_H_
