// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/media_controls/touchless/elements/media_controls_touchless_volume_container_element.h"

#include "third_party/blink/renderer/core/dom/dom_token_list.h"
#include "third_party/blink/renderer/core/html/media/html_media_element.h"
#include "third_party/blink/renderer/modules/media_controls/elements/media_control_elements_helper.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

namespace {

const char kMutedCSSClass[] = "muted";

}  // namespace

MediaControlsTouchlessVolumeContainerElement::
    MediaControlsTouchlessVolumeContainerElement(
        MediaControlsTouchlessImpl& media_controls)
    : MediaControlsTouchlessElement(media_controls) {
  SetShadowPseudoId(
      AtomicString("-internal-media-controls-touchless-volume-container"));

  Element* volume_bar_background = MediaControlElementsHelper::CreateDiv(
      "-internal-media-controls-touchless-volume-bar-background", this);
  volume_bar_ = MediaControlElementsHelper::CreateDiv(
      "-internal-media-controls-touchless-volume-bar", volume_bar_background);
  volume_icon_ = MediaControlElementsHelper::CreateDiv(
      "-internal-media-controls-touchless-volume-icon", this);
}

void MediaControlsTouchlessVolumeContainerElement::UpdateVolume() {
  volume_ = MediaElement().volume();
  UpdateCSSClass();
  UpdateVolumeBarCSS();
}

void MediaControlsTouchlessVolumeContainerElement::UpdateCSSClass() {
  if (volume_ > 0)
    volume_icon_->classList().Remove(kMutedCSSClass);
  else
    volume_icon_->classList().Add(kMutedCSSClass);
}

void MediaControlsTouchlessVolumeContainerElement::UpdateVolumeBarCSS() {
  StringBuilder builder;
  builder.Append("height:");
  builder.AppendNumber(volume_ * 100);
  builder.Append("%");
  volume_bar_->setAttribute("style", builder.ToAtomicString());
}

void MediaControlsTouchlessVolumeContainerElement::Trace(
    blink::Visitor* visitor) {
  visitor->Trace(volume_bar_);
  visitor->Trace(volume_icon_);
  MediaControlsTouchlessElement::Trace(visitor);
}

}  // namespace blink
