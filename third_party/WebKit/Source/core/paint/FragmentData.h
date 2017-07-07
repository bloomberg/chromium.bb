// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FragmentData_h
#define FragmentData_h

#include "core/paint/ClipRects.h"
#include "core/paint/ObjectPaintProperties.h"
#include "platform/RuntimeEnabledFeatures.h"

namespace blink {

class ObjectPaintProperties;

// Represents the data for a particular fragment of a LayoutObject.
// Only LayoutObjects with a self-painting PaintLayer may have more than one
// FragmentDeta, and even then only when they are inside of multicol.
// See README.md.
class CORE_EXPORT FragmentData {
 public:
  static std::unique_ptr<FragmentData> Create() {
    return WTF::WrapUnique(new FragmentData());
  }

  ObjectPaintProperties* PaintProperties() const {
    return paint_properties_.get();
  }
  ObjectPaintProperties& EnsurePaintProperties();
  void ClearPaintProperties();

  ClipRects* PreviousClipRects() const {
    return previous_clip_rects_.Get();
  }
  void SetPreviousClipRects(ClipRects& clip_rects) {
    previous_clip_rects_ = &clip_rects;
  }
  void ClearPreviousClipRects() { previous_clip_rects_.Clear(); }

  FragmentData* NextFragment() { return next_fragment_.get(); }

 private:
  // Holds references to the paint property nodes created by this object.
  std::unique_ptr<ObjectPaintProperties> paint_properties_;

  // These are used to detect changes to clipping that might invalidate
  // subsequence caching or paint phase optimizations.
  RefPtr<ClipRects> previous_clip_rects_;

  std::unique_ptr<FragmentData> next_fragment_;
};

}  // namespace blink

#endif  // FragmentData_h
