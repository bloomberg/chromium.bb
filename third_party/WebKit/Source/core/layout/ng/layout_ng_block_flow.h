// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LayoutNGBlockFlow_h
#define LayoutNGBlockFlow_h

#include "core/CoreExport.h"
#include "core/layout/LayoutBlockFlow.h"
#include "core/layout/ng/ng_physical_box_fragment.h"
#include "core/paint/ng/ng_paint_fragment.h"

namespace blink {

class NGBreakToken;
class NGConstraintSpace;
class NGLayoutResult;
enum class NGBaselineAlgorithmType;
struct NGBaseline;
struct NGInlineNodeData;

// This overrides the default layout block algorithm to use Layout NG.
class CORE_EXPORT LayoutNGBlockFlow : public LayoutBlockFlow {
 public:
  explicit LayoutNGBlockFlow(Element*);
  ~LayoutNGBlockFlow() override;

  void UpdateBlockLayout(bool relayout_children) override;

  const char* GetName() const override { return "LayoutNGBlockFlow"; }

  NGInlineNodeData* GetNGInlineNodeData() const;
  void ResetNGInlineNodeData();
  bool HasNGInlineNodeData() const { return ng_inline_node_data_.get(); }
  virtual void WillCollectInlines();

  LayoutUnit FirstLineBoxBaseline() const override;
  LayoutUnit InlineBlockBaseline(LineDirectionMode) const override;

  void PaintObject(const PaintInfo&, const LayoutPoint&) const override;

  // Returns the last layout result for this block flow with the given
  // constraint space and break token, or null if it is not up-to-date or
  // otherwise unavailable.
  RefPtr<NGLayoutResult> CachedLayoutResult(const NGConstraintSpace&,
                                            NGBreakToken*) const;

  void SetCachedLayoutResult(const NGConstraintSpace&,
                             NGBreakToken*,
                             RefPtr<NGLayoutResult>);

  const NGPaintFragment* PaintFragment() const { return paint_fragment_.get(); }

 protected:
  bool IsOfType(LayoutObjectType) const override;

  void AddOverflowFromChildren() override;

 private:
  void UpdateOutOfFlowBlockLayout();

  const NGPhysicalFragment* CurrentFragment() const;

  const NGBaseline* FragmentBaseline(NGBaselineAlgorithmType) const;

  void UpdateMargins(const NGConstraintSpace&);

  std::unique_ptr<NGInlineNodeData> ng_inline_node_data_;

  RefPtr<NGLayoutResult> cached_result_;
  RefPtr<const NGConstraintSpace> cached_constraint_space_;
  std::unique_ptr<const NGPaintFragment> paint_fragment_;
};

DEFINE_LAYOUT_OBJECT_TYPE_CASTS(LayoutNGBlockFlow, IsLayoutNGBlockFlow());

}  // namespace blink

#endif  // LayoutNGBlockFlow_h
