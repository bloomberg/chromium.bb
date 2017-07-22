// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MediaRemotingElements_h
#define MediaRemotingElements_h

#include "core/html/media/MediaRemotingInterstitial.h"

namespace blink {

// A button shown when media remoting starts. It allows user to click to exit
// media remoting and back to traditional tab mirroring.
class MediaRemotingExitButtonElement final : public HTMLDivElement {
 public:
  explicit MediaRemotingExitButtonElement(MediaRemotingInterstitial&);

  void OnShown();
  void OnHidden();
  HTMLVideoElement& GetVideoElement() const;

  DECLARE_VIRTUAL_TRACE();

 private:
  class PointerEventsListener;

  Member<MediaRemotingInterstitial> interstitial_;
  // A document event listener to listen to the mouse clicking events while
  // media remoting is ongoing. This listener is used instead of the
  // DefaultEventHandler() to avoid other web sites stopping propagating the
  // events. It will be removed once media remoting stops.
  Member<PointerEventsListener> listener_;
};

// A text message shown to indicate media remoting is ongoing.
class MediaRemotingCastMessageElement final : public HTMLDivElement {
 public:
  explicit MediaRemotingCastMessageElement(MediaRemotingInterstitial&);
};

// An icon shown to indicate media remoting is ongoing.
class MediaRemotingCastIconElement final : public HTMLDivElement {
 public:
  explicit MediaRemotingCastIconElement(MediaRemotingInterstitial&);
};

}  // namespace blink

#endif  // MediaRemotingElements_h
