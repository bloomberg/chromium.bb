// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_CUSTOM_CUSTOM_ELEMENT_FORM_ASSOCIATED_CALLBACK_REACTION_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_CUSTOM_CUSTOM_ELEMENT_FORM_ASSOCIATED_CALLBACK_REACTION_H_

#include "base/macros.h"
#include "third_party/blink/renderer/core/html/custom/custom_element_reaction.h"

namespace blink {

class HTMLFormElement;

class CustomElementFormAssociatedCallbackReaction final
    : public CustomElementReaction {
 public:
  CustomElementFormAssociatedCallbackReaction(CustomElementDefinition*,
                                              HTMLFormElement* nullable_form);
  void Trace(Visitor*) override;

 private:
  void Invoke(Element*) override;

  Member<HTMLFormElement> form_;

  DISALLOW_COPY_AND_ASSIGN(CustomElementFormAssociatedCallbackReaction);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_CUSTOM_CUSTOM_ELEMENT_FORM_ASSOCIATED_CALLBACK_REACTION_H_
