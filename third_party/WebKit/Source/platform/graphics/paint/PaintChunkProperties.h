// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PaintChunkProperties_h
#define PaintChunkProperties_h

#include <iosfwd>
#include "platform/PlatformExport.h"
#include "platform/graphics/paint/PropertyTreeState.h"
#include "platform/wtf/Allocator.h"
#include "platform/wtf/Forward.h"
#include "platform/wtf/Noncopyable.h"

namespace blink {

// The set of paint properties applying to a |PaintChunk|. These properties are
// not local-only paint style parameters such as color, but instead represent
// the hierarchy of transforms, clips, and effects that apply to a contiguous
// chunk of display items. A single DisplayItemClient can generate multiple
// properties of the same type and this struct represents the total state of all
// properties for a given |PaintChunk|.
//
// This differs from |ObjectPaintProperties| because it only stores one property
// for each type (e.g., either transform or perspective, but not both).
struct PLATFORM_EXPORT PaintChunkProperties {
  DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();

  PaintChunkProperties(const PropertyTreeState& state)
      : property_tree_state(state), backface_hidden(false) {}

  PaintChunkProperties()
      : property_tree_state(nullptr, nullptr, nullptr),
        backface_hidden(false) {}

  PropertyTreeState property_tree_state;
  bool backface_hidden;

  String ToString() const;
};

// Equality is based only on the pointers and is not 'deep' which would require
// crawling the entire property tree to compute.
inline bool operator==(const PaintChunkProperties& a,
                       const PaintChunkProperties& b) {
  return a.property_tree_state == b.property_tree_state &&
         a.backface_hidden == b.backface_hidden;
}

inline bool operator!=(const PaintChunkProperties& a,
                       const PaintChunkProperties& b) {
  return !(a == b);
}

// Redeclared here to avoid ODR issues.
// See platform/testing/PaintPrinters.h.
void PrintTo(const PaintChunkProperties&, std::ostream*);

}  // namespace blink

#endif  // PaintChunkProperties_h
