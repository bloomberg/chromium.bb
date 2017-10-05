// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGPhysicalFragment_h
#define NGPhysicalFragment_h

#include "core/CoreExport.h"
#include "core/layout/ng/geometry/ng_physical_offset.h"
#include "core/layout/ng/geometry/ng_physical_size.h"
#include "core/layout/ng/ng_break_token.h"
#include "platform/geometry/LayoutRect.h"
#include "platform/wtf/RefPtr.h"

namespace blink {

class ComputedStyle;
class LayoutObject;
class Node;
struct NGPixelSnappedPhysicalBoxStrut;

class NGPhysicalFragment;

struct CORE_EXPORT NGPhysicalFragmentTraits {
  static void Destruct(const NGPhysicalFragment*);
};

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
class CORE_EXPORT NGPhysicalFragment
    : public RefCounted<NGPhysicalFragment, NGPhysicalFragmentTraits> {
 public:
  enum NGFragmentType {
    kFragmentBox = 0,
    kFragmentText = 1,
    kFragmentLineBox = 2
    // When adding new values, make sure the bit size of |type_| is large
    // enough to store.
  };

  virtual ~NGPhysicalFragment();

  NGFragmentType Type() const { return static_cast<NGFragmentType>(type_); }
  bool IsContainer() const {
    return Type() == NGFragmentType::kFragmentBox ||
           Type() == NGFragmentType::kFragmentLineBox;
  }
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

  NGBreakToken* BreakToken() const { return break_token_.get(); }
  const ComputedStyle& Style() const;
  Node* GetNode() const;

  // GetLayoutObject should only be used when necessary for compatibility
  // with LegacyLayout.
  LayoutObject* GetLayoutObject() const { return layout_object_; }

  // TODO(layout-dev): Implement when we have oveflow support.
  LayoutRect LocalVisualRect() const { return visual_rect_; }

  // Update visual rect for this fragment.
  // This is called not only after layout, but also after transform changes,
  // because visual overflow may change due to font hinting.
  // "const" because it only updates cached value that does not affect layout.
  virtual void UpdateVisualRect() const;

  // Should only be used by the parent fragment's layout.
  void SetOffset(NGPhysicalOffset offset) {
    DCHECK(!is_placed_);
    offset_ = offset;
    is_placed_ = true;
  }

  bool IsPlaced() const { return is_placed_; }

  RefPtr<NGPhysicalFragment> CloneWithoutOffset() const;

  String ToString() const;

  enum DumpFlag {
    DumpHeaderText = 0x1,
    DumpSubtree = 0x2,
    DumpIndentation = 0x4,
    DumpType = 0x8,
    DumpOffset = 0x10,
    DumpSize = 0x20,
    DumpTextOffsets = 0x40,
    DumpAll = -1
  };
  typedef int DumpFlags;

  String DumpFragmentTree(DumpFlags) const;

#ifndef NDEBUG
  void ShowFragmentTree() const;
#endif

 protected:
  NGPhysicalFragment(LayoutObject* layout_object,
                     const ComputedStyle& style,
                     NGPhysicalSize size,
                     NGFragmentType type,
                     RefPtr<NGBreakToken> break_token = nullptr);

  // "const" because it only updates cached value that does not affect layout.
  void SetVisualRect(const LayoutRect& rect) const { visual_rect_ = rect; }

  LayoutObject* layout_object_;
  RefPtr<const ComputedStyle> style_;
  NGPhysicalSize size_;
  NGPhysicalOffset offset_;
  RefPtr<NGBreakToken> break_token_;
  mutable LayoutRect visual_rect_;

  unsigned type_ : 2;  // NGFragmentType
  unsigned is_placed_ : 1;
  unsigned border_edge_ : 4;  // NGBorderEdges::Physical

 private:
  friend struct NGPhysicalFragmentTraits;
  void Destroy() const;
};

}  // namespace blink

#endif  // NGPhysicalFragment_h
