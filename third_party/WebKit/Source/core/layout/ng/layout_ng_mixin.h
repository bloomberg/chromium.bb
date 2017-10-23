// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LayoutNGMixin_h
#define LayoutNGMixin_h

#include "build/build_config.h"
#include "core/layout/LayoutBlockFlow.h"
#include "core/layout/ng/inline/ng_inline_node_data.h"
#include "core/layout/ng/ng_constraint_space.h"
#include "core/layout/ng/ng_physical_box_fragment.h"
#include "core/paint/ng/ng_paint_fragment.h"

namespace blink {

class NGBreakToken;
class NGLayoutResult;

// This mixin holds code shared between LayoutNG subclasses of
// LayoutBlockFlow.

template <typename Base>
class CORE_TEMPLATE_CLASS_EXPORT LayoutNGMixin : public Base {
 public:
  explicit LayoutNGMixin(Element* element) : Base(element) {}
  ~LayoutNGMixin() override;

  NGInlineNodeData* GetNGInlineNodeData() const override;
  void ResetNGInlineNodeData() override;
  bool HasNGInlineNodeData() const override {
    return ng_inline_node_data_.get();
  }

  LayoutUnit FirstLineBoxBaseline() const override;
  LayoutUnit InlineBlockBaseline(LineDirectionMode) const override;

  void Paint(const PaintInfo&, const LayoutPoint&) const override;

  bool NodeAtPoint(HitTestResult&,
                   const HitTestLocation& location_in_container,
                   const LayoutPoint& accumulated_offset,
                   HitTestAction) override;

  // Returns the last layout result for this block flow with the given
  // constraint space and break token, or null if it is not up-to-date or
  // otherwise unavailable.
  scoped_refptr<NGLayoutResult> CachedLayoutResult(
      const NGConstraintSpace&,
      NGBreakToken*) const override;

  void SetCachedLayoutResult(const NGConstraintSpace&,
                             NGBreakToken*,
                             scoped_refptr<NGLayoutResult>) override;
  // For testing only.
  scoped_refptr<NGLayoutResult> CachedLayoutResultForTesting() override;

  NGPaintFragment* PaintFragment() const override {
    return paint_fragment_.get();
  }
  void SetPaintFragment(scoped_refptr<const NGPhysicalFragment>) override;

 protected:
  bool IsOfType(LayoutObject::LayoutObjectType) const override;

  void AddOverflowFromChildren() override;

  const NGPhysicalBoxFragment* CurrentFragment() const override;

  const NGBaseline* FragmentBaseline(NGBaselineAlgorithmType) const;

  void UpdateMargins(const NGConstraintSpace&);

  std::unique_ptr<NGInlineNodeData> ng_inline_node_data_;

  scoped_refptr<NGLayoutResult> cached_result_;
  scoped_refptr<const NGConstraintSpace> cached_constraint_space_;
  std::unique_ptr<NGPaintFragment> paint_fragment_;

  friend class NGBaseLayoutAlgorithmTest;
};

extern template class CORE_EXTERN_TEMPLATE_EXPORT
    LayoutNGMixin<LayoutBlockFlow>;

}  // namespace blink

#endif  // LayoutNGMixin_h
