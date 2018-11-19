// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_CUSTOM_ELEMENT_INTERNALS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_CUSTOM_ELEMENT_INTERNALS_H_

#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"

namespace blink {

class HTMLElement;

class ElementInternals : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  ElementInternals(HTMLElement& target);
  void Trace(Visitor* visitor) override;

 private:
  Member<HTMLElement> target_;

  DISALLOW_COPY_AND_ASSIGN(ElementInternals);
};

}  // namespace blink
#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_CUSTOM_ELEMENT_INTERNALS_H_
