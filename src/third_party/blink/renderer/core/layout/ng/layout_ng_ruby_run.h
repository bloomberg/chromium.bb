// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_LAYOUT_NG_RUBY_RUN_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_LAYOUT_NG_RUBY_RUN_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/layout_ruby_run.h"
#include "third_party/blink/renderer/core/layout/ng/layout_ng_block_flow_mixin.h"

namespace blink {

// A LayoutNG version of LayoutRubyRun.
class CORE_EXPORT LayoutNGRubyRun final
    : public LayoutNGBlockFlowMixin<LayoutRubyRun> {
 public:
  explicit LayoutNGRubyRun();
  ~LayoutNGRubyRun() override;

  const char* GetName() const override { return "LayoutNGRubyRun"; }
  void UpdateBlockLayout(bool relayout_children) override;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_LAYOUT_NG_RUBY_RUN_H_
