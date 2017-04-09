// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MediaControlsMediaEventListener_h
#define MediaControlsMediaEventListener_h

#include "core/CoreExport.h"
#include "core/events/EventListener.h"

namespace blink {

class HTMLMediaElement;
class MediaControls;

class CORE_EXPORT MediaControlsMediaEventListener final : public EventListener {
 public:
  explicit MediaControlsMediaEventListener(MediaControls*);

  // Called by MediaControls when the HTMLMediaElement is added to a document
  // document. All event listeners should be added.
  void Attach();

  // Called by MediaControls when the HTMLMediaElement is no longer in the
  // document. All event listeners should be removed in order to prepare the
  // object to be garbage collected.
  void Detach();

  bool operator==(const EventListener&) const override;

  DECLARE_VIRTUAL_TRACE();

 private:
  HTMLMediaElement& MediaElement();

  void handleEvent(ExecutionContext*, Event*) override;

  Member<MediaControls> media_controls_;
};

}  // namespace blink

#endif  // MediaControlsMediaEventListener_h
