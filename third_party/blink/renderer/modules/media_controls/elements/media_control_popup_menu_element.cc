// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/media_controls/elements/media_control_popup_menu_element.h"

#include "third_party/blink/renderer/core/css/css_style_declaration.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/dom_visual_viewport.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/geometry/dom_rect.h"
#include "third_party/blink/renderer/core/html/media/html_media_element.h"
#include "third_party/blink/renderer/modules/media_controls/elements/media_control_overflow_menu_button_element.h"
#include "third_party/blink/renderer/modules/media_controls/media_controls_impl.h"

namespace blink {

void MediaControlPopupMenuElement::SetIsWanted(bool wanted) {
  MediaControlDivElement::SetIsWanted(wanted);

  if (wanted)
    SetPosition();
}

MediaControlPopupMenuElement::MediaControlPopupMenuElement(
    MediaControlsImpl& media_controls,
    MediaControlElementType type)
    : MediaControlDivElement(media_controls, type) {
  SetIsWanted(false);
}

void MediaControlPopupMenuElement::SetPosition() {
  // The popup is positioned slightly on the inside of the bottom right corner.
  static constexpr int kPopupMenuMarginPx = 4;
  static const char kImportant[] = "important";
  static const char kPx[] = "px";

  DCHECK(MediaElement().getBoundingClientRect());
  DCHECK(GetDocument().domWindow());
  DCHECK(GetDocument().domWindow()->visualViewport());

  Element* anchor_element = MediaControlsImpl::IsModern()
                                ? &GetMediaControls().OverflowButton()
                                : PopupAnchor();
  DOMRect* bounding_client_rect = anchor_element->getBoundingClientRect();
  DOMVisualViewport* viewport = GetDocument().domWindow()->visualViewport();

  WTF::String bottom_str_value = WTF::String::Number(
      viewport->height() - bounding_client_rect->bottom() + kPopupMenuMarginPx);
  WTF::String right_str_value = WTF::String::Number(
      viewport->width() - bounding_client_rect->right() + kPopupMenuMarginPx);

  bottom_str_value.append(kPx);
  right_str_value.append(kPx);

  style()->setProperty(&GetDocument(), "bottom", bottom_str_value, kImportant,
                       ASSERT_NO_EXCEPTION);
  style()->setProperty(&GetDocument(), "right", right_str_value, kImportant,
                       ASSERT_NO_EXCEPTION);
}

}  // namespace blink
