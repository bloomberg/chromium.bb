// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/media_controls/elements/MediaControlOverflowMenuListElement.h"

#include "core/dom/TaskRunnerHelper.h"
#include "core/dom/events/Event.h"
#include "modules/media_controls/MediaControlsImpl.h"
#include "platform/Histogram.h"
#include "public/platform/TaskType.h"

namespace blink {

MediaControlOverflowMenuListElement::MediaControlOverflowMenuListElement(
    MediaControlsImpl& media_controls)
    : MediaControlDivElement(media_controls, kMediaOverflowList) {
  SetShadowPseudoId(
      AtomicString("-internal-media-controls-overflow-menu-list"));
  SetIsWanted(false);
}

void MediaControlOverflowMenuListElement::MaybeRecordTimeTaken(
    TimeTakenHistogram histogram_name) {
  DCHECK(time_shown_);

  if (current_task_handle_.IsActive())
    current_task_handle_.Cancel();

  LinearHistogram histogram(histogram_name == kTimeToAction
                                ? "Media.Controls.Overflow.TimeToAction"
                                : "Media.Controls.Overflow.TimeToDismiss",
                            1, 100, 100);
  histogram.Count(static_cast<int32_t>(
      (TimeTicks::Now() - time_shown_.value()).InSeconds()));

  time_shown_.reset();
}

void MediaControlOverflowMenuListElement::DefaultEventHandler(Event* event) {
  if (event->type() == EventTypeNames::click)
    event->SetDefaultHandled();

  MediaControlDivElement::DefaultEventHandler(event);
}

void MediaControlOverflowMenuListElement::SetIsWanted(bool wanted) {
  MediaControlDivElement::SetIsWanted(wanted);

  // Record the time the overflow menu was shown to a histogram.
  if (wanted) {
    DCHECK(!time_shown_);
    time_shown_ = TimeTicks::Now();
  } else if (time_shown_) {
    // Records the time taken to dismiss using a task runner. This ensures the
    // time to dismiss is always called after the time to action (if there is
    // one). The time to action call will then cancel the time to dismiss as it
    // is not needed.
    DCHECK(!current_task_handle_.IsActive());
    current_task_handle_ =
        TaskRunnerHelper::Get(TaskType::kMediaElementEvent, &GetDocument())
            ->PostCancellableTask(
                BLINK_FROM_HERE,
                WTF::Bind(
                    &MediaControlOverflowMenuListElement::MaybeRecordTimeTaken,
                    WrapWeakPersistent(this), kTimeToDismiss));
  }
}

}  // namespace blink
