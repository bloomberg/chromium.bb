// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/media_controls/elements/MediaControlElementBase.h"

#include "core/html/HTMLMediaElement.h"
#include "core/layout/LayoutObject.h"
#include "modules/media_controls/MediaControlsImpl.h"
#include "platform/text/PlatformLocale.h"

namespace blink {

void MediaControlElementBase::SetIsWanted(bool wanted) {
  if (is_wanted_ == wanted)
    return;

  is_wanted_ = wanted;
  UpdateShownState();
}

bool MediaControlElementBase::IsWanted() const {
  return is_wanted_;
}

void MediaControlElementBase::SetDoesFit(bool fits) {
  does_fit_ = fits;
  UpdateShownState();
}

MediaControlElementType MediaControlElementBase::DisplayType() const {
  return display_type_;
}

bool MediaControlElementBase::HasOverflowButton() const {
  return false;
}

void MediaControlElementBase::ShouldShowButtonInOverflowMenu(bool should_show) {
  if (!HasOverflowButton())
    return;

  if (should_show) {
    overflow_menu_element_->RemoveInlineStyleProperty(CSSPropertyDisplay);
  } else {
    overflow_menu_element_->SetInlineStyleProperty(CSSPropertyDisplay,
                                                   CSSValueNone);
  }
}

String MediaControlElementBase::GetOverflowMenuString() const {
  return MediaElement().GetLocale().QueryString(GetOverflowStringName());
}

void MediaControlElementBase::UpdateOverflowString() {
  if (overflow_menu_element_ && overflow_menu_text_)
    overflow_menu_text_->ReplaceWholeText(GetOverflowMenuString());
}

MediaControlElementBase::MediaControlElementBase(
    MediaControlsImpl& media_controls,
    MediaControlElementType display_type,
    HTMLElement* element)
    : media_controls_(&media_controls),
      display_type_(display_type),
      element_(element),
      is_wanted_(true),
      does_fit_(true) {}

MediaControlsImpl& MediaControlElementBase::GetMediaControls() const {
  DCHECK(media_controls_);
  return *media_controls_;
}

HTMLMediaElement& MediaControlElementBase::MediaElement() const {
  return GetMediaControls().MediaElement();
}

void MediaControlElementBase::SetDisplayType(
    MediaControlElementType display_type) {
  if (display_type == display_type_)
    return;

  display_type_ = display_type;
  if (LayoutObject* object = element_->GetLayoutObject())
    object->SetShouldDoFullPaintInvalidation();
}

void MediaControlElementBase::UpdateShownState() {
  if (is_wanted_ && does_fit_)
    element_->RemoveInlineStyleProperty(CSSPropertyDisplay);
  else
    element_->SetInlineStyleProperty(CSSPropertyDisplay, CSSValueNone);
}

WebLocalizedString::Name MediaControlElementBase::GetOverflowStringName()
    const {
  NOTREACHED();
  return WebLocalizedString::kAXAMPMFieldText;
}

DEFINE_TRACE(MediaControlElementBase) {
  visitor->Trace(media_controls_);
  visitor->Trace(element_);
  visitor->Trace(overflow_menu_element_);
  visitor->Trace(overflow_menu_text_);
}

}  // namespace blink
