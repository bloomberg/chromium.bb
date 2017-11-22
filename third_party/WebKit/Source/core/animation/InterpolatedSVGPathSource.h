// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef InterpolatedSVGPathSource_h
#define InterpolatedSVGPathSource_h

#include "base/macros.h"
#include "core/animation/SVGPathSegInterpolationFunctions.h"
#include "core/svg/SVGPathData.h"
#include "platform/wtf/Vector.h"

namespace blink {

class InterpolatedSVGPathSource {
  STACK_ALLOCATED();

 public:
  InterpolatedSVGPathSource(const InterpolableList& list_value,
                            const Vector<SVGPathSegType>& path_seg_types)
      : current_index_(0),
        interpolable_path_segs_(list_value),
        path_seg_types_(path_seg_types) {
    DCHECK_EQ(interpolable_path_segs_.length(), path_seg_types_.size());
  }

  bool HasMoreData() const;
  PathSegmentData ParseSegment();

 private:
  PathCoordinates current_coordinates_;
  size_t current_index_;
  const InterpolableList& interpolable_path_segs_;
  const Vector<SVGPathSegType>& path_seg_types_;
  DISALLOW_COPY_AND_ASSIGN(InterpolatedSVGPathSource);
};

bool InterpolatedSVGPathSource::HasMoreData() const {
  return current_index_ < interpolable_path_segs_.length();
}

PathSegmentData InterpolatedSVGPathSource::ParseSegment() {
  PathSegmentData segment =
      SVGPathSegInterpolationFunctions::ConsumeInterpolablePathSeg(
          *interpolable_path_segs_.Get(current_index_),
          path_seg_types_.at(current_index_), current_coordinates_);
  current_index_++;
  return segment;
}

}  // namespace blink

#endif  // InterpolatedSVGPathSource_h
