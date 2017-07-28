/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 * Copyright (C) 2011 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "core/html/forms/PasswordInputType.h"

#include "core/InputTypeNames.h"
#include "core/dom/Document.h"
#include "core/editing/FrameSelection.h"
#include "core/frame/LocalFrame.h"
#include "core/html/HTMLInputElement.h"
#include "core/html/forms/FormController.h"
#include "core/layout/LayoutTextControlSingleLine.h"
#include "platform/wtf/Assertions.h"
#include "platform/wtf/RefPtr.h"

namespace blink {

InputType* PasswordInputType::Create(HTMLInputElement& element) {
  return new PasswordInputType(element);
}

void PasswordInputType::CountUsage() {
  CountUsageIfVisible(WebFeature::kInputTypePassword);
  if (GetElement().FastHasAttribute(HTMLNames::maxlengthAttr))
    CountUsageIfVisible(WebFeature::kInputTypePasswordMaxLength);
}

const AtomicString& PasswordInputType::FormControlType() const {
  return InputTypeNames::password;
}

bool PasswordInputType::ShouldSaveAndRestoreFormControlState() const {
  return false;
}

FormControlState PasswordInputType::SaveFormControlState() const {
  // Should never save/restore password fields.
  NOTREACHED();
  return FormControlState();
}

void PasswordInputType::RestoreFormControlState(const FormControlState&) {
  // Should never save/restore password fields.
  NOTREACHED();
}

bool PasswordInputType::ShouldRespectListAttribute() {
  return false;
}

void PasswordInputType::EnableSecureTextInput() {
  LocalFrame* frame = GetElement().GetDocument().GetFrame();
  if (!frame)
    return;
  frame->Selection().SetUseSecureKeyboardEntryWhenActive(true);
}

void PasswordInputType::DisableSecureTextInput() {
  LocalFrame* frame = GetElement().GetDocument().GetFrame();
  if (!frame)
    return;
  frame->Selection().SetUseSecureKeyboardEntryWhenActive(false);
}

void PasswordInputType::OnAttachWithLayoutObject() {
  GetElement().GetDocument().IncrementPasswordCount();
}

void PasswordInputType::OnDetachWithLayoutObject() {
  GetElement().GetDocument().DecrementPasswordCount();
}

}  // namespace blink
