// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MediaControlOverflowMenuListElement_h
#define MediaControlOverflowMenuListElement_h

#include "modules/media_controls/elements/MediaControlDivElement.h"
#include "platform/WebTaskRunner.h"
#include "platform/wtf/Optional.h"
#include "platform/wtf/Time.h"

namespace blink {

class Event;
class MediaControlsImpl;

// Holds a list of elements within the overflow menu.
class MediaControlOverflowMenuListElement final
    : public MediaControlDivElement {
 public:
  explicit MediaControlOverflowMenuListElement(MediaControlsImpl&);

  void SetIsWanted(bool);

 protected:
  friend class MediaControlsImpl;

  enum TimeTakenHistogram {
    kTimeToAction,
    kTimeToDismiss,
  };
  void MaybeRecordTimeTaken(TimeTakenHistogram);

 private:
  void DefaultEventHandler(Event*) override;

  TaskHandle current_task_handle_;

  WTF::Optional<WTF::TimeTicks> time_shown_;
};

}  // namespace blink

#endif  // MediaControlOverflowMenuListElement_h
