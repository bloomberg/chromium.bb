// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/media_controls/elements/MediaControlInputElement.h"

#include "core/events/Event.h"
#include "core/html/HTMLLabelElement.h"
#include "core/html/HTMLMediaElement.h"
#include "modules/media_controls/MediaControlsImpl.h"
#include "platform/text/PlatformLocale.h"

namespace blink {

HTMLElement* MediaControlInputElement::CreateOverflowElement(
    MediaControlsImpl& media_controls,
    MediaControlInputElement* button) {
  if (!button)
    return nullptr;

  // We don't want the button visible within the overflow menu.
  button->SetInlineStyleProperty(CSSPropertyDisplay, CSSValueNone);

  overflow_menu_text_ = Text::Create(media_controls.GetDocument(),
                                     button->GetOverflowMenuString());

  HTMLLabelElement* element =
      HTMLLabelElement::Create(media_controls.GetDocument());
  element->SetShadowPseudoId(
      AtomicString("-internal-media-controls-overflow-menu-list-item"));
  // Appending a button to a label element ensures that clicks on the label
  // are passed down to the button, performing the action we'd expect.
  element->AppendChild(button);
  element->AppendChild(overflow_menu_text_);

  // Initialize the internal states of the main element and the overflow one.
  button->is_overflow_element_ = true;
  overflow_element_ = button;

  // Keeping the element hidden by default. This is setting the style in
  // addition of calling ShouldShowButtonInOverflowMenu() to guarantee that the
  // internal state matches the CSS state.
  element->SetInlineStyleProperty(CSSPropertyDisplay, CSSValueNone);
  SetOverflowElementIsWanted(false);

  return element;
}

void MediaControlInputElement::SetOverflowElementIsWanted(bool wanted) {
  if (!overflow_element_)
    return;
  overflow_element_->SetIsWanted(wanted);
}

void MediaControlInputElement::UpdateOverflowString() {
  if (!overflow_menu_text_)
    return;

  DCHECK(overflow_element_);
  overflow_menu_text_->ReplaceWholeText(GetOverflowMenuString());
}

MediaControlInputElement::MediaControlInputElement(
    MediaControlsImpl& media_controls,
    MediaControlElementType display_type)
    : HTMLInputElement(media_controls.GetDocument(), false),
      MediaControlElementBase(media_controls, display_type, this) {}

WebLocalizedString::Name MediaControlInputElement::GetOverflowStringName()
    const {
  NOTREACHED();
  return WebLocalizedString::kAXAMPMFieldText;
}

void MediaControlInputElement::UpdateShownState() {
  if (is_overflow_element_) {
    Element* parent = parentElement();
    DCHECK(parent);
    DCHECK(isHTMLLabelElement(parent));

    if (IsWanted() && DoesFit())
      parent->RemoveInlineStyleProperty(CSSPropertyDisplay);
    else
      parent->SetInlineStyleProperty(CSSPropertyDisplay, CSSValueNone);

    return;
  }

  MediaControlElementBase::UpdateShownState();
}

void MediaControlInputElement::UpdateDisplayType() {}

bool MediaControlInputElement::IsMouseFocusable() const {
  return false;
}

bool MediaControlInputElement::IsMediaControlElement() const {
  return true;
}

String MediaControlInputElement::GetOverflowMenuString() const {
  return MediaElement().GetLocale().QueryString(GetOverflowStringName());
}

DEFINE_TRACE(MediaControlInputElement) {
  HTMLInputElement::Trace(visitor);
  MediaControlElementBase::Trace(visitor);
  visitor->Trace(overflow_element_);
  visitor->Trace(overflow_menu_text_);
}

}  // namespace blink
