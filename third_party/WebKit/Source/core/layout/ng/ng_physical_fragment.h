// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGPhysicalFragment_h
#define NGPhysicalFragment_h

#include "core/CoreExport.h"
#include "core/layout/ng/ng_break_token.h"
#include "core/layout/ng/ng_units.h"
#include "platform/LayoutUnit.h"
#include "platform/heap/Handle.h"
#include "wtf/RefPtr.h"
#include "wtf/Vector.h"

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
  enum NGFragmentType { kFragmentBox = 0, kFragmentText = 1 };

  NGFragmentType Type() const { return static_cast<NGFragmentType>(type_); }
  bool IsBox() const { return Type() == NGFragmentType::kFragmentBox; }
  bool IsText() const { return Type() == NGFragmentType::kFragmentText; }

  // Override RefCounted's deref() to ensure operator delete is called on the
  // appropriate subclass type.
  void deref() const {
    if (derefBase())
      destroy();
  }

  // The accessors in this class shouldn't be used by layout code directly,
  // instead should be accessed by the NGFragmentBase classes. These accessors
  // exist for paint, hit-testing, etc.

  // Returns the border-box size.
  NGPhysicalSize Size() const { return size_; }
  LayoutUnit Width() const { return size_.width; }
  LayoutUnit Height() const { return size_.height; }

  // Returns the total size, including the contents outside of the border-box.
  LayoutUnit WidthOverflow() const { return overflow_.width; }
  LayoutUnit HeightOverflow() const { return overflow_.height; }

  // Returns the offset relative to the parent fragement's content-box.
  LayoutUnit LeftOffset() const {
    DCHECK(is_placed_);
    return offset_.left;
  }

  LayoutUnit TopOffset() const {
    DCHECK(is_placed_);
    return offset_.top;
  }

  NGPhysicalOffset Offset() const {
    DCHECK(is_placed_);
    return offset_;
  }

  // Should only be used by the parent fragement's layout.
  void SetOffset(NGPhysicalOffset offset) {
    DCHECK(!is_placed_);
    offset_ = offset;
    is_placed_ = true;
  }

  NGBreakToken* BreakToken() const { return break_token_.get(); }

  const ComputedStyle& Style() const;

  // GetLayoutObject should only be used when necessary for compatibility
  // with LegacyLayout.
  LayoutObject* GetLayoutObject() const { return layout_object_; }

  bool IsPlaced() const { return is_placed_; }

 protected:
  NGPhysicalFragment(LayoutObject* layout_object,
                     NGPhysicalSize size,
                     NGPhysicalSize overflow,
                     NGFragmentType type,
                     RefPtr<NGBreakToken> break_token = nullptr);

  LayoutObject* layout_object_;
  NGPhysicalSize size_;
  NGPhysicalSize overflow_;
  NGPhysicalOffset offset_;
  RefPtr<NGBreakToken> break_token_;

  unsigned type_ : 1;
  unsigned is_placed_ : 1;

 private:
  void destroy() const;
};

}  // namespace blink

#endif  // NGPhysicalFragment_h
