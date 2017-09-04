/*
 * Copyright (C) 2006, 2007 Apple, Inc.  All rights reserved.
 * Copyright (C) 2012 Google, Inc.  All rights reserved.
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

#include "core/editing/spellcheck/SpellCheckerClientImpl.h"

#include "core/dom/Element.h"
#include "core/editing/FrameSelection.h"
#include "core/editing/markers/DocumentMarkerController.h"
#include "core/editing/spellcheck/SpellChecker.h"
#include "core/exported/WebViewImpl.h"
#include "core/frame/LocalFrame.h"
#include "core/page/Page.h"

namespace blink {

SpellCheckerClientImpl::SpellCheckerClientImpl(WebViewImpl* webview)
    : web_view_(webview),
      spell_check_this_field_status_(kSpellCheckAutomatic) {}

SpellCheckerClientImpl::~SpellCheckerClientImpl() {}

bool SpellCheckerClientImpl::ShouldSpellcheckByDefault() {
  // Spellcheck should be enabled for all editable areas (such as textareas,
  // contentEditable regions, designMode docs and inputs).
  if (!web_view_->FocusedCoreFrame()->IsLocalFrame())
    return false;
  const LocalFrame* frame = ToLocalFrame(web_view_->FocusedCoreFrame());
  if (!frame)
    return false;
  if (frame->GetSpellChecker().IsSpellCheckingEnabledInFocusedNode())
    return true;
  const Document* document = frame->GetDocument();
  if (!document)
    return false;
  const Element* element = document->FocusedElement();
  // If |element| is null, we default to allowing spellchecking. This is done
  // in order to mitigate the issue when the user clicks outside the textbox,
  // as a result of which |element| becomes null, resulting in all the spell
  // check markers being deleted. Also, the LocalFrame will decide not to do
  // spellchecking if the user can't edit - so returning true here will not
  // cause any problems to the LocalFrame's behavior.
  if (!element)
    return true;
  const LayoutObject* layout_object = element->GetLayoutObject();
  if (!layout_object)
    return false;

  return true;
}

bool SpellCheckerClientImpl::IsSpellCheckingEnabled() {
  if (spell_check_this_field_status_ == kSpellCheckForcedOff)
    return false;
  if (spell_check_this_field_status_ == kSpellCheckForcedOn)
    return true;
  return ShouldSpellcheckByDefault();
}

void SpellCheckerClientImpl::ToggleSpellCheckingEnabled() {
  if (IsSpellCheckingEnabled()) {
    spell_check_this_field_status_ = kSpellCheckForcedOff;
    Page* const page = web_view_->GetPage();
    if (!page)
      return;
    for (Frame* frame = page->MainFrame(); frame;
         frame = frame->Tree().TraverseNext()) {
      if (!frame->IsLocalFrame())
        continue;
      ToLocalFrame(frame)->GetDocument()->Markers().RemoveMarkersOfTypes(
          DocumentMarker::MisspellingMarkers());
    }
    return;
  }
  spell_check_this_field_status_ = kSpellCheckForcedOn;
}

}  // namespace blink
