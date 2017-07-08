// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MediaRemotingInterstitial_h
#define MediaRemotingInterstitial_h

#include "core/html/HTMLDivElement.h"

namespace blink {

class HTMLImageElement;
class HTMLVideoElement;
class MediaRemotingExitButtonElement;
class MediaRemotingCastMessageElement;
class MediaRemotingCastIconElement;

// Media Remoting UI. DOM structure looks like:
//
// MediaRemotingInterstitial
//     (-internal-media-remoting-interstitial)
// +-HTMLImageElement
// |    (-internal-media-remoting-background-image)
// \-MediaRemotingCastIconElement
// |    (-internal-media-remoting-cast-icon)
// \-MediaRemotingCastMessageElement
// |    (-internal-media-remoting-cast-text-message)
// \-MediaRemotingExitButtonElement
//      (-internal-media-remoting-disable-button)
class MediaRemotingInterstitial final : public HTMLDivElement {
 public:
  explicit MediaRemotingInterstitial(HTMLVideoElement&);

  // Show/Hide Media Remoting interstitial.
  void Show();
  void Hide();

  void OnPosterImageChanged();

  HTMLVideoElement& GetVideoElement() const { return *video_element_; }

  DECLARE_VIRTUAL_TRACE();

 private:
  // Node override.
  bool IsMediaRemotingInterstitial() const override { return true; }
  void DidMoveToNewDocument(Document&) override;

  void ToggleInterstitialTimerFired(TimerBase*);

  // Indicates whether the interstitial should be visible. It is set/changed
  // when SHow()/Hide() is called.
  bool should_be_visible_ = false;

  TaskRunnerTimer<MediaRemotingInterstitial> toggle_insterstitial_timer_;
  Member<HTMLVideoElement> video_element_;
  Member<HTMLImageElement> background_image_;
  Member<MediaRemotingExitButtonElement> exit_button_;
  Member<MediaRemotingCastIconElement> cast_icon_;
  Member<MediaRemotingCastMessageElement> cast_text_message_;
};

}  // namespace blink

#endif  // MediaRemotingInterstitial_h
