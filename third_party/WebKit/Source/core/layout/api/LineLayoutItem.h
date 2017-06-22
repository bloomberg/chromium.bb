// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LineLayoutItem_h
#define LineLayoutItem_h

#include "core/layout/LayoutObject.h"
#include "core/layout/LayoutObjectInlines.h"
#include "core/layout/LayoutText.h"
#include "core/paint/ObjectPaintInvalidator.h"

#include "platform/LayoutUnit.h"
#include "platform/wtf/Allocator.h"
#include "platform/wtf/HashTableDeletedValueType.h"

namespace blink {

class ComputedStyle;
class Document;
class HitTestRequest;
class HitTestLocation;
class LayoutObject;
class LineLayoutBox;
class LineLayoutBoxModel;
class LineLayoutAPIShim;

static LayoutObject* const kHashTableDeletedValue =
    reinterpret_cast<LayoutObject*>(-1);

class LineLayoutItem {
  DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();

 public:
  explicit LineLayoutItem(LayoutObject* layout_object)
      : layout_object_(layout_object) {}

  explicit LineLayoutItem(WTF::HashTableDeletedValueType)
      : layout_object_(kHashTableDeletedValue) {}

  LineLayoutItem(std::nullptr_t) : layout_object_(0) {}

  LineLayoutItem() : layout_object_(0) {}

  explicit operator bool() const { return layout_object_; }

  bool IsEqual(const LayoutObject* layout_object) const {
    return layout_object_ == layout_object;
  }

  bool operator==(const LineLayoutItem& other) const {
    return layout_object_ == other.layout_object_;
  }

  bool operator!=(const LineLayoutItem& other) const {
    return !(*this == other);
  }

  String DebugName() const { return layout_object_->DebugName(); }

  bool NeedsLayout() const { return layout_object_->NeedsLayout(); }

  Node* GetNode() const { return layout_object_->GetNode(); }

  Node* NonPseudoNode() const { return layout_object_->NonPseudoNode(); }

  LineLayoutItem Parent() const {
    return LineLayoutItem(layout_object_->Parent());
  }

  // Implemented in LineLayoutBox.h
  // Intentionally returns a LineLayoutBox to avoid exposing LayoutBlock
  // to the line layout code.
  LineLayoutBox ContainingBlock() const;

  // Implemented in LineLayoutBoxModel.h
  // Intentionally returns a LineLayoutBoxModel to avoid exposing
  // LayoutBoxModelObject to the line layout code.
  LineLayoutBoxModel EnclosingBoxModelObject() const;

  LineLayoutItem Container() const {
    return LineLayoutItem(layout_object_->Container());
  }

  bool IsDescendantOf(const LineLayoutItem item) const {
    return layout_object_->IsDescendantOf(item.layout_object_);
  }

  void UpdateHitTestResult(HitTestResult& result, const LayoutPoint& point) {
    return layout_object_->UpdateHitTestResult(result, point);
  }

  LineLayoutItem NextSibling() const {
    return LineLayoutItem(layout_object_->NextSibling());
  }

  LineLayoutItem PreviousSibling() const {
    return LineLayoutItem(layout_object_->PreviousSibling());
  }

  LineLayoutItem SlowFirstChild() const {
    return LineLayoutItem(layout_object_->SlowFirstChild());
  }

  LineLayoutItem SlowLastChild() const {
    return LineLayoutItem(layout_object_->SlowLastChild());
  }

  // TODO(dgrogan/eae): Collapse these 4 methods to 1. Settle on pointer or
  // ref. Give firstLine a default value.
  const ComputedStyle* Style() const { return layout_object_->Style(); }

  const ComputedStyle& StyleRef() const { return layout_object_->StyleRef(); }

  const ComputedStyle* Style(bool first_line) const {
    return layout_object_->Style(first_line);
  }

  const ComputedStyle& StyleRef(bool first_line) const {
    return layout_object_->StyleRef(first_line);
  }

  Document& GetDocument() const { return layout_object_->GetDocument(); }

  // TODO(dgrogan): This is the only caller: move the logic from LayoutObject
  // to here.
  bool PreservesNewline() const { return layout_object_->PreservesNewline(); }

  unsigned length() const { return layout_object_->length(); }

  void DirtyLinesFromChangedChild(
      LineLayoutItem item,
      MarkingBehavior marking_behaviour = kMarkContainerChain) const {
    layout_object_->DirtyLinesFromChangedChild(item.GetLayoutObject(),
                                               marking_behaviour);
  }

  bool AncestorLineBoxDirty() const {
    return layout_object_->AncestorLineBoxDirty();
  }

  // TODO(dgrogan/eae): Remove this method and replace every call with an ||.
  bool IsFloatingOrOutOfFlowPositioned() const {
    return layout_object_->IsFloatingOrOutOfFlowPositioned();
  }

  bool IsFloating() const { return layout_object_->IsFloating(); }

  bool IsOutOfFlowPositioned() const {
    return layout_object_->IsOutOfFlowPositioned();
  }

  bool IsBox() const { return layout_object_->IsBox(); }

  bool IsBoxModelObject() const { return layout_object_->IsBoxModelObject(); }

  bool IsBR() const { return layout_object_->IsBR(); }

  bool IsCombineText() const { return layout_object_->IsCombineText(); }

  bool IsHorizontalWritingMode() const {
    return layout_object_->IsHorizontalWritingMode();
  }

  bool IsImage() const { return layout_object_->IsImage(); }

  bool IsInline() const { return layout_object_->IsInline(); }

  bool IsInlineBlockOrInlineTable() const {
    return layout_object_->IsInlineBlockOrInlineTable();
  }

  bool IsInlineElementContinuation() const {
    return layout_object_->IsInlineElementContinuation();
  }

  // TODO(dgrogan/eae): Replace isType with an enum in the API? As it stands
  // we mix isProperty and isType, which is confusing.
  bool IsLayoutBlock() const { return layout_object_->IsLayoutBlock(); }

  bool IsLayoutBlockFlow() const { return layout_object_->IsLayoutBlockFlow(); }

  bool IsLayoutInline() const { return layout_object_->IsLayoutInline(); }

  bool IsListMarker() const { return layout_object_->IsListMarker(); }

  bool IsAtomicInlineLevel() const {
    return layout_object_->IsAtomicInlineLevel();
  }

  bool IsRubyText() const { return layout_object_->IsRubyText(); }

  bool IsRubyRun() const { return layout_object_->IsRubyRun(); }

  bool IsRubyBase() const { return layout_object_->IsRubyBase(); }

  bool IsSVGInline() const { return layout_object_->IsSVGInline(); }

  bool IsSVGInlineText() const { return layout_object_->IsSVGInlineText(); }

  bool IsSVGText() const { return layout_object_->IsSVGText(); }

  bool IsSVGTextPath() const { return layout_object_->IsSVGTextPath(); }

  bool IsTableCell() const { return layout_object_->IsTableCell(); }

  bool IsText() const { return layout_object_->IsText(); }

  bool IsEmptyText() const {
    return IsText() && ToLayoutText(layout_object_)->GetText().IsEmpty();
  }

  bool HasLayer() const { return layout_object_->HasLayer(); }

  bool SelfNeedsLayout() const { return layout_object_->SelfNeedsLayout(); }

  // TODO(dgrogan/eae): Why does layoutObject need to know if its ancestor
  // line box is dirty at all?
  void SetAncestorLineBoxDirty() const {
    layout_object_->SetAncestorLineBoxDirty();
  }

  int CaretMinOffset() const { return layout_object_->CaretMinOffset(); }

  int CaretMaxOffset() const { return layout_object_->CaretMaxOffset(); }

  bool HasFlippedBlocksWritingMode() const {
    return layout_object_->HasFlippedBlocksWritingMode();
  }

  bool VisibleToHitTestRequest(const HitTestRequest& request) const {
    return layout_object_->VisibleToHitTestRequest(request);
  }

  bool HitTest(HitTestResult& result,
               const HitTestLocation& location_in_container,
               const LayoutPoint& accumulated_offset,
               HitTestFilter filter = kHitTestAll) {
    return layout_object_->HitTest(result, location_in_container,
                                   accumulated_offset, filter);
  }

  SelectionState GetSelectionState() const {
    return layout_object_->GetSelectionState();
  }

  // TODO(dgrogan/eae): Can we move this to style?
  Color SelectionBackgroundColor() const {
    return layout_object_->SelectionBackgroundColor();
  }

  // TODO(dgrogan/eae): Needed for Color::current. Can we move this somewhere?
  Color ResolveColor(const ComputedStyle& style_to_use, int color_property) {
    return layout_object_->ResolveColor(style_to_use, color_property);
  }

  bool IsInFlowPositioned() const {
    return layout_object_->IsInFlowPositioned();
  }

  // TODO(dgrogan/eae): Can we change this to GlobalToLocal and vice versa
  // instead of having 4 methods? See localToAbsoluteQuad below.
  PositionWithAffinity PositionForPoint(const LayoutPoint& point) {
    return layout_object_->PositionForPoint(point);
  }

  PositionWithAffinity CreatePositionWithAffinity(int offset,
                                                  TextAffinity affinity) {
    return layout_object_->CreatePositionWithAffinity(offset, affinity);
  }

  LineLayoutItem PreviousInPreOrder(const LayoutObject* stay_within) const {
    return LineLayoutItem(layout_object_->PreviousInPreOrder(stay_within));
  }

  FloatQuad LocalToAbsoluteQuad(const FloatQuad& quad,
                                MapCoordinatesFlags mode = 0) const {
    return layout_object_->LocalToAbsoluteQuad(quad, mode);
  }

  FloatPoint LocalToAbsolute(const FloatPoint& local_point = FloatPoint(),
                             MapCoordinatesFlags flags = 0) const {
    return layout_object_->LocalToAbsolute(local_point, flags);
  }

  bool HasOverflowClip() const { return layout_object_->HasOverflowClip(); }

  // TODO(dgrogan/eae): Can we instead add a TearDown method to the API
  // instead of exposing this and other shutdown code to line layout?
  bool DocumentBeingDestroyed() const {
    return layout_object_->DocumentBeingDestroyed();
  }

  LayoutRect VisualRect() const { return layout_object_->VisualRect(); }

  bool IsHashTableDeletedValue() const {
    return layout_object_ == kHashTableDeletedValue;
  }

  void SetShouldDoFullPaintInvalidation() {
    layout_object_->SetShouldDoFullPaintInvalidation();
  }

  void SlowSetPaintingLayerNeedsRepaint() {
    ObjectPaintInvalidator(*layout_object_).SlowSetPaintingLayerNeedsRepaint();
  }

  void SetIsTruncated(bool set_truncation) {
    layout_object_->SetIsTruncated(set_truncation);
  }

  struct LineLayoutItemHash {
    STATIC_ONLY(LineLayoutItemHash);
    static unsigned GetHash(const LineLayoutItem& key) {
      return WTF::PtrHash<LayoutObject>::GetHash(key.layout_object_);
    }
    static bool Equal(const LineLayoutItem& a, const LineLayoutItem& b) {
      return WTF::PtrHash<LayoutObject>::Equal(a.layout_object_,
                                               b.layout_object_);
    }
    static const bool safe_to_compare_to_empty_or_deleted = true;
  };

#ifndef NDEBUG

  const char* GetName() const { return layout_object_->GetName(); }

  // Intentionally returns a void* to avoid exposing LayoutObject* to the line
  // layout code.
  void* DebugPointer() const { return layout_object_; }

  void ShowTreeForThis() const { layout_object_->ShowTreeForThis(); }

  String DecoratedName() const { return layout_object_->DecoratedName(); }

#endif

 protected:
  LayoutObject* GetLayoutObject() { return layout_object_; }
  const LayoutObject* GetLayoutObject() const { return layout_object_; }

 private:
  LayoutObject* layout_object_;

  friend class LayoutBlockFlow;
  friend class LineLayoutAPIShim;
  friend class LineLayoutBlockFlow;
  friend class LineLayoutBox;
  friend class LineLayoutRubyRun;
};

}  // namespace blink

namespace WTF {

template <>
struct DefaultHash<blink::LineLayoutItem> {
  using Hash = blink::LineLayoutItem::LineLayoutItemHash;
};

template <>
struct HashTraits<blink::LineLayoutItem>
    : SimpleClassHashTraits<blink::LineLayoutItem> {
  STATIC_ONLY(HashTraits);
};

}  // namespace WTF

#endif  // LineLayoutItem_h
