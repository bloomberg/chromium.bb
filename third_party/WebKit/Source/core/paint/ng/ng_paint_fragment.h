// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ng_paint_fragment_h
#define ng_paint_fragment_h

#include "core/CoreExport.h"
#include "core/layout/ng/ng_physical_fragment.h"
#include "core/loader/resource/ImageResourceObserver.h"
#include "platform/graphics/paint/DisplayItemClient.h"
#include "platform/scroll/ScrollTypes.h"

namespace blink {

class LayoutInline;
struct PaintInfo;

// The NGPaintFragment contains a NGPhysicalFragment and geometry in the paint
// coordinate system.
//
// NGPhysicalFragment is limited to its parent coordinate system for caching and
// sharing subtree. This class makes it possible to compute visual rects in the
// parent transform node.
//
// NGPaintFragment is an ImageResourceObserver, which means that it gets
// notified when associated images are changed.
// This is used for 2 main use cases:
// - reply to 'background-image' as we need to invalidate the background in this
//   case.
//   (See https://drafts.csswg.org/css-backgrounds-3/#the-background-image)
// - image (<img>, svg <image>) or video (<video>) elements that are
//   placeholders for displaying them.
class CORE_EXPORT NGPaintFragment : public DisplayItemClient,
                                    public ImageResourceObserver {
 public:
  NGPaintFragment(scoped_refptr<const NGPhysicalFragment>, NGPaintFragment*);
  static std::unique_ptr<NGPaintFragment> Create(
      scoped_refptr<const NGPhysicalFragment>);

  const NGPhysicalFragment& PhysicalFragment() const {
    return *physical_fragment_;
  }

  // The parent NGPaintFragment. This is nullptr for a root; i.e., when parent
  // is not for NGPaint. In the first phase, this means that this is a root of
  // an inline formatting context.
  NGPaintFragment* Parent() const { return parent_; }
  const Vector<std::unique_ptr<NGPaintFragment>>& Children() const {
    return children_;
  }

  // Returns offset to its container box for inline fragments.
  const NGPhysicalOffset& InlineOffsetToContainerBox() const {
    DCHECK(PhysicalFragment().IsInline());
    return inline_offset_to_container_box_;
  }

  // Update VisualRect for fragments without LayoutObjects (i.e., line boxes,)
  // after its descendants were updated.
  void UpdateVisualRectForNonLayoutObjectChildren();

  void AddSelfOutlineRect(Vector<LayoutRect>*, const LayoutPoint& offset) const;

  // TODO(layout-dev): Implement when we have oveflow support.
  // TODO(eae): Switch to using NG geometry types.
  bool HasOverflowClip() const;
  bool ShouldClipOverflow() const;
  bool HasSelfPaintingLayer() const;
  LayoutRect VisualRect() const override { return visual_rect_; }
  void SetVisualRect(const LayoutRect& rect) { visual_rect_ = rect; }
  LayoutRect VisualOverflowRect() const;
  LayoutRect OverflowClipRect(const LayoutPoint& location,
                              OverlayScrollbarClipBehavior) const {
    return {location, VisualRect().Size()};
  }

  LayoutRect PartialInvalidationRect() const override;

  // Paint all descendant inline box fragments that belong to the specified
  // LayoutObject.
  void PaintInlineBoxForDescendants(const PaintInfo&,
                                    const LayoutPoint& paint_offset,
                                    const LayoutInline*,
                                    NGPhysicalOffset = {}) const;

  // DisplayItemClient methods.
  String DebugName() const override { return "NGPaintFragment"; }

  // Commonly used functions for NGPhysicalFragment.
  Node* GetNode() const { return PhysicalFragment().GetNode(); }
  LayoutObject* GetLayoutObject() const {
    return PhysicalFragment().GetLayoutObject();
  }
  const ComputedStyle& Style() const { return PhysicalFragment().Style(); }
  NGPhysicalOffset Offset() const { return PhysicalFragment().Offset(); }
  NGPhysicalSize Size() const { return PhysicalFragment().Size(); }

  // A range of fragments for |FragmentsFor()|.
  class FragmentRange {
   public:
    explicit FragmentRange(NGPaintFragment* first) : first_(first) {}

    bool IsEmpty() const { return !first_; }

    class iterator {
     public:
      explicit iterator(NGPaintFragment* first) : current_(first) {}

      NGPaintFragment* operator*() const { return current_; }
      NGPaintFragment* operator->() const { return current_; }
      void operator++() {
        CHECK(current_);
        current_ = current_->next_fragment_;
      }
      bool operator==(const iterator& other) const {
        return current_ == other.current_;
      }
      bool operator!=(const iterator& other) const {
        return current_ != other.current_;
      }

     private:
      NGPaintFragment* current_;
    };

    iterator begin() const { return iterator(first_); }
    iterator end() const { return iterator(nullptr); }

   private:
    NGPaintFragment* first_;
  };

  // Returns NGPaintFragment for the inline formatting context the LayoutObject
  // belongs to.
  //
  // When the LayoutObject is an inline block, it belongs to an inline
  // formatting context while it creates its own for its descendants. This
  // function always returns the one it belongs to.
  static NGPaintFragment* GetForInlineContainer(const LayoutObject*);

  // Returns a range of NGPaintFragment in an inline formatting context that are
  // for a LayoutObject.
  static FragmentRange InlineFragmentsFor(const LayoutObject*);

 private:
  void PopulateDescendants(
      const NGPhysicalOffset inline_offset_to_container_box,
      HashMap<const LayoutObject*, NGPaintFragment*>* first_fragment_map,
      HashMap<const LayoutObject*, NGPaintFragment*>* last_fragment_map);

  //
  // Following fields are computed in the layout phase.
  //

  scoped_refptr<const NGPhysicalFragment> physical_fragment_;

  NGPaintFragment* parent_;
  Vector<std::unique_ptr<NGPaintFragment>> children_;

  NGPaintFragment* next_fragment_ = nullptr;
  NGPhysicalOffset inline_offset_to_container_box_;

  // Maps LayoutObject to NGPaintFragment for the root of an inline formatting
  // context.
  // TODO(kojii): This is to be stored in fields of LayoutObject where they are
  // no longer used in NGPaint, specifically:
  //   LayoutText::first_text_box_
  //   LayoutInline::line_boxes_
  //   LayotuBox::inline_box_wrapper_
  // but doing so is likely to have some impacts on the performance.
  // Alternatively we can keep in the root NGPaintFragment. Having this in all
  // NGPaintFragment is tentative.
  HashMap<const LayoutObject*, NGPaintFragment*> first_fragment_map_;

  //
  // Following fields are computed in the pre-paint phase.
  //

  LayoutRect visual_rect_;
};

}  // namespace blink

#endif  // ng_paint_fragment_h
