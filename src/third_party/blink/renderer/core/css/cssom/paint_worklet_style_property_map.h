// Copyright 2018 the Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSSOM_PAINT_WORKLET_STYLE_PROPERTY_MAP_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSSOM_PAINT_WORKLET_STYLE_PROPERTY_MAP_H_

#include <memory>

#include "base/macros.h"
#include "third_party/blink/renderer/core/css/cssom/cross_thread_style_value.h"
#include "third_party/blink/renderer/core/css/cssom/style_property_map_read_only.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

// This class is designed for CSS Paint such that it can be safely passed cross
// threads.
//
// Here is a typical usage.
// At CSSPaintValue::GetImage which is on the main thread, call the
// BuildCrossThreadData and give the data to the Blink::PaintWorkletInput.
// The PaintWorkletInput is passed to the worklet thread, and we build an
// instance of PaintWorkletStylePropertyMap from the data, and the instance is
// eventually pass to the JS paint callback.
class CORE_EXPORT PaintWorkletStylePropertyMap
    : public StylePropertyMapReadOnly {
 public:
  using StylePropertyMapEntry = std::pair<String, CSSStyleValueVector>;
  using CrossThreadData =
      HashMap<String, std::unique_ptr<CrossThreadStyleValue>>;
  // Build the data that will be passed to the worklet thread to construct a
  // style map. Should be called on the main thread only.
  static base::Optional<CrossThreadData> BuildCrossThreadData(
      const Document&,
      const ComputedStyle&,
      Node* styled_node,
      const Vector<CSSPropertyID>& native_properties,
      const Vector<AtomicString>& custom_properties);

  static CrossThreadData CopyCrossThreadData(const CrossThreadData& data);

  // This constructor should be called on the worklet-thread only.
  explicit PaintWorkletStylePropertyMap(CrossThreadData data);

  CSSStyleValue* get(const ExecutionContext*,
                     const String& property_name,
                     ExceptionState&) const override;

  CSSStyleValueVector getAll(const ExecutionContext*,
                             const String& property_name,
                             ExceptionState&) const override;

  bool has(const ExecutionContext*,
           const String& property_name,
           ExceptionState&) const override;

  unsigned int size() const override;

  void Trace(blink::Visitor*) override;

  const CrossThreadData& StyleMapDataForTest() const { return data_; }

 private:
  IterationSource* StartIteration(ScriptState*, ExceptionState&) override;

  CrossThreadData data_;

  DISALLOW_COPY_AND_ASSIGN(PaintWorkletStylePropertyMap);
};

}  // namespace blink

#endif
