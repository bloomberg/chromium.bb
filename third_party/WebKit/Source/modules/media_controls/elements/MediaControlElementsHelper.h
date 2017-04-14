// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MediaControlElementsHelper_h
#define MediaControlElementsHelper_h

#include "platform/wtf/Allocator.h"

namespace blink {

class Event;

// Helper class for media control elements. It contains methods, constants or
// concepts shared by more than one element.
class MediaControlElementsHelper final {
  STATIC_ONLY(MediaControlElementsHelper);

 public:
  static bool IsUserInteractionEvent(Event*);
};

}  // namespace blink

#endif  // MediaControlElementsHelper_h
