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

  // The complete set of property nodes that should be used as a starting point
  // to paint this fragment. See also the comment for
  // RarePaintData::local_border_box_properties_.
  PropertyTreeState* LocalBorderBoxProperties() const {
    return local_border_box_properties_.get();
  }

  void ClearLocalBorderBoxProperties();
  void SetLocalBorderBoxProperties(PropertyTreeState&);

  // This is the complete set of property nodes that can be used to paint the
  // contents of this fragment. It is similar to local_border_box_properties_
  // but includes properties (e.g., overflow clip, scroll translation) that
  // apply to contents.
  PropertyTreeState ContentsProperties() const;

  FragmentData* NextFragment() { return next_fragment_.get(); }

 private:
  // Holds references to the paint property nodes created by this object.
  std::unique_ptr<ObjectPaintProperties> paint_properties_;

  // These are used to detect changes to clipping that might invalidate
  // subsequence caching or paint phase optimizations.
  RefPtr<ClipRects> previous_clip_rects_;

  // This is a complete set of property nodes that should be used as a
  // starting point to paint a LayoutObject. This data is cached because some
  // properties inherit from the containing block chain instead of the
  // painting parent and cannot be derived in O(1) during the paint walk.
  //
  // For example: <div style='opacity: 0.3;'/>
  //   The div's local border box properties would have an opacity 0.3 effect
  //   node. Even though the div has no transform, its local border box
  //   properties would have a transform node that points to the div's
  //   ancestor transform space.
  std::unique_ptr<PropertyTreeState> local_border_box_properties_;

  std::unique_ptr<FragmentData> next_fragment_;
};

}  // namespace blink

#endif  // FragmentData_h
