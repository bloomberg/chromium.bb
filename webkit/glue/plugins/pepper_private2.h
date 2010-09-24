// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_PLUGINS_PEPPER_PRIVATE2_H_
#define WEBKIT_GLUE_PLUGINS_PEPPER_PRIVATE2_H_

#include "webkit/glue/plugins/pepper_resource.h"

struct PPB_Private2;

namespace pepper {

class Private2 {
 public:
  // Returns a pointer to the interface implementing PPB_Private2 that is
  // exposed to the plugin.
  static const PPB_Private2* GetInterface();
};

}  // namespace pepper

#endif  // WEBKIT_GLUE_PLUGINS_PEPPER_PRIVATE2_H_
