// Copyright 2018 the Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/cssom/paint_worklet_style_property_map.h"

#include "third_party/blink/renderer/core/css/css_custom_property_declaration.h"
#include "third_party/blink/renderer/core/css/css_variable_data.h"
#include "third_party/blink/renderer/core/css/cssom/computed_style_property_map.h"
#include "third_party/blink/renderer/core/css/cssom/css_unparsed_value.h"
#include "third_party/blink/renderer/core/css/cssom/css_unsupported_style_value.h"
#include "third_party/blink/renderer/core/css/properties/css_property_ref.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/style/computed_style.h"

namespace blink {

namespace {

class PaintWorkletStylePropertyMapIterationSource final
    : public PairIterable<String, CSSStyleValueVector>::IterationSource {
 public:
  explicit PaintWorkletStylePropertyMapIterationSource(
      HeapVector<PaintWorkletStylePropertyMap::StylePropertyMapEntry> values)
      : index_(0), values_(values) {}

  bool Next(ScriptState*,
            String& key,
            CSSStyleValueVector& value,
            ExceptionState&) override {
    if (index_ >= values_.size())
      return false;

    const PaintWorkletStylePropertyMap::StylePropertyMapEntry& pair =
        values_.at(index_++);
    key = pair.first;
    value = pair.second;
    return true;
  }

  void Trace(blink::Visitor* visitor) override {
    visitor->Trace(values_);
    PairIterable<String, CSSStyleValueVector>::IterationSource::Trace(visitor);
  }

 private:
  wtf_size_t index_;
  const HeapVector<PaintWorkletStylePropertyMap::StylePropertyMapEntry> values_;
};

}  // namespace

PaintWorkletStylePropertyMap::PaintWorkletStylePropertyMap(
    const Document& document,
    const ComputedStyle& style,
    Node* styled_node,
    const Vector<CSSPropertyID>& native_properties,
    const Vector<AtomicString>& custom_properties)
    : StylePropertyMapReadOnly() {
  DCHECK(IsMainThread());
  values_.ReserveCapacityForSize(native_properties.size() +
                                 custom_properties.size());
  BuildNativeValues(style, styled_node, native_properties);
  BuildCustomValues(document, style, styled_node, custom_properties);
}

void PaintWorkletStylePropertyMap::BuildNativeValues(
    const ComputedStyle& style,
    Node* styled_node,
    const Vector<CSSPropertyID>& native_properties) {
  DCHECK(IsMainThread());
  for (const auto& property_id : native_properties) {
    // Silently drop shorthand properties.
    DCHECK_NE(property_id, CSSPropertyInvalid);
    DCHECK_NE(property_id, CSSPropertyVariable);
    if (CSSProperty::Get(property_id).IsShorthand())
      continue;
    std::unique_ptr<CrossThreadStyleValue> value =
        CSSProperty::Get(property_id)
            .CrossThreadStyleValueFromComputedStyle(
                style, /* layout_object */ nullptr, styled_node,
                /* allow_visited_style */ false);
    String key = CSSProperty::Get(property_id).GetPropertyNameString();
    if (!key.IsSafeToSendToAnotherThread())
      key = key.IsolatedCopy();
    values_.Set(key, std::move(value));
  }
}

void PaintWorkletStylePropertyMap::BuildCustomValues(
    const Document& document,
    const ComputedStyle& style,
    Node* styled_node,
    const Vector<AtomicString>& custom_properties) {
  DCHECK(IsMainThread());
  for (const auto& property_name : custom_properties) {
    CSSPropertyRef ref(property_name, document);
    std::unique_ptr<CrossThreadStyleValue> value =
        ref.GetProperty().CrossThreadStyleValueFromComputedStyle(
            style, /* layout_object */ nullptr, styled_node,
            /* allow_visited_style */ false);
    // Ensure that the String can be safely passed cross threads.
    String key = property_name.GetString();
    if (!key.IsSafeToSendToAnotherThread())
      key = key.IsolatedCopy();
    values_.Set(key, std::move(value));
  }
}

CSSStyleValue* PaintWorkletStylePropertyMap::get(
    const ExecutionContext* execution_context,
    const String& property_name,
    ExceptionState& exception_state) {
  CSSStyleValueVector all_values =
      getAll(execution_context, property_name, exception_state);
  return all_values.IsEmpty() ? nullptr : all_values[0];
}

CSSStyleValueVector PaintWorkletStylePropertyMap::getAll(
    const ExecutionContext* execution_context,
    const String& property_name,
    ExceptionState& exception_state) {
  CSSPropertyID property_id = cssPropertyID(property_name);
  if (property_id == CSSPropertyInvalid) {
    exception_state.ThrowTypeError("Invalid propertyName: " + property_name);
    return CSSStyleValueVector();
  }

  DCHECK(isValidCSSPropertyID(property_id));

  CSSStyleValueVector values;
  auto value = values_.find(property_name);
  if (value == values_.end())
    return CSSStyleValueVector();
  values.push_back(value->value->ToCSSStyleValue());
  return values;
}

bool PaintWorkletStylePropertyMap::has(
    const ExecutionContext* execution_context,
    const String& property_name,
    ExceptionState& exception_state) {
  return !getAll(execution_context, property_name, exception_state).IsEmpty();
}

unsigned PaintWorkletStylePropertyMap::size() {
  return values_.size();
}

PaintWorkletStylePropertyMap::IterationSource*
PaintWorkletStylePropertyMap::StartIteration(ScriptState* script_state,
                                             ExceptionState& exception_state) {
  // TODO(xidachen): implement this function. Note that the output should be
  // sorted.
  NOTREACHED();
  HeapVector<PaintWorkletStylePropertyMap::StylePropertyMapEntry> result;
  return MakeGarbageCollected<PaintWorkletStylePropertyMapIterationSource>(
      result);
}

void PaintWorkletStylePropertyMap::Trace(blink::Visitor* visitor) {
  StylePropertyMapReadOnly::Trace(visitor);
}

}  // namespace blink
