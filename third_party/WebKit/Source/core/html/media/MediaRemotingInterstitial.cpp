// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/html/media/MediaRemotingInterstitial.h"

#include "core/dom/TaskRunnerHelper.h"
#include "core/html/HTMLImageElement.h"
#include "core/html/HTMLVideoElement.h"
#include "core/html/media/MediaRemotingElements.h"
#include "platform/text/PlatformLocale.h"
#include "public/platform/WebLocalizedString.h"

namespace {

constexpr double kStyleChangeTransSeconds = 0.2;
constexpr double kHiddenAnimationSeconds = 0.3;

}  // namespace

namespace blink {

MediaRemotingInterstitial::MediaRemotingInterstitial(
    HTMLVideoElement& videoElement)
    : HTMLDivElement(videoElement.GetDocument()),
      toggle_insterstitial_timer_(
          TaskRunnerHelper::Get(TaskType::kUnthrottled,
                                &videoElement.GetDocument()),
          this,
          &MediaRemotingInterstitial::ToggleInterstitialTimerFired),
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

void MediaRemotingInterstitial::Show(
    const WebString& remote_device_friendly_name) {
  DCHECK(!should_be_visible_);
  if (remote_device_friendly_name.IsEmpty()) {
    cast_text_message_->setInnerText(
        GetVideoElement().GetLocale().QueryString(
            WebLocalizedString::kMediaRemotingCastToUnknownDeviceText),
        ASSERT_NO_EXCEPTION);
  } else {
    cast_text_message_->setInnerText(
        GetVideoElement().GetLocale().QueryString(
            WebLocalizedString::kMediaRemotingCastText,
            remote_device_friendly_name),
        ASSERT_NO_EXCEPTION);
  }
  if (toggle_insterstitial_timer_.IsActive())
    toggle_insterstitial_timer_.Stop();
  should_be_visible_ = true;
  RemoveInlineStyleProperty(CSSPropertyDisplay);
  exit_button_->OnShown();
  toggle_insterstitial_timer_.StartOneShot(kStyleChangeTransSeconds,
                                           BLINK_FROM_HERE);
}

void MediaRemotingInterstitial::Hide() {
  DCHECK(should_be_visible_);
  if (toggle_insterstitial_timer_.IsActive())
    toggle_insterstitial_timer_.Stop();
  should_be_visible_ = false;
  SetInlineStyleProperty(CSSPropertyOpacity, 0,
                         CSSPrimitiveValue::UnitType::kNumber);
  exit_button_->OnHidden();
  toggle_insterstitial_timer_.StartOneShot(kHiddenAnimationSeconds,
                                           BLINK_FROM_HERE);
}

void MediaRemotingInterstitial::ToggleInterstitialTimerFired(TimerBase*) {
  toggle_insterstitial_timer_.Stop();
  if (should_be_visible_) {
    SetInlineStyleProperty(CSSPropertyOpacity, 1,
                           CSSPrimitiveValue::UnitType::kNumber);
  } else {
    SetInlineStyleProperty(CSSPropertyDisplay, CSSValueNone);
  }
}

void MediaRemotingInterstitial::DidMoveToNewDocument(Document& old_document) {
  toggle_insterstitial_timer_.MoveToNewTaskRunner(
      TaskRunnerHelper::Get(TaskType::kUnthrottled, &GetDocument()));

  HTMLDivElement::DidMoveToNewDocument(old_document);
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
