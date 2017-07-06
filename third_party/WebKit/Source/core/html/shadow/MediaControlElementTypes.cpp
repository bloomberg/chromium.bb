/*
 * Copyright (C) 2008, 2009, 2010, 2011 Apple Inc. All rights reserved.
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "core/html/shadow/MediaControlElementTypes.h"

#include "bindings/core/v8/ExceptionState.h"
#include "core/CSSValueKeywords.h"
#include "core/HTMLNames.h"
#include "core/css/StylePropertySet.h"
#include "core/dom/Text.h"
#include "core/events/MouseEvent.h"
#include "core/html/HTMLLabelElement.h"
#include "core/html/HTMLMediaElement.h"
#include "core/html/media/MediaControls.h"
#include "core/layout/LayoutObject.h"
#include "platform/text/PlatformLocale.h"

namespace blink {

using namespace HTMLNames;

class Event;

const HTMLMediaElement* ToParentMediaElement(const Node* node) {
  if (!node)
    return nullptr;
  const Node* media_node = node->OwnerShadowHost();
  if (!media_node)
    return nullptr;
  if (!IsHTMLMediaElement(media_node))
    return nullptr;

  return ToHTMLMediaElement(media_node);
}

const HTMLMediaElement* ToParentMediaElement(
    const LayoutObject& layout_object) {
  return ToParentMediaElement(layout_object.GetNode());
}

MediaControlElementType GetMediaControlElementType(const Node* node) {
  SECURITY_DCHECK(node->IsMediaControlElement());
  const HTMLElement* element = ToHTMLElement(node);
  if (isHTMLInputElement(*element))
    return static_cast<const MediaControlInputElement*>(element)->DisplayType();
  return static_cast<const MediaControlDivElement*>(element)->DisplayType();
}

MediaControlElement::MediaControlElement(MediaControls& media_controls,
                                         MediaControlElementType display_type,
                                         HTMLElement* element)
    : media_controls_(&media_controls),
      display_type_(display_type),
      element_(element),
      is_wanted_(true),
      does_fit_(true) {}

HTMLMediaElement& MediaControlElement::MediaElement() const {
  return GetMediaControls().MediaElement();
}

void MediaControlElement::UpdateShownState() {
  if (is_wanted_ && does_fit_)
    element_->RemoveInlineStyleProperty(CSSPropertyDisplay);
  else
    element_->SetInlineStyleProperty(CSSPropertyDisplay, CSSValueNone);
}

void MediaControlElement::SetDoesFit(bool fits) {
  does_fit_ = fits;
  UpdateShownState();
}

void MediaControlElement::SetIsWanted(bool wanted) {
  if (is_wanted_ == wanted)
    return;
  is_wanted_ = wanted;
  UpdateShownState();
}

bool MediaControlElement::IsWanted() {
  return is_wanted_;
}

void MediaControlElement::SetDisplayType(MediaControlElementType display_type) {
  if (display_type == display_type_)
    return;

  display_type_ = display_type;
  if (LayoutObject* object = element_->GetLayoutObject())
    object->SetShouldDoFullPaintInvalidation();
}

void MediaControlElement::ShouldShowButtonInOverflowMenu(bool should_show) {
  if (!HasOverflowButton())
    return;
  if (should_show) {
    overflow_menu_element_->RemoveInlineStyleProperty(CSSPropertyDisplay);
  } else {
    overflow_menu_element_->SetInlineStyleProperty(CSSPropertyDisplay,
                                                   CSSValueNone);
  }
}

String MediaControlElement::GetOverflowMenuString() {
  return MediaElement().GetLocale().QueryString(GetOverflowStringName());
}

void MediaControlElement::UpdateOverflowString() {
  if (overflow_menu_element_ && overflow_menu_text_)
    overflow_menu_text_->ReplaceWholeText(GetOverflowMenuString());
}

DEFINE_TRACE(MediaControlElement) {
  visitor->Trace(media_controls_);
  visitor->Trace(element_);
  visitor->Trace(overflow_menu_element_);
  visitor->Trace(overflow_menu_text_);
}

// ----------------------------

MediaControlDivElement::MediaControlDivElement(
    MediaControls& media_controls,
    MediaControlElementType display_type)
    : HTMLDivElement(media_controls.OwnerDocument()),
      MediaControlElement(media_controls, display_type, this) {}

DEFINE_TRACE(MediaControlDivElement) {
  MediaControlElement::Trace(visitor);
  HTMLDivElement::Trace(visitor);
}

// ----------------------------

MediaControlInputElement::MediaControlInputElement(
    MediaControls& media_controls,
    MediaControlElementType display_type)
    : HTMLInputElement(media_controls.OwnerDocument(), false),
      MediaControlElement(media_controls, display_type, this) {}

bool MediaControlInputElement::IsMouseFocusable() const {
  return false;
}

HTMLElement* MediaControlInputElement::CreateOverflowElement(
    MediaControls& media_controls,
    MediaControlInputElement* button) {
  if (!button)
    return nullptr;

  // We don't want the button visible within the overflow menu.
  button->SetIsWanted(false);

  overflow_menu_text_ = Text::Create(media_controls.OwnerDocument(),
                                     button->GetOverflowMenuString());

  HTMLLabelElement* element =
      HTMLLabelElement::Create(media_controls.OwnerDocument());
  element->SetShadowPseudoId(
      AtomicString("-internal-media-controls-overflow-menu-list-item"));
  // Appending a button to a label element ensures that clicks on the label
  // are passed down to the button, performing the action we'd expect.
  element->AppendChild(button);
  element->AppendChild(overflow_menu_text_);
  overflow_menu_element_ = element;
  return element;
}

DEFINE_TRACE(MediaControlInputElement) {
  MediaControlElement::Trace(visitor);
  HTMLInputElement::Trace(visitor);
}

}  // namespace blink
