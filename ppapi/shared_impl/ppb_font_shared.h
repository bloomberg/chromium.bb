// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_SHARED_IMPL_PPB_FONT_SHARED_H_
#define PPAPI_SHARED_IMPL_PPB_FONT_SHARED_H_
#pragma once

#include <string>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_stdint.h"
#include "ppapi/shared_impl/ppapi_shared_export.h"

struct PP_FontDescription_Dev;

namespace ppapi {

class PPAPI_SHARED_EXPORT PPB_Font_Shared {
 public:
  // Validates the parameters in thee description. Can be called on any thread.
  static bool IsPPFontDescriptionValid(const PP_FontDescription_Dev& desc);

 private:
  DISALLOW_COPY_AND_ASSIGN(PPB_Font_Shared);
};

}  // namespace ppapi

#endif  // PPAPI_SHARED_IMPL_PPB_FONT_SHARED_H_
