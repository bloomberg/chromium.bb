// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGPaintFragmentTraversal_h
#define NGPaintFragmentTraversal_h

#include "core/CoreExport.h"
#include "core/layout/ng/geometry/ng_physical_offset.h"
#include "platform/wtf/Vector.h"

namespace blink {

class LayoutObject;
class NGPaintFragment;

// Used for return value of traversing fragment tree.
struct CORE_EXPORT NGPaintFragmentWithContainerOffset {
  DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();
  NGPaintFragment* fragment;
  // Offset relative to container fragment
  NGPhysicalOffset container_offset;
};

// Utility class for traversing the paint fragment tree.
class CORE_EXPORT NGPaintFragmentTraversal {
  STATIC_ONLY(NGPaintFragmentTraversal);

 public:
  // Return descendants without paint layer in preorder.
  static Vector<NGPaintFragmentWithContainerOffset> DescendantsOf(
      const NGPaintFragment&);

  static Vector<NGPaintFragmentWithContainerOffset> SelfFragmentsOf(
      const NGPaintFragment&,
      const LayoutObject* target);
};

}  // namespace blink

#endif  // NGPaintFragmentTraversal_h
