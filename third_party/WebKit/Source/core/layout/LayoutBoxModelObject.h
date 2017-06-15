/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003, 2006, 2007, 2009 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#ifndef LayoutBoxModelObject_h
#define LayoutBoxModelObject_h

#include <memory>
#include "core/CoreExport.h"
#include "core/layout/BackgroundBleedAvoidance.h"
#include "core/layout/ContentChangeType.h"
#include "core/layout/LayoutObject.h"
#include "core/page/scrolling/StickyPositionScrollingConstraints.h"
#include "platform/geometry/LayoutRect.h"
#include "platform/wtf/PtrUtil.h"

namespace blink {

class PaintLayer;
class PaintLayerScrollableArea;

enum PaintLayerType {
  kNoPaintLayer,
  kNormalPaintLayer,
  // A forced or overflow clip layer is required for bookkeeping purposes,
  // but does not force a layer to be self painting.
  kOverflowClipPaintLayer,
  kForcedPaintLayer
};

enum : uint32_t {
  kBackgroundPaintInGraphicsLayer = 1 << 0,
  kBackgroundPaintInScrollingContents = 1 << 1
};
using BackgroundPaintLocation = uint32_t;

// Modes for some of the line-related functions.
enum LinePositionMode {
  kPositionOnContainingLine,
  kPositionOfInteriorLineBoxes
};
enum LineDirectionMode { kHorizontalLine, kVerticalLine };

struct LayoutBoxModelObjectRareData {
  WTF_MAKE_NONCOPYABLE(LayoutBoxModelObjectRareData);
  USING_FAST_MALLOC(LayoutBoxModelObjectRareData);

 public:
  LayoutBoxModelObjectRareData() {}

  StickyPositionScrollingConstraints sticky_position_scrolling_constraints_;
};

// This class is the base class for all CSS objects.
//
// All CSS objects follow the box model object. See THE BOX MODEL section in
// LayoutBox for more information.
//
// This class actually doesn't have the box model but it exposes some common
// functions or concepts that sub-classes can extend upon. For example, there
// are accessors for margins, borders, paddings and borderBoundingBox().
//
// The reason for this partial implementation is that the 2 classes inheriting
// from it (LayoutBox and LayoutInline) have different requirements but need to
// have a PaintLayer. For a full implementation of the box model, see LayoutBox.
//
// An important member of this class is PaintLayer, which is stored in a rare-
// data pattern (see: Layer()). PaintLayers are instantiated for several reasons
// based on the return value of layerTypeRequired().
// Interestingly, most SVG objects inherit from LayoutSVGModelObject and thus
// can't have a PaintLayer. This is an unfortunate artifact of our
// design as it limits code sharing and prevents hardware accelerating SVG
// (the current design require a PaintLayer for compositing).
//
//
// ***** COORDINATE SYSTEMS *****
//
// In order to fully understand LayoutBoxModelObject and the inherited classes,
// we need to introduce the concept of coordinate systems.
// There is 3 main coordinate systems:
// - physical coordinates: it is the coordinate system used for painting and
//   correspond to physical direction as seen on the physical display (screen,
//   printed page). In CSS, 'top', 'right', 'bottom', 'left' are all in physical
//   coordinates. The code matches this convention too.
//
// - logical coordinates: this is the coordinate system used for layout. It is
//   determined by 'writing-mode' and 'direction'. Any property using 'before',
//   'after', 'start' or 'end' is in logical coordinates. Those are also named
//   respectively 'logical top', 'logical bottom', 'logical left' and
//   'logical right'.
//
// Example with writing-mode: vertical-rl; direction: ltr;
//
//                    'top' / 'start' side
//
//                     block-flow direction
//           <------------------------------------ |
//           ------------------------------------- |
//           |        c   |          s           | |
// 'left'    |        o   |          o           | |   inline     'right'
//    /      |        n   |          m           | |  direction      /
// 'after'   |        t   |          e           | |              'before'
//  side     |        e   |                      | |                side
//           |        n   |                      | |
//           |        t   |                      | |
//           ------------------------------------- v
//
//                 'bottom' / 'end' side
//
// See https://drafts.csswg.org/css-writing-modes-3/#text-flow for some
// extra details.
//
// - physical coordinates with flipped block-flow direction: those are physical
//   coordinates but we flipped the block direction. See
//   LayoutBox::noOverflowRect.
//
// For more, see Source/core/layout/README.md ### Coordinate Spaces.
class CORE_EXPORT LayoutBoxModelObject : public LayoutObject {
 public:
  LayoutBoxModelObject(ContainerNode*);
  ~LayoutBoxModelObject() override;

  // This is the only way layers should ever be destroyed.
  void DestroyLayer();

  LayoutSize RelativePositionOffset() const;
  LayoutSize RelativePositionLogicalOffset() const {
    return Style()->IsHorizontalWritingMode()
               ? RelativePositionOffset()
               : RelativePositionOffset().TransposedSize();
  }

  // Populates StickyPositionConstraints, setting the sticky box rect,
  // containing block rect and updating the constraint offsets according to the
  // available space.
  FloatRect ComputeStickyConstrainingRect() const;
  void UpdateStickyPositionConstraints() const;
  LayoutSize StickyPositionOffset() const;

  LayoutSize OffsetForInFlowPosition() const;

  // IE extensions. Used to calculate offsetWidth/Height. Overridden by inlines
  // (LayoutInline) to return the remaining width on a given line (and the
  // height of a single line).
  virtual LayoutUnit OffsetLeft(const Element*) const;
  virtual LayoutUnit OffsetTop(const Element*) const;
  virtual LayoutUnit OffsetWidth() const = 0;
  virtual LayoutUnit OffsetHeight() const = 0;

  int PixelSnappedOffsetLeft(const Element* parent) const {
    return RoundToInt(OffsetLeft(parent));
  }
  int PixelSnappedOffsetTop(const Element* parent) const {
    return RoundToInt(OffsetTop(parent));
  }
  virtual int PixelSnappedOffsetWidth(const Element*) const;
  virtual int PixelSnappedOffsetHeight(const Element*) const;

  bool HasSelfPaintingLayer() const;
  PaintLayer* Layer() const {
    return GetRarePaintData() ? GetRarePaintData()->Layer() : nullptr;
  }
  // The type of PaintLayer to instantiate. Any value returned from this
  // function other than NoPaintLayer will lead to a PaintLayer being created.
  virtual PaintLayerType LayerTypeRequired() const = 0;
  PaintLayerScrollableArea* GetScrollableArea() const;

  virtual void UpdateFromStyle();

  // This will work on inlines to return the bounding box of all of the lines'
  // border boxes.
  virtual IntRect BorderBoundingBox() const = 0;

  virtual LayoutRect VisualOverflowRect() const = 0;

  // Checks if this box, or any of it's descendants, or any of it's
  // continuations, will take up space in the layout of the page.
  bool HasNonEmptyLayoutSize() const;
  bool UsesCompositedScrolling() const;

  // Returns which layers backgrounds should be painted into for overflow
  // scrolling boxes.
  BackgroundPaintLocation GetBackgroundPaintLocation(uint32_t* reasons) const;

  // These return the CSS computed padding values.
  LayoutUnit ComputedCSSPaddingTop() const {
    return ComputedCSSPadding(Style()->PaddingTop());
  }
  LayoutUnit ComputedCSSPaddingBottom() const {
    return ComputedCSSPadding(Style()->PaddingBottom());
  }
  LayoutUnit ComputedCSSPaddingLeft() const {
    return ComputedCSSPadding(Style()->PaddingLeft());
  }
  LayoutUnit ComputedCSSPaddingRight() const {
    return ComputedCSSPadding(Style()->PaddingRight());
  }
  LayoutUnit ComputedCSSPaddingBefore() const {
    return ComputedCSSPadding(Style()->PaddingBefore());
  }
  LayoutUnit ComputedCSSPaddingAfter() const {
    return ComputedCSSPadding(Style()->PaddingAfter());
  }
  LayoutUnit ComputedCSSPaddingStart() const {
    return ComputedCSSPadding(Style()->PaddingStart());
  }
  LayoutUnit ComputedCSSPaddingEnd() const {
    return ComputedCSSPadding(Style()->PaddingEnd());
  }
  LayoutUnit ComputedCSSPaddingOver() const {
    return ComputedCSSPadding(Style()->PaddingOver());
  }
  LayoutUnit ComputedCSSPaddingUnder() const {
    return ComputedCSSPadding(Style()->PaddingUnder());
  }

  // These functions are used during layout.
  // - Table cells override them to include the intrinsic padding (see
  //   explanations in LayoutTableCell).
  // - Table override them to exclude padding with collapsing borders.
  virtual LayoutUnit PaddingTop() const { return ComputedCSSPaddingTop(); }
  virtual LayoutUnit PaddingBottom() const {
    return ComputedCSSPaddingBottom();
  }
  virtual LayoutUnit PaddingLeft() const { return ComputedCSSPaddingLeft(); }
  virtual LayoutUnit PaddingRight() const { return ComputedCSSPaddingRight(); }
  virtual LayoutUnit PaddingBefore() const {
    return ComputedCSSPaddingBefore();
  }
  virtual LayoutUnit PaddingAfter() const { return ComputedCSSPaddingAfter(); }
  virtual LayoutUnit PaddingStart() const { return ComputedCSSPaddingStart(); }
  virtual LayoutUnit PaddingEnd() const { return ComputedCSSPaddingEnd(); }
  LayoutUnit PaddingOver() const { return ComputedCSSPaddingOver(); }
  LayoutUnit PaddingUnder() const { return ComputedCSSPaddingUnder(); }

  virtual LayoutUnit BorderTop() const {
    return LayoutUnit(Style()->BorderTopWidth());
  }
  virtual LayoutUnit BorderBottom() const {
    return LayoutUnit(Style()->BorderBottomWidth());
  }
  virtual LayoutUnit BorderLeft() const {
    return LayoutUnit(Style()->BorderLeftWidth());
  }
  virtual LayoutUnit BorderRight() const {
    return LayoutUnit(Style()->BorderRightWidth());
  }
  virtual LayoutUnit BorderBefore() const {
    return LayoutUnit(Style()->BorderBeforeWidth());
  }
  virtual LayoutUnit BorderAfter() const {
    return LayoutUnit(Style()->BorderAfterWidth());
  }
  virtual LayoutUnit BorderStart() const {
    return LayoutUnit(Style()->BorderStartWidth());
  }
  virtual LayoutUnit BorderEnd() const {
    return LayoutUnit(Style()->BorderEndWidth());
  }
  LayoutUnit BorderOver() const {
    return LayoutUnit(Style()->BorderOverWidth());
  }
  LayoutUnit BorderUnder() const {
    return LayoutUnit(Style()->BorderUnderWidth());
  }

  LayoutUnit BorderWidth() const { return BorderLeft() + BorderRight(); }
  LayoutUnit BorderHeight() const { return BorderTop() + BorderBottom(); }

  virtual LayoutRectOutsets BorderBoxOutsets() const {
    return LayoutRectOutsets(BorderTop(), BorderRight(), BorderBottom(),
                             BorderLeft());
  }

  // Insets from the border box to the inside of the border.
  LayoutRectOutsets BorderInsets() const {
    return LayoutRectOutsets(-BorderTop(), -BorderRight(), -BorderBottom(),
                             -BorderLeft());
  }

  LayoutRectOutsets BorderPaddingInsets() const {
    return LayoutRectOutsets(
        -(PaddingTop() + BorderTop()), -(PaddingRight() + BorderRight()),
        -(PaddingBottom() + BorderBottom()), -(PaddingLeft() + BorderLeft()));
  }

  bool HasBorderOrPadding() const {
    return Style()->HasBorder() || Style()->HasPadding();
  }

  LayoutUnit BorderAndPaddingStart() const {
    return BorderStart() + PaddingStart();
  }
  DISABLE_CFI_PERF LayoutUnit BorderAndPaddingBefore() const {
    return BorderBefore() + PaddingBefore();
  }
  DISABLE_CFI_PERF LayoutUnit BorderAndPaddingAfter() const {
    return BorderAfter() + PaddingAfter();
  }
  LayoutUnit BorderAndPaddingOver() const {
    return BorderOver() + PaddingOver();
  }
  LayoutUnit BorderAndPaddingUnder() const {
    return BorderUnder() + PaddingUnder();
  }

  DISABLE_CFI_PERF LayoutUnit BorderAndPaddingHeight() const {
    return BorderTop() + BorderBottom() + PaddingTop() + PaddingBottom();
  }
  DISABLE_CFI_PERF LayoutUnit BorderAndPaddingWidth() const {
    return BorderLeft() + BorderRight() + PaddingLeft() + PaddingRight();
  }
  DISABLE_CFI_PERF LayoutUnit BorderAndPaddingLogicalHeight() const {
    return HasBorderOrPadding()
               ? BorderAndPaddingBefore() + BorderAndPaddingAfter()
               : LayoutUnit();
  }
  DISABLE_CFI_PERF LayoutUnit BorderAndPaddingLogicalWidth() const {
    return BorderStart() + BorderEnd() + PaddingStart() + PaddingEnd();
  }
  DISABLE_CFI_PERF LayoutUnit BorderAndPaddingLogicalLeft() const {
    return Style()->IsHorizontalWritingMode() ? BorderLeft() + PaddingLeft()
                                              : BorderTop() + PaddingTop();
  }

  LayoutUnit BorderLogicalLeft() const {
    return LayoutUnit(Style()->IsHorizontalWritingMode() ? BorderLeft()
                                                         : BorderTop());
  }
  LayoutUnit BorderLogicalRight() const {
    return LayoutUnit(Style()->IsHorizontalWritingMode() ? BorderRight()
                                                         : BorderBottom());
  }

  LayoutUnit PaddingLogicalWidth() const {
    return PaddingStart() + PaddingEnd();
  }
  LayoutUnit PaddingLogicalHeight() const {
    return PaddingBefore() + PaddingAfter();
  }

  LayoutUnit CollapsedBorderAndCSSPaddingLogicalWidth() const {
    return ComputedCSSPaddingStart() + ComputedCSSPaddingEnd() + BorderStart() +
           BorderEnd();
  }
  LayoutUnit CollapsedBorderAndCSSPaddingLogicalHeight() const {
    return ComputedCSSPaddingBefore() + ComputedCSSPaddingAfter() +
           BorderBefore() + BorderAfter();
  }

  virtual LayoutRectOutsets MarginBoxOutsets() const = 0;
  virtual LayoutUnit MarginTop() const = 0;
  virtual LayoutUnit MarginBottom() const = 0;
  virtual LayoutUnit MarginLeft() const = 0;
  virtual LayoutUnit MarginRight() const = 0;
  virtual LayoutUnit MarginBefore(
      const ComputedStyle* other_style = nullptr) const = 0;
  virtual LayoutUnit MarginAfter(
      const ComputedStyle* other_style = nullptr) const = 0;
  virtual LayoutUnit MarginStart(
      const ComputedStyle* other_style = nullptr) const = 0;
  virtual LayoutUnit MarginEnd(
      const ComputedStyle* other_style = nullptr) const = 0;
  virtual LayoutUnit MarginOver() const = 0;
  virtual LayoutUnit MarginUnder() const = 0;
  DISABLE_CFI_PERF LayoutUnit MarginHeight() const {
    return MarginTop() + MarginBottom();
  }
  DISABLE_CFI_PERF LayoutUnit MarginWidth() const {
    return MarginLeft() + MarginRight();
  }
  DISABLE_CFI_PERF LayoutUnit MarginLogicalHeight() const {
    return MarginBefore() + MarginAfter();
  }
  DISABLE_CFI_PERF LayoutUnit MarginLogicalWidth() const {
    return MarginStart() + MarginEnd();
  }

  bool HasInlineDirectionBordersPaddingOrMargin() const {
    return HasInlineDirectionBordersOrPadding() || MarginStart() || MarginEnd();
  }
  bool HasInlineDirectionBordersOrPadding() const {
    return BorderStart() || BorderEnd() || PaddingStart() || PaddingEnd();
  }

  virtual LayoutUnit ContainingBlockLogicalWidthForContent() const;

  virtual void ChildBecameNonInline(LayoutObject* /*child*/) {}

  // Overridden by subclasses to determine line height and baseline position.
  virtual LayoutUnit LineHeight(
      bool first_line,
      LineDirectionMode,
      LinePositionMode = kPositionOnContainingLine) const = 0;
  virtual int BaselinePosition(
      FontBaseline,
      bool first_line,
      LineDirectionMode,
      LinePositionMode = kPositionOnContainingLine) const = 0;

  const LayoutObject* PushMappingToContainer(
      const LayoutBoxModelObject* ancestor_to_stop_at,
      LayoutGeometryMap&) const override;

  void SetSelectionState(SelectionState) override;

  void ContentChanged(ContentChangeType);
  bool HasAcceleratedCompositing() const;

  void ComputeLayerHitTestRects(LayerHitTestRects&) const override;

  // Returns true if the background is painted opaque in the given rect.
  // The query rect is given in local coordinate system.
  virtual bool BackgroundIsKnownToBeOpaqueInRect(const LayoutRect&) const {
    return false;
  }

  void DeprecatedInvalidateTree(const PaintInvalidationState&) override;

  // http://www.w3.org/TR/css3-background/#body-background
  // <html> root element with no background steals background from its first
  // <body> child. The used background for such body element should be the
  // initial value. (i.e. transparent)
  bool BackgroundStolenForBeingBody(
      const ComputedStyle* root_element_style = nullptr) const;

  void AbsoluteQuads(Vector<FloatQuad>& quads,
                     MapCoordinatesFlags mode = 0) const override;

 protected:
  // Compute absolute quads for |this|, but not any continuations. May only be
  // called for objects which can be or have continuations, i.e. LayoutInline or
  // LayoutBlockFlow.
  virtual void AbsoluteQuadsForSelf(Vector<FloatQuad>& quads,
                                    MapCoordinatesFlags mode = 0) const;

  void WillBeDestroyed() override;

  LayoutPoint AdjustedPositionRelativeTo(const LayoutPoint&,
                                         const Element*) const;

  // Returns the continuation associated with |this|.
  // Returns nullptr if no continuation is associated with |this|.
  //
  // See the section about CONTINUATIONS AND ANONYMOUS LAYOUTBLOCKFLOWS in
  // LayoutInline for more details about them.
  //
  // Our implementation uses a HashMap to store them to avoid paying the cost
  // for each LayoutBoxModelObject (|continuationMap| in the cpp file).
  LayoutBoxModelObject* Continuation() const;

  // Set the next link in the continuation chain.
  //
  // See continuation above for more details.
  void SetContinuation(LayoutBoxModelObject*);

  virtual LayoutSize AccumulateInFlowPositionOffsets() const {
    return LayoutSize();
  }

  LayoutRect LocalCaretRectForEmptyElement(LayoutUnit width,
                                           LayoutUnit text_indent_offset);

  bool HasAutoHeightOrContainingBlockWithAutoHeight() const;
  LayoutBlock* ContainingBlockForAutoHeightDetection(
      Length logical_height) const;

  void AddOutlineRectsForNormalChildren(Vector<LayoutRect>&,
                                        const LayoutPoint& additional_offset,
                                        IncludeBlockVisualOverflowOrNot) const;
  void AddOutlineRectsForDescendant(const LayoutObject& descendant,
                                    Vector<LayoutRect>&,
                                    const LayoutPoint& additional_offset,
                                    IncludeBlockVisualOverflowOrNot) const;

  void AddLayerHitTestRects(LayerHitTestRects&,
                            const PaintLayer*,
                            const LayoutPoint&,
                            const LayoutRect&) const override;

  void StyleWillChange(StyleDifference,
                       const ComputedStyle& new_style) override;
  void StyleDidChange(StyleDifference, const ComputedStyle* old_style) override;

  void InvalidateStickyConstraints();

 public:
  // These functions are only used internally to manipulate the layout tree
  // structure via remove/insert/appendChildNode.
  // Since they are typically called only to move objects around within
  // anonymous blocks (which only have layers in the case of column spans), the
  // default for fullRemoveInsert is false rather than true.
  void MoveChildTo(LayoutBoxModelObject* to_box_model_object,
                   LayoutObject* child,
                   LayoutObject* before_child,
                   bool full_remove_insert = false);
  void MoveChildTo(LayoutBoxModelObject* to_box_model_object,
                   LayoutObject* child,
                   bool full_remove_insert = false) {
    MoveChildTo(to_box_model_object, child, 0, full_remove_insert);
  }
  void MoveAllChildrenTo(LayoutBoxModelObject* to_box_model_object,
                         bool full_remove_insert = false) {
    MoveAllChildrenTo(to_box_model_object, 0, full_remove_insert);
  }
  void MoveAllChildrenTo(LayoutBoxModelObject* to_box_model_object,
                         LayoutObject* before_child,
                         bool full_remove_insert = false) {
    MoveChildrenTo(to_box_model_object, SlowFirstChild(), 0, before_child,
                   full_remove_insert);
  }
  // Move all of the kids from |startChild| up to but excluding |endChild|. 0
  // can be passed as the |endChild| to denote that all the kids from
  // |startChild| onwards should be moved.
  void MoveChildrenTo(LayoutBoxModelObject* to_box_model_object,
                      LayoutObject* start_child,
                      LayoutObject* end_child,
                      bool full_remove_insert = false) {
    MoveChildrenTo(to_box_model_object, start_child, end_child, 0,
                   full_remove_insert);
  }
  virtual void MoveChildrenTo(LayoutBoxModelObject* to_box_model_object,
                              LayoutObject* start_child,
                              LayoutObject* end_child,
                              LayoutObject* before_child,
                              bool full_remove_insert = false);

 private:
  void CreateLayerAfterStyleChange();

  LayoutUnit ComputedCSSPadding(const Length&) const;
  bool IsBoxModelObject() const final { return true; }

  LayoutBoxModelObjectRareData& EnsureRareData() {
    if (!rare_data_)
      rare_data_ = WTF::MakeUnique<LayoutBoxModelObjectRareData>();
    return *rare_data_.get();
  }

  std::unique_ptr<LayoutBoxModelObjectRareData> rare_data_;
};

DEFINE_LAYOUT_OBJECT_TYPE_CASTS(LayoutBoxModelObject, IsBoxModelObject());

}  // namespace blink

#endif  // LayoutBoxModelObject_h
