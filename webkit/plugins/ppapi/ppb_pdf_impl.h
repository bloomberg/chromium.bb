// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_PLUGINS_PPB_PDF_IMPL_H_
#define WEBKIT_GLUE_PLUGINS_PPB_PDF_IMPL_H_

#include "webkit/plugins/ppapi/resource.h"

struct PPB_PDF;

namespace webkit {
namespace ppapi {

class PPB_PDF_Impl {
 public:
  // Returns a pointer to the interface implementing PPB_PDF that is exposed
  // to the plugin.
  static const PPB_PDF* GetInterface();
};

}  // namespace ppapi
}  // namespace webkit

#endif  // WEBKIT_GLUE_PLUGINS_PPB_PDF_IMPL_H_
