// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/csspaint/css_paint_definition.h"

#include <memory>

#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_no_argument_constructor.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_paint_callback.h"
#include "third_party/blink/renderer/core/css/css_computed_style_declaration.h"
#include "third_party/blink/renderer/core/css/cssom/cross_thread_color_value.h"
#include "third_party/blink/renderer/core/css/cssom/cross_thread_unit_value.h"
#include "third_party/blink/renderer/core/css/cssom/css_paint_worklet_input.h"
#include "third_party/blink/renderer/core/css/cssom/prepopulated_computed_style_property_map.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/modules/csspaint/paint_rendering_context_2d.h"
#include "third_party/blink/renderer/modules/csspaint/paint_size.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/v8_binding_macros.h"
#include "third_party/blink/renderer/platform/graphics/paint_generated_image.h"

namespace blink {

namespace {

FloatSize GetSpecifiedSize(const FloatSize& size, float zoom) {
  float un_zoom_factor = 1 / zoom;
  auto un_zoom_fn = [un_zoom_factor](float a) -> float {
    return a * un_zoom_factor;
  };
  return FloatSize(un_zoom_fn(size.Width()), un_zoom_fn(size.Height()));
}

}  // namespace

CSSPaintDefinition::CSSPaintDefinition(
    ScriptState* script_state,
    V8NoArgumentConstructor* constructor,
    V8PaintCallback* paint,
    const Vector<CSSPropertyID>& native_invalidation_properties,
    const Vector<AtomicString>& custom_invalidation_properties,
    const Vector<CSSSyntaxDefinition>& input_argument_types,
    const PaintRenderingContext2DSettings* context_settings)
    : script_state_(script_state),
      constructor_(constructor),
      paint_(paint),
      did_call_constructor_(false),
      context_settings_(context_settings) {
  native_invalidation_properties_ = native_invalidation_properties;
  custom_invalidation_properties_ = custom_invalidation_properties;
  input_argument_types_ = input_argument_types;
}

CSSPaintDefinition::~CSSPaintDefinition() = default;

// PaintDefinition override
sk_sp<PaintRecord> CSSPaintDefinition::Paint(
    const CompositorPaintWorkletInput* compositor_input,
    const CompositorPaintWorkletJob::AnimatedPropertyValues&
        animated_property_values) {
  // TODO(crbug.com/1227698): Use To<> with checks instead of static_cast.
  const CSSPaintWorkletInput* input =
      static_cast<const CSSPaintWorkletInput*>(compositor_input);
  PaintWorkletStylePropertyMap* style_map =
      MakeGarbageCollected<PaintWorkletStylePropertyMap>(input->StyleMapData());
  CSSStyleValueVector paint_arguments;
  for (const auto& style_value : input->ParsedInputArguments()) {
    paint_arguments.push_back(style_value->ToCSSStyleValue());
  }

  ApplyAnimatedPropertyOverrides(style_map, animated_property_values);

  sk_sp<PaintRecord> result =
      Paint(FloatSize(input->GetSize()), input->EffectiveZoom(), style_map,
            &paint_arguments, input->DeviceScaleFactor());

  // Return empty record if paint fails.
  if (!result)
    result = sk_make_sp<PaintRecord>();
  return result;
}

sk_sp<PaintRecord> CSSPaintDefinition::Paint(
    const FloatSize& container_size,
    float zoom,
    StylePropertyMapReadOnly* style_map,
    const CSSStyleValueVector* paint_arguments,
    float device_scale_factor) {
  const FloatSize specified_size = GetSpecifiedSize(container_size, zoom);
  ScriptState::Scope scope(script_state_);

  MaybeCreatePaintInstance();
  // We may have failed to create an instance, in which case produce an
  // invalid image.
  if (instance_.IsEmpty())
    return nullptr;

  v8::Isolate* isolate = script_state_->GetIsolate();

  // Do subpixel snapping for the |container_size|.
  auto* rendering_context = MakeGarbageCollected<PaintRenderingContext2D>(
      RoundedIntSize(container_size), context_settings_, zoom,
      device_scale_factor);
  PaintSize* paint_size = MakeGarbageCollected<PaintSize>(specified_size);

  CSSStyleValueVector empty_paint_arguments;
  if (!paint_arguments)
    paint_arguments = &empty_paint_arguments;

  v8::TryCatch try_catch(isolate);
  try_catch.SetVerbose(true);

  // The paint function may have produced an error, in which case produce an
  // invalid image.
  if (paint_
          ->Invoke(instance_.NewLocal(isolate), rendering_context, paint_size,
                   style_map, *paint_arguments)
          .IsNothing()) {
    return nullptr;
  }

  return rendering_context->GetRecord();
}

void CSSPaintDefinition::ApplyAnimatedPropertyOverrides(
    PaintWorkletStylePropertyMap* style_map,
    const CompositorPaintWorkletJob::AnimatedPropertyValues&
        animated_property_values) {
  for (const auto& property_value : animated_property_values) {
    DCHECK(property_value.second.has_value());
    String property_name(
        property_value.first.custom_property_name.value().c_str());
    DCHECK(style_map->StyleMapData().Contains(property_name));
    CrossThreadStyleValue* old_value =
        style_map->StyleMapData().at(property_name);
    switch (old_value->GetType()) {
      case CrossThreadStyleValue::StyleValueType::kUnitType: {
        DCHECK(property_value.second.float_value);
        std::unique_ptr<CrossThreadUnitValue> new_value =
            std::make_unique<CrossThreadUnitValue>(
                property_value.second.float_value.value(),
                DynamicTo<CrossThreadUnitValue>(old_value)->GetUnitType());
        style_map->StyleMapData().Set(property_name, std::move(new_value));
        break;
      }
      case CrossThreadStyleValue::StyleValueType::kColorType: {
        DCHECK(property_value.second.color_value);
        SkColor sk_color = property_value.second.color_value.value();
        Color color(MakeRGBA(SkColorGetR(sk_color), SkColorGetG(sk_color),
                             SkColorGetB(sk_color), SkColorGetA(sk_color)));
        std::unique_ptr<CrossThreadColorValue> new_value =
            std::make_unique<CrossThreadColorValue>(color);
        style_map->StyleMapData().Set(property_name, std::move(new_value));
        break;
      }
      default:
        NOTREACHED();
        break;
    }
  }
}

void CSSPaintDefinition::MaybeCreatePaintInstance() {
  if (did_call_constructor_)
    return;
  did_call_constructor_ = true;

  DCHECK(instance_.IsEmpty());

  ScriptValue paint_instance;
  if (!constructor_->Construct().To(&paint_instance))
    return;

  instance_.Set(constructor_->GetIsolate(), paint_instance.V8Value());
}

void CSSPaintDefinition::Trace(Visitor* visitor) const {
  visitor->Trace(constructor_);
  visitor->Trace(paint_);
  visitor->Trace(instance_);
  visitor->Trace(context_settings_);
  visitor->Trace(script_state_);
  PaintDefinition::Trace(visitor);
}

}  // namespace blink
