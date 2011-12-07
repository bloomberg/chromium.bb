// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/shared_impl/font_impl.h"

#include "ppapi/c/dev/ppb_font_dev.h"

namespace ppapi {

// static
bool FontImpl::IsPPFontDescriptionValid(const PP_FontDescription_Dev& desc) {
  // Check validity of string. We can't check the actual text since we could
  // be on the wrong thread and don't know if we're in the plugin or the host.
  if (desc.face.type != PP_VARTYPE_STRING &&
      desc.face.type != PP_VARTYPE_UNDEFINED)
    return false;

  // Check enum ranges.
  if (static_cast<int>(desc.family) < PP_FONTFAMILY_DEFAULT ||
      static_cast<int>(desc.family) > PP_FONTFAMILY_MONOSPACE)
    return false;
  if (static_cast<int>(desc.weight) < PP_FONTWEIGHT_100 ||
      static_cast<int>(desc.weight) > PP_FONTWEIGHT_900)
    return false;

  // Check for excessive sizes which may cause layout to get confused.
  if (desc.size > 200)
    return false;

  return true;
}

}  // namespace ppapi
