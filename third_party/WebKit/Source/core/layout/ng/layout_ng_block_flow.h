// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LayoutNGBlockFlow_h
#define LayoutNGBlockFlow_h

#include "core/CoreExport.h"
#include "core/layout/LayoutBlockFlow.h"
#include "core/layout/ng/inline/ng_inline_node_data.h"
#include "core/layout/ng/ng_block_node.h"

namespace blink {

// This overrides the default layout block algorithm to use Layout NG.
class CORE_EXPORT LayoutNGBlockFlow final : public LayoutBlockFlow {
 public:
  explicit LayoutNGBlockFlow(Element*);
  ~LayoutNGBlockFlow() override = default;

  void UpdateBlockLayout(bool relayout_children) override;

  const char* GetName() const override { return "LayoutNGBlockFlow"; }

  NGInlineNodeData& GetNGInlineNodeData() const;
  void ResetNGInlineNodeData();
  bool HasNGInlineNodeData() const { return ng_inline_node_data_.get(); }

  int FirstLineBoxBaseline() const override;
  int InlineBlockBaseline(LineDirectionMode) const override;

 private:
  bool IsOfType(LayoutObjectType) const override;

  std::unique_ptr<NGInlineNodeData> ng_inline_node_data_;
};

DEFINE_LAYOUT_OBJECT_TYPE_CASTS(LayoutNGBlockFlow, IsLayoutNGBlockFlow());

}  // namespace blink

#endif  // LayoutNGBlockFlow_h
