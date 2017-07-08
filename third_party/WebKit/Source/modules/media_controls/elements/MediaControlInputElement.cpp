// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/media_controls/elements/MediaControlInputElement.h"

#include "core/html/HTMLLabelElement.h"
#include "modules/media_controls/MediaControlsImpl.h"

namespace blink {

HTMLElement* MediaControlInputElement::CreateOverflowElement(
    MediaControlsImpl& media_controls,
    MediaControlInputElement* button) {
  if (!button)
    return nullptr;

  // We don't want the button visible within the overflow menu.
  button->SetIsWanted(false);

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
  overflow_menu_element_ = element;
  return element;
}

MediaControlInputElement::MediaControlInputElement(
    MediaControlsImpl& media_controls,
    MediaControlElementType display_type)
    : HTMLInputElement(media_controls.GetDocument(), false),
      MediaControlElementBase(media_controls, display_type, this) {}

void MediaControlInputElement::UpdateDisplayType() {}

bool MediaControlInputElement::IsMediaControlElement() const {
  return true;
}

bool MediaControlInputElement::IsMouseFocusable() const {
  return false;
}

DEFINE_TRACE(MediaControlInputElement) {
  HTMLInputElement::Trace(visitor);
  MediaControlElementBase::Trace(visitor);
}

}  // namespace blink
