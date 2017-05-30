// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef RelList_h
#define RelList_h

#include "core/dom/DOMTokenList.h"

namespace blink {

class RelList final : public DOMTokenList {
 public:
  static RelList* Create(Element* element) { return new RelList(element); }

 private:
  explicit RelList(Element*);
  bool ValidateTokenValue(const AtomicString&, ExceptionState&) const override;
};

}  // namespace blink

#endif  // RelList_h
