// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/glue/form_field.h"

namespace webkit_glue {

FormField::FormField()
  : element_(NULL) {
}

FormField::FormField(WebCore::HTMLFormControlElement* element,
                     const string16& name,
                     const string16& value)
  : element_(element),
    name_(name),
    value_(value) {
}

}  // namespace webkit_glue
