// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/html/media/PictureInPictureInterstitial.h"

#include "bindings/core/v8/ExceptionState.h"
#include "core/dom/Document.h"
#include "core/html/HTMLImageElement.h"
#include "core/html/media/HTMLVideoElement.h"
#include "platform/text/PlatformLocale.h"
#include "public/platform/WebLocalizedString.h"
#include "third_party/WebKit/public/platform/WebLayer.h"

namespace {

constexpr double kPictureInPictureStyleChangeTransSeconds = 0.2;
constexpr double kPictureInPictureHiddenAnimationSeconds = 0.3;

}  // namespace

namespace blink {

PictureInPictureInterstitial::PictureInPictureInterstitial(
    HTMLVideoElement& videoElement)
    : HTMLDivElement(videoElement.GetDocument()),
      interstitial_timer_(
          videoElement.GetDocument().GetTaskRunner(TaskType::kUnthrottled),
          this,
          &PictureInPictureInterstitial::ToggleInterstitialTimerFired),
      video_element_(&videoElement) {
  SetShadowPseudoId(AtomicString("-internal-media-interstitial"));
  background_image_ = HTMLImageElement::Create(GetDocument());
  background_image_->SetShadowPseudoId(
      AtomicString("-internal-media-interstitial-background-image"));
  background_image_->SetSrc(videoElement.getAttribute(HTMLNames::posterAttr));
  AppendChild(background_image_);

  pip_icon_ = HTMLDivElement::Create(GetDocument());
  pip_icon_->SetShadowPseudoId(
      AtomicString("-internal-picture-in-picture-icon"));
  AppendChild(pip_icon_);

  HTMLDivElement* message_element_ = HTMLDivElement::Create(GetDocument());
  message_element_->SetShadowPseudoId(
      AtomicString("-internal-media-interstitial-message"));
  message_element_->setInnerText(
      GetVideoElement().GetLocale().QueryString(
          WebLocalizedString::kPictureInPictureInterstitialText),
      ASSERT_NO_EXCEPTION);
  AppendChild(message_element_);
}

void PictureInPictureInterstitial::Show() {
  if (should_be_visible_)
    return;

  if (interstitial_timer_.IsActive())
    interstitial_timer_.Stop();
  should_be_visible_ = true;
  RemoveInlineStyleProperty(CSSPropertyDisplay);
  interstitial_timer_.StartOneShot(kPictureInPictureStyleChangeTransSeconds,
                                   FROM_HERE);

  DCHECK(GetVideoElement().PlatformLayer());
  GetVideoElement().PlatformLayer()->SetDrawsContent(false);
}

void PictureInPictureInterstitial::Hide() {
  if (!should_be_visible_)
    return;

  if (interstitial_timer_.IsActive())
    interstitial_timer_.Stop();
  should_be_visible_ = false;
  SetInlineStyleProperty(CSSPropertyOpacity, 0,
                         CSSPrimitiveValue::UnitType::kNumber);
  interstitial_timer_.StartOneShot(kPictureInPictureHiddenAnimationSeconds,
                                   FROM_HERE);

  DCHECK(GetVideoElement().PlatformLayer());
  GetVideoElement().PlatformLayer()->SetDrawsContent(true);
}

void PictureInPictureInterstitial::ToggleInterstitialTimerFired(TimerBase*) {
  interstitial_timer_.Stop();
  if (should_be_visible_) {
    SetInlineStyleProperty(CSSPropertyBackgroundColor, CSSValueBlack);
    SetInlineStyleProperty(CSSPropertyOpacity, 1,
                           CSSPrimitiveValue::UnitType::kNumber);
  } else {
    SetInlineStyleProperty(CSSPropertyDisplay, CSSValueNone);
  }
}

void PictureInPictureInterstitial::OnPosterImageChanged() {
  background_image_->SetSrc(
      GetVideoElement().getAttribute(HTMLNames::posterAttr));
}

void PictureInPictureInterstitial::Trace(blink::Visitor* visitor) {
  visitor->Trace(video_element_);
  visitor->Trace(background_image_);
  visitor->Trace(pip_icon_);
  visitor->Trace(message_element_);
  HTMLDivElement::Trace(visitor);
}

}  // namespace blink
