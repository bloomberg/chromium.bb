/*
 * Copyright (C) 2005, 2008 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "core/editing/commands/RemoveCSSPropertyCommand.h"

#include "bindings/core/v8/ExceptionState.h"
#include "core/css/CSSStyleDeclaration.h"
#include "core/css/StylePropertySet.h"
#include "core/dom/Element.h"
#include "platform/wtf/Assertions.h"

namespace blink {

RemoveCSSPropertyCommand::RemoveCSSPropertyCommand(Document& document,
                                                   Element* element,
                                                   CSSPropertyID property)
    : SimpleEditCommand(document),
      element_(element),
      property_(property),
      important_(false) {
  DCHECK(element_);
}

RemoveCSSPropertyCommand::~RemoveCSSPropertyCommand() {}

void RemoveCSSPropertyCommand::DoApply(EditingState*) {
  const StylePropertySet* style = element_->InlineStyle();
  if (!style)
    return;

  old_value_ = style->GetPropertyValue(property_);
  important_ = style->PropertyIsImportant(property_);

  // Mutate using the CSSOM wrapper so we get the same event behavior as a
  // script. Setting to null string removes the property. We don't have internal
  // version of removeProperty.
  element_->style()->SetPropertyInternal(property_, String(), String(), false,
                                         IGNORE_EXCEPTION_FOR_TESTING);
}

void RemoveCSSPropertyCommand::DoUnapply() {
  element_->style()->SetPropertyInternal(property_, String(), old_value_,
                                         important_,
                                         IGNORE_EXCEPTION_FOR_TESTING);
}

void RemoveCSSPropertyCommand::Trace(blink::Visitor* visitor) {
  visitor->Trace(element_);
  SimpleEditCommand::Trace(visitor);
}

}  // namespace blink
