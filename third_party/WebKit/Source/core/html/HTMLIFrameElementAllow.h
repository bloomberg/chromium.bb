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

class CORE_EXPORT HTMLIFrameElementAllow final : public DOMTokenList,
                                                 public DOMTokenListObserver {
  USING_GARBAGE_COLLECTED_MIXIN(HTMLIFrameElementAllow);

 public:
  static HTMLIFrameElementAllow* create(HTMLIFrameElement* element) {
    return new HTMLIFrameElementAllow(element);
  }

  ~HTMLIFrameElementAllow() override;

  // Returns unique set of valid feature names.
  Vector<WebFeaturePolicyFeature> parseAllowedFeatureNames(
      String& invalidTokensErrorMessage) const;

  DECLARE_VIRTUAL_TRACE();

 private:
  explicit HTMLIFrameElementAllow(HTMLIFrameElement*);
  bool validateTokenValue(const AtomicString&, ExceptionState&) const override;

  // DOMTokenListObserver.
  void valueWasSet() override;

  Member<HTMLIFrameElement> m_element;
};

}  // namespace blink

#endif  // HTMLIFrameElementAllow_h
