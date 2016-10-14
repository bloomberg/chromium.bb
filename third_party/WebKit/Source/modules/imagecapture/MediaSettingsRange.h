// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MediaSettingsRange_h
#define MediaSettingsRange_h

#include "bindings/core/v8/ScriptWrappable.h"

namespace blink {

class MediaSettingsRange final : public GarbageCollected<MediaSettingsRange>,
                                 public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static MediaSettingsRange* create(long max,
                                    long min,
                                    long current,
                                    long step) {
    return new MediaSettingsRange(max, min, current, step);
  }

  long max() const { return m_max; }
  long min() const { return m_min; }
  long current() const { return m_current; }
  long step() const { return m_step; }

  DEFINE_INLINE_TRACE() {}

 private:
  MediaSettingsRange(long max, long min, long current, long step)
      : m_max(max), m_min(min), m_current(current), m_step(step) {}

  long m_max;
  long m_min;
  long m_current;
  long m_step;
};

}  // namespace blink

#endif  // MediaSettingsRange_h
