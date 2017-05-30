// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HTMLIFrameElementAllow_h
#define HTMLIFrameElementAllow_h

#include "core/CoreExport.h"
#include "core/dom/DOMTokenList.h"
#include "platform/heap/Handle.h"
#include "public/platform/WebFeaturePolicy.h"

namespace blink {

class HTMLIFrameElement;

class CORE_EXPORT HTMLIFrameElementAllow final : public DOMTokenList {
 public:
  static HTMLIFrameElementAllow* Create(HTMLIFrameElement* element) {
    return new HTMLIFrameElementAllow(element);
  }

  // Returns unique set of valid feature names.
  Vector<WebFeaturePolicyFeature> ParseAllowedFeatureNames(
      String& invalid_tokens_error_message) const;

 private:
  explicit HTMLIFrameElementAllow(HTMLIFrameElement*);
  bool ValidateTokenValue(const AtomicString&, ExceptionState&) const override;
};

}  // namespace blink

#endif  // HTMLIFrameElementAllow_h
