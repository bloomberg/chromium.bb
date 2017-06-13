// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/csspaint/CSSPaintDefinition.h"

#include "bindings/core/v8/V8BindingForCore.h"
#include "core/css/CSSComputedStyleDeclaration.h"
#include "core/css/cssom/FilteredComputedStylePropertyMap.h"
#include "core/dom/ExecutionContext.h"
#include "core/layout/LayoutObject.h"
#include "modules/csspaint/PaintRenderingContext2D.h"
#include "modules/csspaint/PaintSize.h"
#include "platform/bindings/ScriptState.h"
#include "platform/bindings/V8BindingMacros.h"
#include "platform/bindings/V8ObjectConstructor.h"
#include "platform/graphics/ImageBuffer.h"
#include "platform/graphics/PaintGeneratedImage.h"
#include "platform/graphics/RecordingImageBufferSurface.h"
#include "platform/wtf/PtrUtil.h"

namespace blink {

namespace {

IntSize GetSpecifiedSize(const IntSize& size, float zoom) {
  float un_zoom_factor = 1 / zoom;
  auto un_zoom_fn = [un_zoom_factor](int a) -> int {
    return round(a * un_zoom_factor);
  };
  return IntSize(un_zoom_fn(size.Width()), un_zoom_fn(size.Height()));
}

}  // namespace

CSSPaintDefinition* CSSPaintDefinition::Create(
    ScriptState* script_state,
    v8::Local<v8::Function> constructor,
    v8::Local<v8::Function> paint,
    Vector<CSSPropertyID>& native_invalidation_properties,
    Vector<AtomicString>& custom_invalidation_properties,
    Vector<CSSSyntaxDescriptor>& input_argument_types,
    bool has_alpha) {
  return new CSSPaintDefinition(
      script_state, constructor, paint, native_invalidation_properties,
      custom_invalidation_properties, input_argument_types, has_alpha);
}

CSSPaintDefinition::CSSPaintDefinition(
    ScriptState* script_state,
    v8::Local<v8::Function> constructor,
    v8::Local<v8::Function> paint,
    Vector<CSSPropertyID>& native_invalidation_properties,
    Vector<AtomicString>& custom_invalidation_properties,
    Vector<CSSSyntaxDescriptor>& input_argument_types,
    bool has_alpha)
    : script_state_(script_state),
      constructor_(script_state->GetIsolate(), this, constructor),
      paint_(script_state->GetIsolate(), this, paint),
      instance_(this),
      did_call_constructor_(false),
      has_alpha_(has_alpha) {
  native_invalidation_properties_.swap(native_invalidation_properties);
  custom_invalidation_properties_.swap(custom_invalidation_properties);
  input_argument_types_.swap(input_argument_types);
}

CSSPaintDefinition::~CSSPaintDefinition() {}

PassRefPtr<Image> CSSPaintDefinition::Paint(
    const ImageResourceObserver& client,
    const IntSize& size,
    const CSSStyleValueVector* paint_arguments) {
  DCHECK(paint_arguments);

  // TODO: Break dependency on LayoutObject. Passing the Node should work.
  const LayoutObject& layout_object = static_cast<const LayoutObject&>(client);

  float zoom = layout_object.StyleRef().EffectiveZoom();
  const IntSize specified_size = GetSpecifiedSize(size, zoom);

  ScriptState::Scope scope(script_state_.Get());

  MaybeCreatePaintInstance();

  v8::Isolate* isolate = script_state_->GetIsolate();
  v8::Local<v8::Object> instance = instance_.NewLocal(isolate);

  // We may have failed to create an instance class, in which case produce an
  // invalid image.
  if (IsUndefinedOrNull(instance))
    return nullptr;

  DCHECK(layout_object.GetNode());

  PaintRenderingContext2D* rendering_context = PaintRenderingContext2D::Create(
      ImageBuffer::Create(WTF::WrapUnique(new RecordingImageBufferSurface(
          size, RecordingImageBufferSurface::kDisallowFallback,
          has_alpha_ ? kNonOpaque : kOpaque))),
      has_alpha_, zoom);
  PaintSize* paint_size = PaintSize::Create(specified_size);
  StylePropertyMapReadonly* style_map =
      FilteredComputedStylePropertyMap::Create(
          CSSComputedStyleDeclaration::Create(layout_object.GetNode()),
          native_invalidation_properties_, custom_invalidation_properties_,
          layout_object.GetNode());

  v8::Local<v8::Value> argv[] = {
      ToV8(rendering_context, script_state_->GetContext()->Global(), isolate),
      ToV8(paint_size, script_state_->GetContext()->Global(), isolate),
      ToV8(style_map, script_state_->GetContext()->Global(), isolate),
      ToV8(*paint_arguments, script_state_->GetContext()->Global(), isolate)};

  v8::Local<v8::Function> paint = paint_.NewLocal(isolate);

  v8::TryCatch block(isolate);
  block.SetVerbose(true);

  V8ScriptRunner::CallFunction(paint,
                               ExecutionContext::From(script_state_.Get()),
                               instance, WTF_ARRAY_LENGTH(argv), argv, isolate);

  // The paint function may have produced an error, in which case produce an
  // invalid image.
  if (block.HasCaught()) {
    return nullptr;
  }

  return PaintGeneratedImage::Create(
      rendering_context->GetImageBuffer()->GetRecord(), specified_size);
}

void CSSPaintDefinition::MaybeCreatePaintInstance() {
  if (did_call_constructor_)
    return;

  DCHECK(instance_.IsEmpty());

  v8::Isolate* isolate = script_state_->GetIsolate();
  v8::Local<v8::Function> constructor = constructor_.NewLocal(isolate);
  DCHECK(!IsUndefinedOrNull(constructor));

  v8::Local<v8::Object> paint_instance;
  if (V8ObjectConstructor::NewInstance(isolate, constructor)
          .ToLocal(&paint_instance)) {
    instance_.Set(isolate, paint_instance);
  }

  did_call_constructor_ = true;
}

DEFINE_TRACE_WRAPPERS(CSSPaintDefinition) {
  visitor->TraceWrappers(constructor_.Cast<v8::Value>());
  visitor->TraceWrappers(paint_.Cast<v8::Value>());
  visitor->TraceWrappers(instance_.Cast<v8::Value>());
}

}  // namespace blink
