// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_CUSTOM_CUSTOM_LAYOUT_EDGES_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_CUSTOM_CUSTOM_LAYOUT_EDGES_H_

#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/bindings/trace_wrapper_member.h"
#include "third_party/blink/renderer/platform/heap/handle.h"

namespace blink {

class LayoutCustom;
class LayoutUnit;

// Represents the border, scrollbar, and padding edges given to the custom
// layout.
class CustomLayoutEdges : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static CustomLayoutEdges* Create(const LayoutCustom&);
  ~CustomLayoutEdges() override;

  // layout_edges.idl
  double inlineStart() const { return inline_start_; }
  double inlineEnd() const { return inline_end_; }
  double blockStart() const { return block_start_; }
  double blockEnd() const { return block_end_; }
  double inlineSum() const { return inline_start_ + inline_end_; }
  double blockSum() const { return block_start_ + block_end_; }

 private:
  CustomLayoutEdges(LayoutUnit inline_start,
                    LayoutUnit inline_end,
                    LayoutUnit block_start,
                    LayoutUnit block_end);

  double inline_start_;
  double inline_end_;
  double block_start_;
  double block_end_;

  DISALLOW_COPY_AND_ASSIGN(CustomLayoutEdges);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_CUSTOM_CUSTOM_LAYOUT_EDGES_H_
