// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LayoutItem_h
#define LayoutItem_h

#include "core/inspector/InspectorTraceEvents.h"
#include "core/layout/LayoutObject.h"

#include "platform/wtf/Allocator.h"

namespace blink {

class LayoutAPIShim;
class LocalFrame;
class LocalFrameView;
class LayoutViewItem;
class Node;

class LayoutItem {
  DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();

 public:
  explicit LayoutItem(LayoutObject* layout_object)
      : layout_object_(layout_object) {}

  LayoutItem(std::nullptr_t) : layout_object_(0) {}

  LayoutItem() : layout_object_(0) {}

  // TODO(leviw): This should be "explicit operator bool", but
  // using this operator allows the API to be landed in pieces.
  // https://crbug.com/499321
  operator LayoutObject*() const { return layout_object_; }

  // TODO(pilgrim): Remove this when we replace the operator above with
  // operator bool.
  bool IsNull() const { return !layout_object_; }

  String DebugName() const { return layout_object_->DebugName(); }

  bool IsDescendantOf(LayoutItem item) const {
    return layout_object_->IsDescendantOf(item.GetLayoutObject());
  }

  bool IsBoxModelObject() const { return layout_object_->IsBoxModelObject(); }

  bool IsBox() const { return layout_object_->IsBox(); }

  bool IsBR() const { return layout_object_->IsBR(); }

  bool IsLayoutBlock() const { return layout_object_->IsLayoutBlock(); }

  bool IsText() const { return layout_object_->IsText(); }

  bool IsTextControl() const { return layout_object_->IsTextControl(); }

  bool IsLayoutEmbeddedContent() const {
    return layout_object_->IsLayoutEmbeddedContent();
  }

  bool IsEmbeddedObject() const { return layout_object_->IsEmbeddedObject(); }

  bool IsImage() const { return layout_object_->IsImage(); }

  bool IsLayoutFullScreen() const {
    return layout_object_->IsLayoutFullScreen();
  }

  bool IsListItem() const { return layout_object_->IsListItem(); }

  bool IsMedia() const { return layout_object_->IsMedia(); }

  bool IsMenuList() const { return layout_object_->IsMenuList(); }

  bool IsProgress() const { return layout_object_->IsProgress(); }

  bool IsSlider() const { return layout_object_->IsSlider(); }

  bool IsLayoutView() const { return layout_object_->IsLayoutView(); }

  bool NeedsLayout() { return layout_object_->NeedsLayout(); }

  void UpdateLayout() { layout_object_->UpdateLayout(); }

  LayoutItem Container() const {
    return LayoutItem(layout_object_->Container());
  }

  Node* GetNode() const { return layout_object_->GetNode(); }

  Document& GetDocument() const { return layout_object_->GetDocument(); }

  LocalFrame* GetFrame() const { return layout_object_->GetFrame(); }

  LayoutItem NextInPreOrder() const {
    return LayoutItem(layout_object_->NextInPreOrder());
  }

  void UpdateStyleAndLayout() {
    return layout_object_->GetDocument().UpdateStyleAndLayout();
  }

  const ComputedStyle& StyleRef() const { return layout_object_->StyleRef(); }

  ComputedStyle* MutableStyle() const { return layout_object_->MutableStyle(); }

  ComputedStyle& MutableStyleRef() const {
    return layout_object_->MutableStyleRef();
  }

  void SetStyle(RefPtr<ComputedStyle> style) {
    layout_object_->SetStyle(std::move(style));
  }

  LayoutSize OffsetFromContainer(const LayoutItem& item) const {
    return layout_object_->OffsetFromContainer(item.GetLayoutObject());
  }

  LayoutViewItem View() const;

  LocalFrameView* GetFrameView() const {
    return layout_object_->GetDocument().View();
  }

  const ComputedStyle* Style() const { return layout_object_->Style(); }

  PaintLayer* EnclosingLayer() const {
    return layout_object_->EnclosingLayer();
  }

  bool HasLayer() const { return layout_object_->HasLayer(); }

  void SetNeedsLayout(LayoutInvalidationReasonForTracing reason,
                      MarkingBehavior marking = kMarkContainerChain,
                      SubtreeLayoutScope* scope = nullptr) {
    layout_object_->SetNeedsLayout(reason, marking, scope);
  }

  void SetNeedsLayoutAndFullPaintInvalidation(
      LayoutInvalidationReasonForTracing reason,
      MarkingBehavior behavior = kMarkContainerChain,
      SubtreeLayoutScope* scope = nullptr) {
    layout_object_->SetNeedsLayoutAndFullPaintInvalidation(reason, behavior,
                                                           scope);
  }

  void SetNeedsLayoutAndPrefWidthsRecalc(
      LayoutInvalidationReasonForTracing reason) {
    layout_object_->SetNeedsLayoutAndPrefWidthsRecalc(reason);
  }

  void SetNeedsLayoutAndPrefWidthsRecalcAndFullPaintInvalidation(
      LayoutInvalidationReasonForTracing reason) {
    layout_object_->SetNeedsLayoutAndPrefWidthsRecalcAndFullPaintInvalidation(
        reason);
  }

  void SetMayNeedPaintInvalidation() {
    layout_object_->SetMayNeedPaintInvalidation();
  }

  void SetShouldDoFullPaintInvalidation(
      PaintInvalidationReason reason = PaintInvalidationReason::kFull) {
    layout_object_->SetShouldDoFullPaintInvalidation(reason);
  }

  void SetShouldDoFullPaintInvalidationIncludingNonCompositingDescendants() {
    layout_object_
        ->SetShouldDoFullPaintInvalidationIncludingNonCompositingDescendants();
  }

  void ComputeLayerHitTestRects(LayerHitTestRects& layer_rects) const {
    layout_object_->ComputeLayerHitTestRects(layer_rects);
  }

  FloatPoint LocalToAbsolute(const FloatPoint& local_point = FloatPoint(),
                             MapCoordinatesFlags mode = 0) const {
    return layout_object_->LocalToAbsolute(local_point, mode);
  }

  FloatQuad LocalToAbsoluteQuad(const FloatQuad& quad,
                                MapCoordinatesFlags mode = 0) const {
    return layout_object_->LocalToAbsoluteQuad(quad, mode);
  }

  FloatPoint AbsoluteToLocal(const FloatPoint& point,
                             MapCoordinatesFlags mode = 0) const {
    return layout_object_->AbsoluteToLocal(point, mode);
  }

  bool WasNotifiedOfSubtreeChange() const {
    return layout_object_->WasNotifiedOfSubtreeChange();
  }

  void HandleSubtreeModifications() {
    layout_object_->HandleSubtreeModifications();
  }

  bool NeedsOverflowRecalcAfterStyleChange() const {
    return layout_object_->NeedsOverflowRecalcAfterStyleChange();
  }

  CompositingState GetCompositingState() const {
    return layout_object_->GetCompositingState();
  }

  bool MapToVisualRectInAncestorSpace(
      const LayoutBoxModelObject* ancestor,
      LayoutRect& layout_rect,
      VisualRectFlags flags = kDefaultVisualRectFlags) const {
    return layout_object_->MapToVisualRectInAncestorSpace(ancestor, layout_rect,
                                                          flags);
  }

  Color ResolveColor(int color_property) const {
    return layout_object_->ResolveColor(color_property);
  }

  void InvalidatePaintRectangle(const LayoutRect& dirty_rect) const {
    layout_object_->InvalidatePaintRectangle(dirty_rect);
  }

  RefPtr<ComputedStyle> GetUncachedPseudoStyle(
      const PseudoStyleRequest& pseudo_style_request,
      const ComputedStyle* parent_style = nullptr) const {
    return layout_object_->GetUncachedPseudoStyle(pseudo_style_request,
                                                  parent_style);
  }

 protected:
  LayoutObject* GetLayoutObject() { return layout_object_; }
  const LayoutObject* GetLayoutObject() const { return layout_object_; }

 private:
  LayoutObject* layout_object_;

  friend class LayoutAPIShim;
};

}  // namespace blink

#endif  // LayoutItem_h
