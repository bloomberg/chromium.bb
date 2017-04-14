/*
 * Copyright (C) 2008, 2009, 2010, 2011 Apple Inc. All rights reserved.
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef MediaControlElements_h
#define MediaControlElements_h

#include "core/html/shadow/MediaControlElementTypes.h"
#include "core/html/shadow/MediaControlTimelineMetrics.h"

namespace blink {

class CORE_EXPORT MediaControlTimelineElement final
    : public MediaControlInputElement {
 public:
  static MediaControlTimelineElement* Create(MediaControls&);

  bool WillRespondToMouseClickEvents() override;

  // FIXME: An "earliest possible position" will be needed once that concept
  // is supported by HTMLMediaElement, see https://crbug.com/137275
  void SetPosition(double);
  void SetDuration(double);

  void OnPlaying();

 private:
  explicit MediaControlTimelineElement(MediaControls&);

  void DefaultEventHandler(Event*) override;
  bool KeepEventInNode(Event*) override;

  // Width in CSS pixels * pageZoomFactor (ignores CSS transforms for
  // simplicity; deliberately ignores pinch zoom's pageScaleFactor).
  int TimelineWidth();

  MediaControlTimelineMetrics metrics_;
};

// ----------------------------

class CORE_EXPORT MediaControlVolumeSliderElement final
    : public MediaControlInputElement {
 public:
  static MediaControlVolumeSliderElement* Create(MediaControls&);

  bool WillRespondToMouseMoveEvents() override;
  bool WillRespondToMouseClickEvents() override;
  void SetVolume(double);

 private:
  explicit MediaControlVolumeSliderElement(MediaControls&);

  void DefaultEventHandler(Event*) override;
  bool KeepEventInNode(Event*) override;
};

}  // namespace blink

#endif  // MediaControlElements_h
