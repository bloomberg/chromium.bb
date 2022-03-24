// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/live_caption/views/caption_bubble_controller_views.h"

#include <memory>
#include <string>

#include "base/bind.h"
#include "components/live_caption/caption_bubble_context.h"
#include "components/live_caption/live_caption_controller.h"
#include "components/live_caption/views/caption_bubble.h"
#include "components/live_caption/views/caption_bubble_model.h"

namespace captions {

// Static
std::unique_ptr<CaptionBubbleController> CaptionBubbleController::Create() {
  return std::make_unique<CaptionBubbleControllerViews>();
}

CaptionBubbleControllerViews::CaptionBubbleControllerViews() {
  caption_bubble_ = new CaptionBubble(
      base::BindOnce(&CaptionBubbleControllerViews::OnCaptionBubbleDestroyed,
                     base::Unretained(this)),
      /* hide_on_inactivity= */ true);
  caption_widget_ =
      views::BubbleDialogDelegateView::CreateBubble(caption_bubble_);
}

CaptionBubbleControllerViews::~CaptionBubbleControllerViews() {
  if (caption_widget_)
    caption_widget_->CloseNow();
}

void CaptionBubbleControllerViews::OnCaptionBubbleDestroyed() {
  caption_bubble_ = nullptr;
  caption_widget_ = nullptr;
}

bool CaptionBubbleControllerViews::OnTranscription(
    CaptionBubbleContext* caption_bubble_context,
    const media::SpeechRecognitionResult& result) {
  if (!caption_bubble_)
    return false;
  SetActiveModel(caption_bubble_context);
  if (active_model_->IsClosed())
    return false;

  // If the caption bubble has no activity and it receives a final
  // transcription, don't set text. The speech service sends a final
  // transcription after several seconds of no audio. This prevents the bubble
  // reappearing with a final transcription after it had disappeared due to no
  // activity.
  if (!caption_bubble_->HasActivity() && result.is_final)
    return true;

  active_model_->SetPartialText(result.transcription);
  if (result.is_final)
    active_model_->CommitPartialText();

  return true;
}

void CaptionBubbleControllerViews::OnError(
    CaptionBubbleContext* caption_bubble_context) {
  if (!caption_bubble_)
    return;
  SetActiveModel(caption_bubble_context);
  if (active_model_->IsClosed())
    return;
  active_model_->OnError();
}

void CaptionBubbleControllerViews::OnAudioStreamEnd(
    CaptionBubbleContext* caption_bubble_context) {
  if (!caption_bubble_)
    return;

  CaptionBubbleModel* caption_bubble_model =
      caption_bubble_models_[caption_bubble_context].get();
  if (active_model_ == caption_bubble_model) {
    active_model_ = nullptr;
    caption_bubble_->SetModel(nullptr);
  }
  caption_bubble_models_.erase(caption_bubble_context);
}

void CaptionBubbleControllerViews::UpdateCaptionStyle(
    absl::optional<ui::CaptionStyle> caption_style) {
  caption_bubble_->UpdateCaptionStyle(caption_style);
}

void CaptionBubbleControllerViews::SetActiveModel(
    CaptionBubbleContext* caption_bubble_context) {
  if (!caption_bubble_models_.count(caption_bubble_context)) {
    caption_bubble_models_.emplace(
        caption_bubble_context,
        std::make_unique<CaptionBubbleModel>(caption_bubble_context));
  }

  CaptionBubbleModel* caption_bubble_model =
      caption_bubble_models_[caption_bubble_context].get();
  if (active_model_ != caption_bubble_model) {
    active_model_ = caption_bubble_model;
    caption_bubble_->SetModel(active_model_);
  }
}

bool CaptionBubbleControllerViews::IsWidgetVisibleForTesting() {
  return caption_widget_ && caption_widget_->IsVisible();
}

std::string CaptionBubbleControllerViews::GetBubbleLabelTextForTesting() {
  return caption_bubble_
             ? base::UTF16ToUTF8(
                   caption_bubble_->GetLabelForTesting()->GetText())  // IN-TEST
             : "";
}

}  // namespace captions
