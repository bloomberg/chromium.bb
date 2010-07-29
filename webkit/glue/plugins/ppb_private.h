// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_PLUGINS_PPB_PRIVATE_H_
#define WEBKIT_GLUE_PLUGINS_PPB_PRIVATE_H_

#include "third_party/ppapi/c/pp_var.h"

#define PPB_PRIVATE_INTERFACE "PPB_Private;1"

typedef enum _pp_ResourceString {
  PP_RESOURCESTRING_PDFGETPASSWORD = 0,
} PP_ResourceString;

typedef struct _ppb_Private {
  // Returns a localized string.
  PP_Var (*GetLocalizedString)(PP_ResourceString string_id);
} PPB_Private;

#endif  // WEBKIT_GLUE_PLUGINS_PPB_PRIVATE_H_
