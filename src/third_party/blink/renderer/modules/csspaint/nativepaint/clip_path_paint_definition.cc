// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/csspaint/nativepaint/clip_path_paint_definition.h"

#include "third_party/blink/renderer/core/animation/basic_shape_interpolation_functions.h"
#include "third_party/blink/renderer/core/animation/css/compositor_keyframe_double.h"
#include "third_party/blink/renderer/core/animation/element_animations.h"
#include "third_party/blink/renderer/core/animation/path_interpolation_functions.h"
#include "third_party/blink/renderer/core/css/basic_shape_functions.h"
#include "third_party/blink/renderer/core/css/css_to_length_conversion_data.h"
#include "third_party/blink/renderer/core/css/cssom/paint_worklet_deferred_image.h"
#include "third_party/blink/renderer/core/css/cssom/paint_worklet_input.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver_state.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/style/shape_clip_path_operation.h"
#include "third_party/blink/renderer/modules/csspaint/paint_rendering_context_2d.h"
#include "third_party/blink/renderer/platform/graphics/paint_worklet_paint_dispatcher.h"
#include "third_party/blink/renderer/platform/graphics/platform_paint_worklet_layer_painter.h"
#include "ui/gfx/geometry/size_f.h"

namespace blink {

namespace {

// This class includes information that is required by the compositor thread
// when painting clip path.
class ClipPathPaintWorkletInput : public PaintWorkletInput {
 public:
  ClipPathPaintWorkletInput(
      const gfx::RectF& container_rect,
      int worklet_id,
      float zoom,
      const Vector<scoped_refptr<BasicShape>>& animated_shapes,
      const Vector<double>& offsets,
      const absl::optional<double>& progress,
      cc::PaintWorkletInput::PropertyKeys property_keys)
      : PaintWorkletInput(container_rect.size(),
                          worklet_id,
                          std::move(property_keys)),
        zoom_(zoom),
        offsets_(offsets),
        progress_(progress) {
    for (const auto& basic_shape : animated_shapes) {
      InterpolationValue interpolation_value =
          CreateInterpolationValue(*basic_shape.get());
      DCHECK(interpolation_value);
      basic_shape_types_.push_back(basic_shape->GetType());
      interpolation_values_.push_back(std::move(interpolation_value));
    }
  }

  ~ClipPathPaintWorkletInput() override = default;

  const Vector<InterpolationValue>& InterpolationValues() const {
    return interpolation_values_;
  }
  const Vector<double>& Offsets() const { return offsets_; }
  const absl::optional<double>& MainThreadProgress() const { return progress_; }
  const Vector<BasicShape::ShapeType>& BasicShapeTypes() const {
    return basic_shape_types_;
  }

  float Zoom() const { return zoom_; }

  PaintWorkletInputType GetType() const override {
    return PaintWorkletInputType::kClipPath;
  }

 private:
  InterpolationValue CreateInterpolationValue(const BasicShape& basic_shape) {
    if (basic_shape.GetType() == BasicShape::kStylePathType) {
      return PathInterpolationFunctions::ConvertValue(
          To<StylePath>(&basic_shape),
          PathInterpolationFunctions::kForceAbsolute);
    }
    return basic_shape_interpolation_functions::MaybeConvertBasicShape(
        &basic_shape, zoom_);
  }
  float zoom_;
  Vector<BasicShape::ShapeType> basic_shape_types_;
  Vector<InterpolationValue> interpolation_values_;
  Vector<double> offsets_;
  absl::optional<double> progress_;
};

bool ShapesAreCompatible(const NonInterpolableValue& a,
                         BasicShape::ShapeType a_type,
                         const NonInterpolableValue& b,
                         BasicShape::ShapeType b_type) {
  if (a_type == BasicShape::kStylePathType &&
      b_type == BasicShape::kStylePathType)
    return PathInterpolationFunctions::PathsAreCompatible(a, b);

  if (a_type != BasicShape::kStylePathType &&
      b_type != BasicShape::kStylePathType)
    return basic_shape_interpolation_functions::ShapesAreCompatible(a, b);

  return false;
}

scoped_refptr<BasicShape> CreateBasicShape(
    BasicShape::ShapeType type,
    const InterpolableValue& interpolable_value,
    const NonInterpolableValue& untyped_non_interpolable_value) {
  if (type == BasicShape::kStylePathType) {
    return PathInterpolationFunctions::AppliedValue(
        interpolable_value, &untyped_non_interpolable_value);
  }

  CSSToLengthConversionData conversion_data;
  return basic_shape_interpolation_functions::CreateBasicShape(
      interpolable_value, untyped_non_interpolable_value, conversion_data);
}

void GetAnimatedShapesFromKeyframes(
    const PropertySpecificKeyframe* frame,
    const KeyframeEffectModelBase* model,
    Vector<scoped_refptr<BasicShape>>* animated_shapes,
    const Element* element) {
  scoped_refptr<BasicShape> basic_shape;
  if (model->IsStringKeyframeEffectModel()) {
    DCHECK(frame->IsCSSPropertySpecificKeyframe());
    const CSSValue* value =
        static_cast<const CSSPropertySpecificKeyframe*>(frame)->Value();
    const CSSPropertyName property_name =
        CSSPropertyName(CSSPropertyID::kClipPath);
    const CSSValue* computed_value = StyleResolver::ComputeValue(
        const_cast<Element*>(element), property_name, *value);
    StyleResolverState state(element->GetDocument(),
                             *const_cast<Element*>(element));
    basic_shape = BasicShapeForValue(state, *computed_value);
  } else {
    DCHECK(frame->IsTransitionPropertySpecificKeyframe());
    const TransitionKeyframe::PropertySpecificKeyframe* keyframe =
        To<TransitionKeyframe::PropertySpecificKeyframe>(frame);
    const NonInterpolableValue* non_interpolable_value =
        keyframe->GetValue()->Value().non_interpolable_value.get();
    BasicShape::ShapeType type =
        PathInterpolationFunctions::IsPathNonInterpolableValue(
            *non_interpolable_value)
            ? BasicShape::kStylePathType
            // This can be any shape but kStylePathType. This is needed to
            // distinguish between Path shape and other shapes in
            // CreateBasicShape function.
            : BasicShape::kBasicShapeCircleType;
    basic_shape = CreateBasicShape(
        type, *keyframe->GetValue()->Value().interpolable_value.get(),
        *non_interpolable_value);
  }
  DCHECK(basic_shape);
  animated_shapes->push_back(basic_shape);
}

void GetCompositorKeyframeOffset(const PropertySpecificKeyframe* frame,
                                 Vector<double>& offsets) {
  const CompositorKeyframeDouble& value =
      To<CompositorKeyframeDouble>(*(frame->GetCompositorKeyframeValue()));
  offsets.push_back(value.ToDouble());
}

}  // namespace

template <>
struct DowncastTraits<ClipPathPaintWorkletInput> {
  static bool AllowFrom(const cc::PaintWorkletInput& worklet_input) {
    auto* input = DynamicTo<PaintWorkletInput>(worklet_input);
    return input && AllowFrom(*input);
  }

  static bool AllowFrom(const PaintWorkletInput& worklet_input) {
    return worklet_input.GetType() ==
           PaintWorkletInput::PaintWorkletInputType::kClipPath;
  }
};

// TODO(crbug.com/1248605): Introduce helper functions commonly used by
// background-color and clip-path animations.
Animation* ClipPathPaintDefinition::GetAnimationIfCompositable(
    const Element* element) {
  return GetAnimationForProperty(element, GetCSSPropertyClipPath());
}

// static
ClipPathPaintDefinition* ClipPathPaintDefinition::Create(
    LocalFrame& local_root) {
  return MakeGarbageCollected<ClipPathPaintDefinition>(local_root);
}

ClipPathPaintDefinition::ClipPathPaintDefinition(LocalFrame& local_root)
    : NativeCssPaintDefinition(
          &local_root,
          PaintWorkletInput::PaintWorkletInputType::kClipPath) {}

sk_sp<PaintRecord> ClipPathPaintDefinition::Paint(
    const CompositorPaintWorkletInput* compositor_input,
    const CompositorPaintWorkletJob::AnimatedPropertyValues&
        animated_property_values) {
  const ClipPathPaintWorkletInput* input =
      To<ClipPathPaintWorkletInput>(compositor_input);
  gfx::SizeF container_size = input->ContainerSize();

  const Vector<InterpolationValue>& interpolation_values =
      input->InterpolationValues();
  Vector<BasicShape::ShapeType> basic_shape_types = input->BasicShapeTypes();
  Vector<double> offsets = input->Offsets();
  DCHECK_GT(interpolation_values.size(), 1u);
  DCHECK_EQ(interpolation_values.size(), offsets.size());

  // TODO(crbug.com/1188760): We should handle the case when it is null, and
  // paint the original clip-path retrieved from its style.
  float progress = input->MainThreadProgress().has_value()
                       ? input->MainThreadProgress().value()
                       : 0;
  // This would mean that the animation started on compositor, so we override
  // the progress that we obtained from the main thread.
  if (!animated_property_values.empty()) {
    DCHECK_EQ(animated_property_values.size(), 1u);
    const auto& entry = animated_property_values.begin();
    progress = entry->second.float_value.value();
  }

  // Get the start and end clip-path based on the progress and offsets.
  unsigned result_index = offsets.size() - 1;
  if (progress <= 0) {
    result_index = 0;
  } else if (progress > 0 && progress < 1) {
    for (unsigned i = 0; i < offsets.size() - 1; i++) {
      if (progress <= offsets[i + 1]) {
        result_index = i;
        break;
      }
    }
  }
  if (result_index == offsets.size() - 1)
    result_index = offsets.size() - 2;
  // Because the progress is a global one, we need to adjust it with offsets.
  float adjusted_progress = (progress - offsets[result_index]) /
                            (offsets[result_index + 1] - offsets[result_index]);

  const InterpolationValue& from = interpolation_values[result_index];
  const InterpolationValue& to = interpolation_values[result_index + 1];
  scoped_refptr<BasicShape> result_shape;

  if (ShapesAreCompatible(*from.non_interpolable_value.get(),
                          basic_shape_types[result_index],
                          *to.non_interpolable_value.get(),
                          basic_shape_types[result_index + 1])) {
    std::unique_ptr<InterpolableValue> result_interpolable_value =
        from.interpolable_value->Clone();
    from.interpolable_value->Interpolate(
        *to.interpolable_value, adjusted_progress, *result_interpolable_value);
    result_shape = CreateBasicShape(basic_shape_types[result_index],
                                    *result_interpolable_value,
                                    *from.non_interpolable_value);
  } else if (adjusted_progress < 0.5) {
    result_shape = CreateBasicShape(basic_shape_types[result_index],
                                    *from.interpolable_value,
                                    *from.non_interpolable_value);
  } else {
    result_shape =
        CreateBasicShape(basic_shape_types[result_index + 1],
                         *to.interpolable_value, *to.non_interpolable_value);
  }

  scoped_refptr<ShapeClipPathOperation> current_shape =
      ShapeClipPathOperation::Create(result_shape);

  Path path = current_shape->GetPath(gfx::RectF(container_size), input->Zoom());
  PaintRenderingContext2DSettings* context_settings =
      PaintRenderingContext2DSettings::Create();
  auto* rendering_context = MakeGarbageCollected<PaintRenderingContext2D>(
      gfx::ToRoundedSize(container_size), context_settings, 1, 1);

  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  rendering_context->GetPaintCanvas()->drawPath(path.GetSkPath(), flags);

  return rendering_context->GetRecord();
}

scoped_refptr<Image> ClipPathPaintDefinition::Paint(
    float zoom,
    const gfx::RectF& reference_box,
    const Node& node) {
  DCHECK(node.IsElementNode());
  const Element* element = static_cast<Element*>(const_cast<Node*>(&node));

  Vector<scoped_refptr<BasicShape>> animated_shapes;
  Vector<double> offsets;
  absl::optional<double> progress;

  Animation* animation = GetAnimationIfCompositable(element);
  // If we are here the animation must be compositable.
  DCHECK(animation);

  const AnimationEffect* effect = animation->effect();
  DCHECK(effect->IsKeyframeEffect());

  const KeyframeEffectModelBase* model =
      static_cast<const KeyframeEffect*>(effect)->Model();

  const PropertySpecificKeyframeVector* frames =
      model->GetPropertySpecificKeyframes(
          PropertyHandle(GetCSSPropertyClipPath()));

  for (const auto& frame : *frames) {
    GetAnimatedShapesFromKeyframes(frame, model, &animated_shapes, element);
    GetCompositorKeyframeOffset(frame, offsets);
  }
  progress = effect->Progress();

  node.GetLayoutObject()->GetMutableForPainting().EnsureId();
  CompositorElementId element_id = CompositorElementIdFromUniqueObjectId(
      node.GetLayoutObject()->UniqueId(),
      CompositorAnimations::CompositorElementNamespaceForProperty(
          CSSPropertyID::kClipPath));
  CompositorPaintWorkletInput::PropertyKeys input_property_keys;
  input_property_keys.emplace_back(
      CompositorPaintWorkletInput::NativePropertyType::kClipPath, element_id);
  scoped_refptr<ClipPathPaintWorkletInput> input =
      base::MakeRefCounted<ClipPathPaintWorkletInput>(
          reference_box, worklet_id_, zoom, animated_shapes, offsets, progress,
          std::move(input_property_keys));

  return PaintWorkletDeferredImage::Create(std::move(input),
                                           reference_box.size());
}

void ClipPathPaintDefinition::Trace(Visitor* visitor) const {
  NativePaintDefinition::Trace(visitor);
}

}  // namespace blink
