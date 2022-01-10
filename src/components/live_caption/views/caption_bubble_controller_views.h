// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LIVE_CAPTION_VIEWS_CAPTION_BUBBLE_CONTROLLER_VIEWS_H_
#define COMPONENTS_LIVE_CAPTION_VIEWS_CAPTION_BUBBLE_CONTROLLER_VIEWS_H_

#include <memory>
#include <string>
#include <unordered_map>

#include "base/memory/raw_ptr.h"
#include "components/live_caption/caption_bubble_controller.h"

namespace views {
class Widget;
}

namespace captions {

class CaptionBubble;
class CaptionBubbleModel;

///////////////////////////////////////////////////////////////////////////////
// Caption Bubble Controller for Views
//
//  The implementation of the caption bubble controller for Views.
//
class CaptionBubbleControllerViews : public CaptionBubbleController {
 public:
  CaptionBubbleControllerViews();
  ~CaptionBubbleControllerViews() override;
  CaptionBubbleControllerViews(const CaptionBubbleControllerViews&) = delete;
  CaptionBubbleControllerViews& operator=(const CaptionBubbleControllerViews&) =
      delete;

  // Called when a transcription is received from the service. Returns whether
  // the transcription result was set on the caption bubble successfully.
  // Transcriptions will halt if this returns false.
  bool OnTranscription(CaptionBubbleContext* caption_bubble_context,
                       const media::SpeechRecognitionResult& result) override;

  // Called when the speech service has an error.
  void OnError(CaptionBubbleContext* caption_bubble_context) override;

  // Called when the audio stream has ended.
  void OnAudioStreamEnd(CaptionBubbleContext* caption_bubble_context) override;

  // Called when the caption style changes.
  void UpdateCaptionStyle(
      absl::optional<ui::CaptionStyle> caption_style) override;

 private:
  friend class CaptionBubbleControllerViewsTest;

  // A callback passed to the CaptionBubble which is called when the
  // CaptionBubble is destroyed.
  void OnCaptionBubbleDestroyed();

  // Sets the active CaptionBubbleModel to the one corresponding to the given
  // media player id, and creates a new CaptionBubbleModel if one does not
  // already exist.
  void SetActiveModel(CaptionBubbleContext* caption_bubble_context);

  bool IsWidgetVisibleForTesting() override;
  std::string GetBubbleLabelTextForTesting() override;

  raw_ptr<CaptionBubble> caption_bubble_;
  raw_ptr<views::Widget> caption_widget_;

  // A pointer to the currently active CaptionBubbleModel.
  raw_ptr<CaptionBubbleModel> active_model_ = nullptr;

  // A map of media player ids and their corresponding CaptionBubbleModel. New
  // entries are added to this map when a previously unseen media player id is
  // received.
  std::unordered_map<CaptionBubbleContext*, std::unique_ptr<CaptionBubbleModel>>
      caption_bubble_models_;
};
}  // namespace captions

#endif  // COMPONENTS_LIVE_CAPTION_VIEWS_CAPTION_BUBBLE_CONTROLLER_VIEWS_H_
