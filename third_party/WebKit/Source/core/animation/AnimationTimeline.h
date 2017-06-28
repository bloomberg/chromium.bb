// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef AnimationTimeline_h
#define AnimationTimeline_h

#include "core/CoreExport.h"
#include "platform/bindings/ScriptWrappable.h"

namespace blink {

class CORE_EXPORT AnimationTimeline
    : public GarbageCollectedFinalized<AnimationTimeline>,
      public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  virtual ~AnimationTimeline() {}

  virtual double currentTime(bool&) = 0;

  virtual bool IsDocumentTimeline() const { return false; }

  DEFINE_INLINE_VIRTUAL_TRACE() {}
};

}  // namespace blink

#endif
