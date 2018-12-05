// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_CUSTOM_CUSTOM_ELEMENT_DISABLED_STATE_CHANGED_CALLBACK_REACTION_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_CUSTOM_CUSTOM_ELEMENT_DISABLED_STATE_CHANGED_CALLBACK_REACTION_H_

#include "base/macros.h"
#include "third_party/blink/renderer/core/html/custom/custom_element_reaction.h"

namespace blink {

class CustomElementDisabledStateChangedCallbackReaction final
    : public CustomElementReaction {
 public:
  CustomElementDisabledStateChangedCallbackReaction(CustomElementDefinition*,
                                                    bool is_disabled);

 private:
  void Invoke(Element*) override;

  bool is_disabled_;

  DISALLOW_COPY_AND_ASSIGN(CustomElementDisabledStateChangedCallbackReaction);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_CUSTOM_CUSTOM_ELEMENT_DISABLED_STATE_CHANGED_CALLBACK_REACTION_H_
