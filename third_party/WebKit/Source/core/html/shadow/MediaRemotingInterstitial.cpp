// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/html/shadow/MediaRemotingInterstitial.h"

#include "core/html/HTMLImageElement.h"
#include "core/html/HTMLVideoElement.h"
#include "core/html/shadow/MediaRemotingElements.h"

namespace blink {

MediaRemotingInterstitial::MediaRemotingInterstitial(
    HTMLVideoElement& videoElement)
    : HTMLDivElement(videoElement.GetDocument()),
      video_element_(&videoElement) {
  SetShadowPseudoId(AtomicString("-internal-media-remoting-interstitial"));
  background_image_ = HTMLImageElement::Create(videoElement.GetDocument());
  background_image_->SetShadowPseudoId(
      AtomicString("-internal-media-remoting-background-image"));
  background_image_->SetSrc(videoElement.getAttribute(HTMLNames::posterAttr));
  AppendChild(background_image_);

  cast_icon_ = new MediaRemotingCastIconElement(*this);
  AppendChild(cast_icon_);

  cast_text_message_ = new MediaRemotingCastMessageElement(*this);
  AppendChild(cast_text_message_);

  exit_button_ = new MediaRemotingExitButtonElement(*this);
  AppendChild(exit_button_);
}

void MediaRemotingInterstitial::Show() {
  RemoveInlineStyleProperty(CSSPropertyDisplay);
  exit_button_->OnShown();
}

void MediaRemotingInterstitial::Hide() {
  SetInlineStyleProperty(CSSPropertyDisplay, CSSValueNone);
  exit_button_->OnHidden();
}

void MediaRemotingInterstitial::OnPosterImageChanged() {
  background_image_->SetSrc(
      GetVideoElement().getAttribute(HTMLNames::posterAttr));
}

DEFINE_TRACE(MediaRemotingInterstitial) {
  visitor->Trace(video_element_);
  visitor->Trace(background_image_);
  visitor->Trace(exit_button_);
  visitor->Trace(cast_icon_);
  visitor->Trace(cast_text_message_);
  HTMLDivElement::Trace(visitor);
}

}  // namespace blink
