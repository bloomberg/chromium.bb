// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_FORM_DATA_H__
#define WEBKIT_GLUE_FORM_DATA_H__

#include <vector>

#include "googleurl/src/gurl.h"
#include "webkit/glue/form_field.h"

namespace webkit_glue {

// Holds information about a form to be filled and/or submitted.
struct FormData {
  // The name of the form.
  string16 name;
  // The URL (minus query parameters) containing the form
  GURL origin;
  // The action target of the form
  GURL action;
  // A vector of all the input fields in the form.
  std::vector<FormField> fields;
};

}  // namespace webkit_glue

#endif  // WEBKIT_GLUE_FORM_DATA_H__
