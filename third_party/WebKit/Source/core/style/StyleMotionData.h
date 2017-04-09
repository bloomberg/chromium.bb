// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef StyleMotionData_h
#define StyleMotionData_h

#include "core/style/StyleOffsetRotation.h"
#include "core/style/StylePath.h"
#include "platform/Length.h"
#include "platform/LengthPoint.h"
#include "wtf/Allocator.h"

namespace blink {

class StyleMotionData {
  DISALLOW_NEW();

 public:
  StyleMotionData(const LengthPoint& anchor,
                  const LengthPoint& position,
                  StylePath* path,
                  const Length& distance,
                  StyleOffsetRotation rotation)
      : anchor_(anchor),
        position_(position),
        path_(path),
        distance_(distance),
        rotation_(rotation) {}

  bool operator==(const StyleMotionData&) const;

  bool operator!=(const StyleMotionData& o) const { return !(*this == o); }

  // Must be public for SET_VAR in ComputedStyle.h
  LengthPoint anchor_;
  LengthPoint position_;
  RefPtr<StylePath> path_;  // nullptr indicates path is 'none'
  Length distance_;
  StyleOffsetRotation rotation_;
};

}  // namespace blink

#endif  // StyleMotionData_h
