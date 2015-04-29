// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/base_event_utils.h"

#include "base/atomic_sequence_num.h"
#include "base/logging.h"

namespace ui {

base::StaticAtomicSequenceNumber g_next_event_id;

uint32 GetNextTouchEventId() {
  // Set the first touch event ID to 1 because we set id to 0 for other types
  // of events.
  uint32 id = g_next_event_id.GetNext();
  if (id == 0)
    id = g_next_event_id.GetNext();
  DCHECK_NE(0U, id);
  return id;
}

}  // namespace ui

