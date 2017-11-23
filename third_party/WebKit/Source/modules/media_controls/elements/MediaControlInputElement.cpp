// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/media_controls/elements/MediaControlInputElement.h"

#include "core/dom/DOMTokenList.h"
#include "core/dom/events/Event.h"
#include "core/html/forms/HTMLLabelElement.h"
#include "core/html/media/HTMLMediaElement.h"
#include "modules/media_controls/MediaControlsImpl.h"
#include "modules/media_controls/elements/MediaControlElementsHelper.h"
#include "platform/Histogram.h"
#include "platform/runtime_enabled_features.h"
#include "platform/text/PlatformLocale.h"
#include "public/platform/WebSize.h"

namespace {

// The default size of an overflow button in pixels.
constexpr int kDefaultButtonSize = 36;

}  // namespace

namespace blink {

// static
bool MediaControlInputElement::ShouldRecordDisplayStates(
    const HTMLMediaElement& media_element) {
  // Only record when the metadat are available so that the display state of the
  // buttons are fairly stable. For example, before metadata are available, the
  // size of the element might differ, it's unknown if the file has an audio
  // track, etc.
  if (media_element.getReadyState() >= HTMLMediaElement::kHaveMetadata)
    return true;

  // When metadata are not available, only record the display state if the
  // element will require a user gesture in order to load.
  if (media_element.EffectivePreloadType() ==
      WebMediaPlayer::Preload::kPreloadNone) {
    return true;
  }

  return false;
}

HTMLElement* MediaControlInputElement::CreateOverflowElement(
    MediaControlInputElement* button) {
  if (!button)
    return nullptr;

  // We don't want the button visible within the overflow menu.
  button->SetInlineStyleProperty(CSSPropertyDisplay, CSSValueNone);

  overflow_menu_text_ =
      Text::Create(GetDocument(), button->GetOverflowMenuString());

  HTMLLabelElement* element = HTMLLabelElement::Create(GetDocument());
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

void MediaControlInputElement::MaybeRecordDisplayed() {
  // Display is defined as wanted and fitting. Overflow elements will only be
  // displayed if their inline counterpart isn't displayed.
  if (!IsWanted() || !DoesFit()) {
    if (IsWanted() && overflow_element_)
      overflow_element_->MaybeRecordDisplayed();
    return;
  }

  // Keep this check after the block above because `display_recorded_` might be
  // true for the inline element but not for the overflow one.
  if (display_recorded_)
    return;

  RecordCTREvent(CTREvent::kDisplayed);
  display_recorded_ = true;
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
      MediaControlElementBase(media_controls, display_type, this) {
  CreateUserAgentShadowRoot();
  CreateShadowSubtree();
}

WebLocalizedString::Name MediaControlInputElement::GetOverflowStringName()
    const {
  NOTREACHED();
  return WebLocalizedString::kAXAMPMFieldText;
}

void MediaControlInputElement::UpdateShownState() {
  if (is_overflow_element_) {
    Element* parent = parentElement();
    DCHECK(parent);
    DCHECK(IsHTMLLabelElement(parent));

    if (IsWanted() && DoesFit())
      parent->RemoveInlineStyleProperty(CSSPropertyDisplay);
    else
      parent->SetInlineStyleProperty(CSSPropertyDisplay, CSSValueNone);

    // Don't update the shown state of the element if we want to hide
    // icons on the overflow menu.
    if (!RuntimeEnabledFeatures::OverflowIconsForMediaControlsEnabled())
      return;
  }

  MediaControlElementBase::UpdateShownState();
}

void MediaControlInputElement::DefaultEventHandler(Event* event) {
  if (event->type() == EventTypeNames::click) {
    if (IsOverflowElement())
      GetMediaControls().MaybeRecordOverflowTimeToAction();
    MaybeRecordInteracted();
  }

  HTMLInputElement::DefaultEventHandler(event);
}

void MediaControlInputElement::MaybeRecordInteracted() {
  if (interaction_recorded_)
    return;

  if (!display_recorded_) {
    // The only valid reason to not have the display recorded at this point is
    // if it wasn't allowed. Regardless, the display will now be recorded.
    DCHECK(!ShouldRecordDisplayStates(MediaElement()));
    RecordCTREvent(CTREvent::kDisplayed);
  }

  RecordCTREvent(CTREvent::kInteracted);
  interaction_recorded_ = true;
}

bool MediaControlInputElement::IsOverflowElement() const {
  return is_overflow_element_;
}

bool MediaControlInputElement::IsMouseFocusable() const {
  return false;
}

bool MediaControlInputElement::IsMediaControlElement() const {
  return true;
}

String MediaControlInputElement::GetOverflowMenuString() const {
  return MediaElement().GetLocale().QueryString(GetOverflowStringName());
}

void MediaControlInputElement::RecordCTREvent(CTREvent event) {
  String histogram_name("Media.Controls.CTR.");
  histogram_name.append(GetNameForHistograms());
  EnumerationHistogram ctr_histogram(histogram_name.Ascii().data(),
                                     static_cast<int>(CTREvent::kCount));
  ctr_histogram.Count(static_cast<int>(event));
}

void MediaControlInputElement::SetClass(const AtomicString& class_name,
                                        bool should_have_class) {
  if (should_have_class)
    classList().Add(class_name);
  else
    classList().Remove(class_name);
}

void MediaControlInputElement::UpdateDisplayType() {
  if (overflow_element_)
    overflow_element_->UpdateDisplayType();
}

WebSize MediaControlInputElement::GetSizeOrDefault() const {
  if (HasOverflowButton()) {
    // If this has an overflow button then it is a button control and therefore
    // has a default size of kDefaultButtonSize.
    return MediaControlElementsHelper::GetSizeOrDefault(
        *this, WebSize(kDefaultButtonSize, kDefaultButtonSize));
  }
  return MediaControlElementsHelper::GetSizeOrDefault(*this, WebSize(0, 0));
}

void MediaControlInputElement::Trace(blink::Visitor* visitor) {
  HTMLInputElement::Trace(visitor);
  MediaControlElementBase::Trace(visitor);
  visitor->Trace(overflow_element_);
  visitor->Trace(overflow_menu_text_);
}

}  // namespace blink
