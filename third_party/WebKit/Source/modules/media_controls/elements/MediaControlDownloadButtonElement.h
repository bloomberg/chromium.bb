// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MediaControlDownloadButtonElement_h
#define MediaControlDownloadButtonElement_h

#include "modules/media_controls/elements/MediaControlInputElement.h"

namespace blink {

class Event;
class MediaControlsImpl;

class MediaControlDownloadButtonElement final
    : public MediaControlInputElement {
 public:
  explicit MediaControlDownloadButtonElement(MediaControlsImpl&);

  // Returns true if the download button should be shown. We should
  // show the button for only non-MSE, non-EME, and non-MediaStream content.
  bool ShouldDisplayDownloadButton();

  // MediaControlInputElement overrides.
  // TODO(mlamouri): add WillRespondToMouseClickEvents
  WebLocalizedString::Name GetOverflowStringName() const final;
  bool HasOverflowButton() const final;

  DECLARE_VIRTUAL_TRACE();

 protected:
  const char* GetNameForHistograms() const final;

 private:
  // This is used for UMA histogram (Media.Controls.Download). New values should
  // be appended only and must be added before |Count|.
  enum class DownloadActionMetrics {
    kShown = 0,
    kClicked,
    kCount  // Keep last.
  };

  void DefaultEventHandler(Event*) final;

  // Points to an anchor element that contains the URL of the media file.
  Member<HTMLAnchorElement> anchor_;
};

}  // namespace blink

#endif  // MediaControlDownloadButtonElement_h
