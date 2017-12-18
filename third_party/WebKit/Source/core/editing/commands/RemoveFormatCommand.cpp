/*
 * Copyright (C) 2007 Apple Computer, Inc.  All rights reserved.
 * Copyright (C) 2010 Google Inc. All rights reserved.
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

#include "core/editing/commands/RemoveFormatCommand.h"

#include "core/CSSValueKeywords.h"
#include "core/css/CSSPropertyValueSet.h"
#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "core/editing/EditingStyle.h"
#include "core/editing/FrameSelection.h"
#include "core/editing/VisibleSelection.h"
#include "core/editing/commands/ApplyStyleCommand.h"
#include "core/frame/LocalFrame.h"
#include "core/html_names.h"

namespace blink {

using namespace HTMLNames;

RemoveFormatCommand::RemoveFormatCommand(Document& document)
    : CompositeEditCommand(document) {}

static bool IsElementForRemoveFormatCommand(const Element* element) {
  DEFINE_STATIC_LOCAL(
      HashSet<QualifiedName>, elements,
      ({
          acronymTag, bTag,   bdoTag,  bigTag,  citeTag,  codeTag,
          dfnTag,     emTag,  fontTag, iTag,    insTag,   kbdTag,
          nobrTag,    qTag,   sTag,    sampTag, smallTag, strikeTag,
          strongTag,  subTag, supTag,  ttTag,   uTag,     varTag,
      }));
  return elements.Contains(element->TagQName());
}

void RemoveFormatCommand::DoApply(EditingState* editing_state) {
  DCHECK(!GetDocument().NeedsLayoutTreeUpdate());

  // TODO(editing-dev): Stop accessing FrameSelection in edit commands.
  LocalFrame* frame = GetDocument().GetFrame();
  const VisibleSelection selection =
      frame->Selection().ComputeVisibleSelectionInDOMTree();
  if (selection.IsNone() || !selection.IsValidFor(GetDocument()))
    return;

  // Get the default style for this editable root, it's the style that we'll
  // give the content that we're operating on.
  Element* root = selection.RootEditableElement();
  EditingStyle* default_style = EditingStyle::Create(root);

  // We want to remove everything but transparent background.
  // FIXME: We shouldn't access style().
  default_style->Style()->SetProperty(CSSPropertyBackgroundColor,
                                      CSSValueTransparent);

  ApplyCommandToComposite(ApplyStyleCommand::Create(
                              GetDocument(), default_style,
                              IsElementForRemoveFormatCommand, GetInputType()),
                          editing_state);
}

InputEvent::InputType RemoveFormatCommand::GetInputType() const {
  return InputEvent::InputType::kFormatRemove;
}

}  // namespace blink
