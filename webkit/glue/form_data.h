// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_FORM_DATA_H__
#define WEBKIT_GLUE_FORM_DATA_H__

#include <vector>

#include "googleurl/src/gurl.h"

// Holds information about a form to be filled and/or submitted.
struct FormData {
  // The name of the form.
  string16 name;
  // The URL (minus query parameters) containing the form
  GURL origin;
  // The action target of the form
  GURL action;
  // A vector of element labels.
  std::vector<string16> labels;
  // A vector of element names.
  std::vector<string16> elements;
  // A vector of element values.
  std::vector<string16> values;
  // The name of the submit button to be used to submit (optional).
  string16 submit;
};

#endif  // WEBKIT_GLUE_FORM_DATA_H__
