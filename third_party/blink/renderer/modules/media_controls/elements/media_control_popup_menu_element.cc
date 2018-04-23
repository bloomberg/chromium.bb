// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/media_controls/elements/media_control_popup_menu_element.h"

#include "third_party/blink/renderer/core/css/css_property_value_set.h"
#include "third_party/blink/renderer/core/css/css_style_declaration.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/dom/events/event_listener.h"
#include "third_party/blink/renderer/core/events/keyboard_event.h"
#include "third_party/blink/renderer/core/frame/dom_visual_viewport.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/geometry/dom_rect.h"
#include "third_party/blink/renderer/core/html/media/html_media_element.h"
#include "third_party/blink/renderer/modules/media_controls/elements/media_control_overflow_menu_button_element.h"
#include "third_party/blink/renderer/modules/media_controls/media_controls_impl.h"
#include "third_party/blink/renderer/platform/keyboard_codes.h"

namespace blink {

namespace {

// Focus the given item in the list if it is displayed. Returns whether it was
// focused.
bool FocusListItemIfDisplayed(Node* node) {
  Element* element = ToElement(node);

  if (!element->InlineStyle() ||
      !element->InlineStyle()->HasProperty(CSSPropertyDisplay)) {
    element->focus();
    return true;
  }

  return false;
}

}  // anonymous namespace

class MediaControlPopupMenuElement::KeyboardEventListener final
    : public EventListener {
 public:
  explicit KeyboardEventListener(MediaControlPopupMenuElement* popup_menu)
      : EventListener(kCPPEventListenerType), popup_menu_(popup_menu) {}

  ~KeyboardEventListener() final = default;

  bool operator==(const EventListener& other) const final {
    return &other == this;
  }

  void Trace(blink::Visitor* visitor) final {
    EventListener::Trace(visitor);
    visitor->Trace(popup_menu_);
  }

 private:
  void handleEvent(ExecutionContext*, Event* event) final {
    if (event->type() == EventTypeNames::keydown && event->IsKeyboardEvent()) {
      KeyboardEvent* keyboard_event = ToKeyboardEvent(event);
      bool handled = true;

      switch (keyboard_event->keyCode()) {
        case VKEY_TAB:
          keyboard_event->shiftKey() ? popup_menu_->SelectNextItem()
                                     : popup_menu_->SelectPreviousitem();
          break;
        case VKEY_UP:
          popup_menu_->SelectNextItem();
          break;
        case VKEY_DOWN:
          popup_menu_->SelectPreviousitem();
          break;
        case VKEY_ESCAPE:
          popup_menu_->CloseFromKeyboard();
          break;
        case VKEY_RETURN:
        case VKEY_SPACE:
          ToElement(event->target()->ToNode())->DispatchSimulatedClick(event);
          break;
        default:
          handled = false;
      }

      if (handled) {
        event->stopPropagation();
        event->SetDefaultHandled();
      }
    }
  }

  Member<MediaControlPopupMenuElement> popup_menu_;
};

MediaControlPopupMenuElement::~MediaControlPopupMenuElement() = default;

void MediaControlPopupMenuElement::SetIsWanted(bool wanted) {
  MediaControlDivElement::SetIsWanted(wanted);

  if (wanted) {
    SetPosition();

    SelectFirstItem();

    if (!keyboard_event_listener_) {
      keyboard_event_listener_ = new KeyboardEventListener(this);
      addEventListener(EventTypeNames::keydown, keyboard_event_listener_,
                       false);
    }
  }
}

void MediaControlPopupMenuElement::DefaultEventHandler(Event* event) {
  if (event->type() == EventTypeNames::pointermove)
    ToElement(event->target()->ToNode())->focus();

  MediaControlDivElement::DefaultEventHandler(event);
}

void MediaControlPopupMenuElement::RemovedFrom(ContainerNode* container) {
  if (keyboard_event_listener_) {
    removeEventListener(EventTypeNames::keydown, keyboard_event_listener_,
                        false);
    keyboard_event_listener_ = nullptr;
  }

  MediaControlDivElement::RemovedFrom(container);
}

void MediaControlPopupMenuElement::Trace(blink::Visitor* visitor) {
  MediaControlDivElement::Trace(visitor);
  visitor->Trace(keyboard_event_listener_);
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

  DOMRect* bounding_client_rect =
      EffectivePopupAnchor()->getBoundingClientRect();
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

Element* MediaControlPopupMenuElement::EffectivePopupAnchor() const {
  return MediaControlsImpl::IsModern() ? &GetMediaControls().OverflowButton()
                                       : PopupAnchor();
}

void MediaControlPopupMenuElement::SelectFirstItem() {
  for (Node* target = lastChild(); target; target = target->previousSibling()) {
    if (FocusListItemIfDisplayed(target))
      break;
  }
}

void MediaControlPopupMenuElement::SelectNextItem() {
  Element* focused_element = GetDocument().FocusedElement();
  if (!focused_element || focused_element->parentElement() != this)
    return;

  for (Node* target = focused_element->previousSibling(); target;
       target = target->previousSibling()) {
    if (FocusListItemIfDisplayed(target))
      break;
  }
}

void MediaControlPopupMenuElement::SelectPreviousitem() {
  Element* focused_element = GetDocument().FocusedElement();
  if (!focused_element || focused_element->parentElement() != this)
    return;

  for (Node* target = focused_element->nextSibling(); target;
       target = target->nextSibling()) {
    if (FocusListItemIfDisplayed(target))
      break;
  }
}

void MediaControlPopupMenuElement::CloseFromKeyboard() {
  SetIsWanted(false);
  EffectivePopupAnchor()->focus();
}

}  // namespace blink
