// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_CUSTOM_CUSTOM_LAYOUT_CONSTRAINTS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_CUSTOM_CUSTOM_LAYOUT_CONSTRAINTS_H_

#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/bindings/trace_wrapper_v8_reference.h"
#include "third_party/blink/renderer/platform/heap/handle.h"

namespace blink {

struct LogicalSize;
class ScriptState;
class ScriptValue;
class SerializedScriptValue;

// Represents the constraints given to the layout by the parent that isn't
// encapsulated by the style, or edges.
class CustomLayoutConstraints : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  CustomLayoutConstraints(const LogicalSize& border_box_size,
                          SerializedScriptValue* data,
                          v8::Isolate*);
  ~CustomLayoutConstraints() override;

  // LayoutConstraints.idl
  double fixedInlineSize() const { return fixed_inline_size_; }
  base::Optional<double> fixedBlockSize() const;
  ScriptValue data(ScriptState*) const;

  void Trace(Visitor*) override;

 private:
  double fixed_inline_size_;
  double fixed_block_size_;
  TraceWrapperV8Reference<v8::Value> layout_worklet_world_v8_data_;

  DISALLOW_COPY_AND_ASSIGN(CustomLayoutConstraints);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_CUSTOM_CUSTOM_LAYOUT_CONSTRAINTS_H_
