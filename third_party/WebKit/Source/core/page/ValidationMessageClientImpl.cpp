/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "core/page/ValidationMessageClientImpl.h"

#include "core/dom/Element.h"
#include "core/dom/TaskRunnerHelper.h"
#include "core/exported/WebViewBase.h"
#include "core/frame/FrameView.h"
#include "core/page/ChromeClient.h"
#include "platform/PlatformChromeClient.h"
#include "platform/wtf/CurrentTime.h"
#include "public/platform/WebRect.h"
#include "public/platform/WebString.h"
#include "public/web/WebTextDirection.h"
#include "public/web/WebViewClient.h"

namespace blink {

ValidationMessageClientImpl::ValidationMessageClientImpl(WebViewBase& web_view)
    : web_view_(web_view),
      current_anchor_(nullptr),
      last_page_scale_factor_(1),
      finish_time_(0) {}

ValidationMessageClientImpl* ValidationMessageClientImpl::Create(
    WebViewBase& web_view) {
  return new ValidationMessageClientImpl(web_view);
}

ValidationMessageClientImpl::~ValidationMessageClientImpl() {}

FrameView* ValidationMessageClientImpl::CurrentView() {
  return current_anchor_->GetDocument().View();
}

void ValidationMessageClientImpl::ShowValidationMessage(
    const Element& anchor,
    const String& message,
    TextDirection message_dir,
    const String& sub_message,
    TextDirection sub_message_dir) {
  if (message.IsEmpty()) {
    HideValidationMessage(anchor);
    return;
  }
  if (!anchor.GetLayoutBox())
    return;
  if (current_anchor_)
    HideValidationMessage(*current_anchor_);
  current_anchor_ = &anchor;
  IntRect anchor_in_viewport =
      CurrentView()->ContentsToViewport(anchor.PixelSnappedBoundingBox());
  last_anchor_rect_in_screen_ =
      CurrentView()->GetChromeClient()->ViewportToScreen(anchor_in_viewport,
                                                         CurrentView());
  last_page_scale_factor_ = web_view_.PageScaleFactor();
  message_ = message;
  const double kMinimumSecondToShowValidationMessage = 5.0;
  const double kSecondPerCharacter = 0.05;
  const double kStatusCheckInterval = 0.1;

  web_view_.Client()->ShowValidationMessage(
      anchor_in_viewport, message_, ToWebTextDirection(message_dir),
      sub_message, ToWebTextDirection(sub_message_dir));
  web_view_.ChromeClient().RegisterPopupOpeningObserver(this);

  finish_time_ =
      MonotonicallyIncreasingTime() +
      std::max(kMinimumSecondToShowValidationMessage,
               (message.length() + sub_message.length()) * kSecondPerCharacter);
  // FIXME: We should invoke checkAnchorStatus actively when layout, scroll,
  // or page scale change happen.
  timer_ = WTF::MakeUnique<TaskRunnerTimer<ValidationMessageClientImpl>>(
      TaskRunnerHelper::Get(TaskType::kUnspecedTimer, &anchor.GetDocument()),
      this, &ValidationMessageClientImpl::CheckAnchorStatus);
  timer_->StartRepeating(kStatusCheckInterval, BLINK_FROM_HERE);
}

void ValidationMessageClientImpl::HideValidationMessage(const Element& anchor) {
  if (!current_anchor_ || !IsValidationMessageVisible(anchor))
    return;
  timer_ = nullptr;
  current_anchor_ = nullptr;
  message_ = String();
  finish_time_ = 0;
  web_view_.Client()->HideValidationMessage();
  web_view_.ChromeClient().UnregisterPopupOpeningObserver(this);
}

bool ValidationMessageClientImpl::IsValidationMessageVisible(
    const Element& anchor) {
  return current_anchor_ == &anchor;
}

void ValidationMessageClientImpl::WillUnloadDocument(const Document& document) {
  if (current_anchor_ && current_anchor_->GetDocument() == document)
    HideValidationMessage(*current_anchor_);
}

void ValidationMessageClientImpl::DocumentDetached(const Document& document) {
  DCHECK(!current_anchor_ || current_anchor_->GetDocument() != document)
      << "willUnloadDocument() should be called beforehand.";
}

void ValidationMessageClientImpl::CheckAnchorStatus(TimerBase*) {
  DCHECK(current_anchor_);
  if (MonotonicallyIncreasingTime() >= finish_time_ || !CurrentView()) {
    HideValidationMessage(*current_anchor_);
    return;
  }

  IntRect new_anchor_rect_in_viewport =
      current_anchor_->VisibleBoundsInVisualViewport();
  if (new_anchor_rect_in_viewport.IsEmpty()) {
    HideValidationMessage(*current_anchor_);
    return;
  }

  IntRect new_anchor_rect_in_viewport_in_screen =
      CurrentView()->GetChromeClient()->ViewportToScreen(
          new_anchor_rect_in_viewport, CurrentView());
  if (new_anchor_rect_in_viewport_in_screen == last_anchor_rect_in_screen_ &&
      web_view_.PageScaleFactor() == last_page_scale_factor_)
    return;
  last_anchor_rect_in_screen_ = new_anchor_rect_in_viewport_in_screen;
  last_page_scale_factor_ = web_view_.PageScaleFactor();
  web_view_.Client()->MoveValidationMessage(new_anchor_rect_in_viewport);
}

void ValidationMessageClientImpl::WillBeDestroyed() {
  if (current_anchor_)
    HideValidationMessage(*current_anchor_);
}

void ValidationMessageClientImpl::WillOpenPopup() {
  if (current_anchor_)
    HideValidationMessage(*current_anchor_);
}

DEFINE_TRACE(ValidationMessageClientImpl) {
  visitor->Trace(current_anchor_);
  ValidationMessageClient::Trace(visitor);
}

}  // namespace blink
