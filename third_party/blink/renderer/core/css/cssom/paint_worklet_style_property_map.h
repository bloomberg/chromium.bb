// Copyright 2018 the Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSSOM_PAINT_WORKLET_STYLE_PROPERTY_MAP_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSSOM_PAINT_WORKLET_STYLE_PROPERTY_MAP_H_

#include "base/macros.h"
#include "third_party/blink/renderer/core/css/cssom/cross_thread_style_value.h"
#include "third_party/blink/renderer/core/css/cssom/style_property_map_read_only.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

// This class is designed for CSS Paint such that it can be safely passed cross
// threads.
//
// Here is a typical usage.
// At CSSPaintValue::GetImage which is on the main thread, build an instance
// of Blink::PaintWorkletInput which calls the constructor of this class. The
// ownership of this style map belongs to the Blink::PaintWorkletInput, which
// will eventually be passed to the worklet thread and used in the JS callback.
class CORE_EXPORT PaintWorkletStylePropertyMap
    : public StylePropertyMapReadOnly {
 public:
  using StylePropertyMapEntry = std::pair<String, CSSStyleValueVector>;
  // This constructor should be called on main-thread only.
  PaintWorkletStylePropertyMap(const Document&,
                               const ComputedStyle&,
                               Node* styled_node,
                               const Vector<CSSPropertyID>& native_properties,
                               const Vector<AtomicString>& custom_properties);

  CSSStyleValue* get(const ExecutionContext*,
                     const String& property_name,
                     ExceptionState&) override;

  CSSStyleValueVector getAll(const ExecutionContext*,
                             const String& property_name,
                             ExceptionState&) override;

  bool has(const ExecutionContext*,
           const String& property_name,
           ExceptionState&) override;

  unsigned int size() override;

  void Trace(blink::Visitor*) override;

  const HashMap<String, std::unique_ptr<CrossThreadStyleValue>>&
  ValuesForTest() {
    return values_;
  }

 private:
  IterationSource* StartIteration(ScriptState*, ExceptionState&) override;

  void BuildNativeValues(const ComputedStyle&,
                         Node* styled_node,
                         const Vector<CSSPropertyID>& native_properties);
  void BuildCustomValues(const Document&,
                         const ComputedStyle&,
                         Node* styled_node,
                         const Vector<AtomicString>& custom_properties);

  HashMap<String, std::unique_ptr<CrossThreadStyleValue>> values_;

  DISALLOW_COPY_AND_ASSIGN(PaintWorkletStylePropertyMap);
};

}  // namespace blink

#endif
