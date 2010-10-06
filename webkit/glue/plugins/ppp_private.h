// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_PLUGINS_PPP_PRIVATE_H_
#define WEBKIT_GLUE_PLUGINS_PPP_PRIVATE_H_

#include "third_party/ppapi/c/pp_instance.h"
#include "third_party/ppapi/c/pp_point.h"
#include "third_party/ppapi/c/pp_var.h"

#define PPP_PRIVATE_INTERFACE "PPP_Private;1"

struct PPP_Private {
  // Returns an absolute URL if the position is over a link.
  PP_Var (*GetLinkAtPosition)(PP_Instance instance,
                              PP_Point point);
};

#endif  // WEBKIT_GLUE_PLUGINS_PPP_PRIVATE_H_
