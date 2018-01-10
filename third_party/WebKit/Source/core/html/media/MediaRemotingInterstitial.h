// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MediaRemotingInterstitial_h
#define MediaRemotingInterstitial_h

#include "core/html/HTMLDivElement.h"
#include "platform/Timer.h"
#include "platform/WebTaskRunner.h"
#include "public/platform/WebLocalizedString.h"

namespace blink {

class HTMLImageElement;
class HTMLVideoElement;

// Media Remoting UI. DOM structure looks like:
//
// MediaRemotingInterstitial
//     (-internal-media-remoting-interstitial)
// +-HTMLImageElement
// |    (-internal-media-remoting-background-image)
// \-HTMLDivElement
// |    (-internal-media-remoting-cast-icon)
// \-HTMLDivElement
// |    (-internal-media-remoting-cast-text-message)
// |-HTMLDivElement
//      (-internal-media-remoting-toast-message)
class MediaRemotingInterstitial final : public HTMLDivElement {
 public:
  explicit MediaRemotingInterstitial(HTMLVideoElement&);

  // Show Media Remoting interstitial. |remote_device_friendly_name| will be
  // shown in the UI to indicate which device the content is rendered on. An
  // empty name indicates an unknown remote device. A default message will be
  // shown in this case.
  void Show(const WebString& remote_device_friendly_name);

  // Hide Media Remoting interstitial. A text message may be displayed for five
  // seconds according to |error_msg|.
  void Hide(WebLocalizedString::Name error_msg);

  void OnPosterImageChanged();

  // Query for whether the remoting interstitial is visible.
  bool IsVisible() const { return state_ == VISIBLE; }

  HTMLVideoElement& GetVideoElement() const { return *video_element_; }

  void Trace(blink::Visitor*) override;

 private:
  // Node override.
  bool IsMediaRemotingInterstitial() const override { return true; }
  void DidMoveToNewDocument(Document&) override;

  void ToggleInterstitialTimerFired(TimerBase*);

  // Indicates whether the interstitial should be visible. It is set/changed
  // when Show()/Hide() is called.
  enum State {
    HIDDEN,   // The interstitial is currently not showing.
    VISIBLE,  // The interstitial is currently visible except the toast.
    TOAST,    // Only the toast is visible.
  };
  State state_ = HIDDEN;

  TaskRunnerTimer<MediaRemotingInterstitial> toggle_interstitial_timer_;
  Member<HTMLVideoElement> video_element_;
  Member<HTMLImageElement> background_image_;
  Member<HTMLDivElement> cast_icon_;
  Member<HTMLDivElement> cast_text_message_;
  Member<HTMLDivElement> toast_message_;
};

}  // namespace blink

#endif  // MediaRemotingInterstitial_h
