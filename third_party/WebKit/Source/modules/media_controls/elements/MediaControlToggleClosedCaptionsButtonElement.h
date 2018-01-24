// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MediaControlToggleClosedCaptionsButtonElement_h
#define MediaControlToggleClosedCaptionsButtonElement_h

#include "modules/ModulesExport.h"
#include "modules/media_controls/elements/MediaControlInputElement.h"

namespace blink {

class Event;
class MediaControlsImpl;

class MODULES_EXPORT MediaControlToggleClosedCaptionsButtonElement final
    : public MediaControlInputElement {
 public:
  explicit MediaControlToggleClosedCaptionsButtonElement(MediaControlsImpl&);

  // MediaControlInputElement overrides.
  bool WillRespondToMouseClickEvents() override;
  void UpdateDisplayType() override;
  WebLocalizedString::Name GetOverflowStringName() const override;
  bool HasOverflowButton() const override;
  String GetOverflowMenuSubtitleString() const override;

 protected:
  const char* GetNameForHistograms() const override;

 private:
  void DefaultEventHandler(Event*) override;
};

}  // namespace blink

#endif  // MediaControlToggleClosedCaptionsButtonElement_h
