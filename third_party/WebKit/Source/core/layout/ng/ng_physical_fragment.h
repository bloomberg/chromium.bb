// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGPhysicalFragment_h
#define NGPhysicalFragment_h

#include "core/CoreExport.h"
#include "core/layout/ng/geometry/ng_border_edges.h"
#include "core/layout/ng/geometry/ng_box_strut.h"
#include "core/layout/ng/geometry/ng_physical_offset.h"
#include "core/layout/ng/geometry/ng_physical_size.h"
#include "core/layout/ng/ng_break_token.h"
#include "platform/LayoutUnit.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/RefPtr.h"
#include "platform/wtf/Vector.h"

namespace blink {

class ComputedStyle;
class LayoutObject;

// The NGPhysicalFragment contains the output geometry from layout. The
// fragment stores all of its information in the physical coordinate system for
// use by paint, hit-testing etc.
//
// The fragment keeps a pointer back to the LayoutObject which generated it.
// Once we have transitioned fully to LayoutNG it should be a const pointer
// such that paint/hit-testing/etc don't modify it.
//
// Layout code should only access geometry information through the
// NGFragment wrapper classes which transforms information into the logical
// coordinate system.
class CORE_EXPORT NGPhysicalFragment : public RefCounted<NGPhysicalFragment> {
 public:
  enum NGFragmentType {
    kFragmentBox = 0,
    kFragmentText = 1,
    kFragmentLineBox = 2
    // When adding new values, make sure the bit size of |type_| is large
    // enough to store.
  };

  NGFragmentType Type() const { return static_cast<NGFragmentType>(type_); }
  bool IsBox() const { return Type() == NGFragmentType::kFragmentBox; }
  bool IsText() const { return Type() == NGFragmentType::kFragmentText; }
  bool IsLineBox() const { return Type() == NGFragmentType::kFragmentLineBox; }

  // The accessors in this class shouldn't be used by layout code directly,
  // instead should be accessed by the NGFragmentBase classes. These accessors
  // exist for paint, hit-testing, etc.

  // Returns the border-box size.
  NGPhysicalSize Size() const { return size_; }

  // Bitmask for border edges, see NGBorderEdges::Physical.
  unsigned BorderEdges() const { return border_edge_; }
  NGPixelSnappedPhysicalBoxStrut BorderWidths() const;

  // Returns the offset relative to the parent fragment's content-box.
  NGPhysicalOffset Offset() const {
    DCHECK(is_placed_);
    return offset_;
  }

  NGBreakToken* BreakToken() const { return break_token_.Get(); }
  const ComputedStyle& Style() const;

  RefPtr<NGPhysicalFragment> CloneWithoutOffset() const;

  // GetLayoutObject should only be used when necessary for compatibility
  // with LegacyLayout.
  LayoutObject* GetLayoutObject() const { return layout_object_; }

  // Should only be used by the parent fragment's layout.
  void SetOffset(NGPhysicalOffset offset) {
    DCHECK(!is_placed_);
    offset_ = offset;
    is_placed_ = true;
  }

  bool IsPlaced() const { return is_placed_; }

  String ToString() const;

  // Override RefCounted's deref() to ensure operator delete is called on the
  // appropriate subclass type.
  void Deref() const {
    if (DerefBase())
      Destroy();
  }

 protected:
  NGPhysicalFragment(LayoutObject* layout_object,
                     NGPhysicalSize size,
                     NGFragmentType type,
                     RefPtr<NGBreakToken> break_token = nullptr);

  LayoutObject* layout_object_;
  NGPhysicalSize size_;
  NGPhysicalOffset offset_;
  RefPtr<NGBreakToken> break_token_;

  unsigned type_ : 2;  // NGFragmentType
  unsigned is_placed_ : 1;
  unsigned border_edge_ : 4;  // NGBorderEdges::Physical

 private:
  void Destroy() const;
};

}  // namespace blink

#endif  // NGPhysicalFragment_h
