// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FormAssociated_h
#define FormAssociated_h

namespace blink {

class HTMLFormElement;

// Contains code to associate form with a form associated element
// https://html.spec.whatwg.org/multipage/forms.html#form-associated-element
class FormAssociated {
 public:
  // HTMLFormElement can be null
  virtual void AssociateWith(HTMLFormElement*) = 0;
};

}  // namespace blink

#endif  // FormAssociated_h
