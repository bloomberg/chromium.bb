// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"

#include "webkit/glue/plugins/pepper_private.h"

#include "base/utf_string_conversions.h"
#include "grit/webkit_strings.h"
#include "webkit/glue/webkit_glue.h"
#include "webkit/glue/plugins/pepper_var.h"
#include "webkit/glue/plugins/ppb_private.h"

namespace pepper {

namespace {

PP_Var GetLocalizedString(PP_ResourceString string_id) {
  std::string rv;
  if (string_id == PP_RESOURCESTRING_PDFGETPASSWORD)
    rv = UTF16ToUTF8(webkit_glue::GetLocalizedString(IDS_PDF_NEED_PASSWORD));

  return StringToPPVar(rv);
}

const PPB_Private ppb_private = {
  &GetLocalizedString,
};

}  // namespace

// static
const PPB_Private* Private::GetInterface() {
  return &ppb_private;
}

}  // namespace pepper
