/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "core/animation/EffectInput.h"

#include "bindings/core/v8/ArrayValue.h"
#include "bindings/core/v8/Dictionary.h"
#include "bindings/core/v8/IDLTypes.h"
#include "bindings/core/v8/NativeValueTraitsImpl.h"
#include "bindings/core/v8/V8BaseKeyframe.h"
#include "bindings/core/v8/V8BasePropertyIndexedKeyframe.h"
#include "bindings/core/v8/string_or_string_sequence.h"
#include "core/animation/AnimationInputHelpers.h"
#include "core/animation/BaseKeyframe.h"
#include "core/animation/BasePropertyIndexedKeyframe.h"
#include "core/animation/CompositorAnimations.h"
#include "core/animation/KeyframeEffectModel.h"
#include "core/animation/StringKeyframe.h"
#include "core/animation/css/CSSAnimations.h"
#include "core/css/CSSStyleSheet.h"
#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "core/dom/ExceptionCode.h"
#include "core/frame/FrameConsole.h"
#include "core/frame/LocalFrame.h"
#include "core/inspector/ConsoleMessage.h"
#include "platform/wtf/ASCIICType.h"
#include "platform/wtf/HashSet.h"

namespace blink {

namespace {

// Converts the composite property of a BasePropertyIndexedKeyframe into a
// vector of EffectModel::CompositeOperation enums.
Vector<WTF::Optional<EffectModel::CompositeOperation>> ParseCompositeProperty(
    const BasePropertyIndexedKeyframe& keyframe) {
  const CompositeOperationOrCompositeOperationOrNullSequence& composite =
      keyframe.composite();

  // This handles the case where we have 'composite: null'. The null value is
  // lifted to the union level in the bindings code.
  if (composite.IsNull())
    return {WTF::nullopt};

  if (composite.IsCompositeOperation()) {
    return {EffectModel::StringToCompositeOperation(
        composite.GetAsCompositeOperation())};
  }

  Vector<WTF::Optional<EffectModel::CompositeOperation>> result;
  for (const String& composite_operation_string :
       composite.GetAsCompositeOperationOrNullSequence()) {
    if (composite_operation_string.IsNull()) {
      result.push_back(WTF::nullopt);
    } else {
      result.push_back(
          EffectModel::StringToCompositeOperation(composite_operation_string));
    }
  }
  return result;
}

void SetKeyframeValue(Element* element,
                      Document& document,
                      StringKeyframe& keyframe,
                      const String& property,
                      const String& value,
                      ExecutionContext* execution_context) {
  StyleSheetContents* style_sheet_contents = document.ElementSheet().Contents();
  CSSPropertyID css_property =
      AnimationInputHelpers::KeyframeAttributeToCSSProperty(property, document);
  if (css_property != CSSPropertyInvalid) {
    MutableCSSPropertyValueSet::SetResult set_result =
        css_property == CSSPropertyVariable
            ? keyframe.SetCSSPropertyValue(
                  AtomicString(property), document.GetPropertyRegistry(), value,
                  document.GetSecureContextMode(), style_sheet_contents)
            : keyframe.SetCSSPropertyValue(css_property, value,
                                           document.GetSecureContextMode(),
                                           style_sheet_contents);
    if (!set_result.did_parse && execution_context) {
      if (document.GetFrame()) {
        document.GetFrame()->Console().AddMessage(ConsoleMessage::Create(
            kJSMessageSource, kWarningMessageLevel,
            "Invalid keyframe value for property " + property + ": " + value));
      }
    }
    return;
  }
  css_property =
      AnimationInputHelpers::KeyframeAttributeToPresentationAttribute(property,
                                                                      element);
  if (css_property != CSSPropertyInvalid) {
    keyframe.SetPresentationAttributeValue(
        CSSProperty::Get(css_property), value, document.GetSecureContextMode(),
        style_sheet_contents);
    return;
  }
  const QualifiedName* svg_attribute =
      AnimationInputHelpers::KeyframeAttributeToSVGAttribute(property, element);
  if (svg_attribute)
    keyframe.SetSVGAttributeValue(*svg_attribute, value);
}

bool ValidatePartialKeyframes(const StringKeyframeVector& keyframes) {
  // CSSAdditiveAnimationsEnabled guards both additive animations and allowing
  // partial (implicit) keyframes.
  if (RuntimeEnabledFeatures::CSSAdditiveAnimationsEnabled())
    return true;

  // An implicit keyframe is inserted in the below cases. Note that the 'first'
  // keyframe is actually all keyframes with offset 0.0, and the 'last' keyframe
  // is actually all keyframes with offset 1.0.
  //
  //   1. A given property is present somewhere in the full set of keyframes,
  //      but is either not present in the first keyframe (requiring an implicit
  //      start value for that property) or last keyframe (requiring an implicit
  //      end value for that property).
  //
  //   2. There is no first keyframe (requiring an implicit start keyframe), or
  //      no last keyframe (requiring an implicit end keyframe).
  //
  // We only care about CSS properties here; animating SVG elements is protected
  // by a different runtime flag.

  Vector<double> computed_offsets =
      KeyframeEffectModelBase::GetComputedOffsets(keyframes);

  PropertyHandleSet properties_with_offset_0;
  PropertyHandleSet properties_with_offset_1;
  for (size_t i = 0; i < keyframes.size(); i++) {
    for (const PropertyHandle& property : keyframes[i]->Properties()) {
      if (!property.IsCSSProperty())
        continue;

      if (computed_offsets[i] == 0.0) {
        properties_with_offset_0.insert(property);
      } else {
        if (!properties_with_offset_0.Contains(property))
          return false;
        if (computed_offsets[i] == 1.0) {
          properties_with_offset_1.insert(property);
        }
      }
    }
  }

  // At this point we have compared all keyframes with offset > 0 against the
  // properties contained in the first keyframe, and found that they match. Now
  // we just need to make sure that there aren't any properties in the first
  // keyframe that aren't in the last keyframe.
  return properties_with_offset_0.size() == properties_with_offset_1.size();
}

// Ensures that a CompositeOperation is of an allowed value for a given
// StringKeyframe and the current runtime flags.
EffectModel::CompositeOperation ResolveCompositeOperationForKeyframe(
    EffectModel::CompositeOperation composite,
    const scoped_refptr<StringKeyframe>& keyframe) {
  if (!RuntimeEnabledFeatures::CSSAdditiveAnimationsEnabled() &&
      keyframe->HasCssProperty() && composite == EffectModel::kCompositeAdd) {
    return EffectModel::kCompositeReplace;
  }
  return composite;
}

bool IsAnimatableKeyframeAttribute(const String& property,
                                   Element* element,
                                   const Document& document) {
  CSSPropertyID css_property =
      AnimationInputHelpers::KeyframeAttributeToCSSProperty(property, document);
  if (css_property != CSSPropertyInvalid) {
    return !CSSAnimations::IsAnimationAffectingProperty(
        CSSProperty::Get(css_property));
  }

  css_property =
      AnimationInputHelpers::KeyframeAttributeToPresentationAttribute(property,
                                                                      element);
  if (css_property != CSSPropertyInvalid)
    return true;

  return !!AnimationInputHelpers::KeyframeAttributeToSVGAttribute(property,
                                                                  element);
}

// Temporary storage struct used when converting array-form keyframes.
struct KeyframeOutput {
  BaseKeyframe base_keyframe;
  Vector<std::pair<String, String>> property_value_pairs;
};

StringKeyframeVector ConvertArrayForm(Element* element,
                                      Document& document,
                                      DictionaryIterator iterator,
                                      ScriptState* script_state,
                                      ExceptionState& exception_state) {
  // This loop captures step 5 of the procedure to process a keyframes argument,
  // in the case where the argument is iterable.
  Vector<KeyframeOutput> processed_keyframes;
  ExecutionContext* execution_context = ExecutionContext::From(script_state);
  v8::Isolate* isolate = script_state->GetIsolate();
  while (iterator.Next(execution_context, exception_state)) {
    KeyframeOutput keyframe_output;

    Dictionary keyframe_dictionary;
    if (!iterator.ValueAsDictionary(keyframe_dictionary, exception_state)) {
      exception_state.ThrowTypeError("Keyframes must be objects.");
      return {};
    }

    // Extract the offset, easing, and composite as per step 1 of the 'procedure
    // to process a keyframe-like object'.
    V8BaseKeyframe::ToImpl(keyframe_dictionary.GetIsolate(),
                           keyframe_dictionary.V8Value(),
                           keyframe_output.base_keyframe, exception_state);
    if (exception_state.HadException())
      return {};

    const Vector<String>& keyframe_properties =
        keyframe_dictionary.GetPropertyNames(exception_state);
    if (exception_state.HadException())
      return {};

    for (const auto& property : keyframe_properties) {
      if (property == "offset" || property == "composite" ||
          property == "easing") {
        continue;
      }

      // By spec, we are not allowed to access any non-animatable property.
      if (!IsAnimatableKeyframeAttribute(property, element, document))
        continue;

      // By spec, we are only allowed to access a given (property, value) pair
      // once. This is observable by the web client, so we take care to adhere
      // to that.
      v8::Local<v8::Value> v8_value;
      if (!keyframe_dictionary.Get(property, v8_value)) {
        // TODO(crbug.com/666661): Propagate exceptions from Dictionary::Get.
        return {};
      }

      if (v8_value->IsArray()) {
        exception_state.ThrowTypeError(
            "Lists of values not permitted in array-form list of keyframes");
        return {};
      }

      String string_value = NativeValueTraits<IDLString>::NativeValue(
          isolate, v8_value, exception_state);
      if (exception_state.HadException())
        return {};
      keyframe_output.property_value_pairs.push_back(
          std::make_pair(property, string_value));
    }
    processed_keyframes.push_back(keyframe_output);
  }
  if (exception_state.HadException())
    return {};

  // 6. If processed keyframes is not loosely sorted by offset, throw a
  // TypeError and abort these steps.
  double previous_offset = -std::numeric_limits<double>::infinity();
  for (const auto& processed_keyframe : processed_keyframes) {
    if (processed_keyframe.base_keyframe.hasOffset()) {
      double offset = processed_keyframe.base_keyframe.offset();
      if (offset < previous_offset) {
        exception_state.ThrowTypeError(
            "Offsets must be montonically non-decreasing.");
        return {};
      }
      previous_offset = offset;
    }
  }

  // 7. If there exist any keyframe in processed keyframes whose keyframe
  // offset is non-null and less than zero or greater than one, throw a
  // TypeError and abort these steps.
  for (const auto& processed_keyframe : processed_keyframes) {
    if (processed_keyframe.base_keyframe.hasOffset()) {
      double offset = processed_keyframe.base_keyframe.offset();
      if (offset < 0 || offset > 1) {
        exception_state.ThrowTypeError(
            "Offsets must be null or in the range [0,1].");
        return {};
      }
    }
  }

  StringKeyframeVector keyframes;
  for (const KeyframeOutput& processed_keyframe : processed_keyframes) {
    // Now we create the actual Keyframe object. We start by assigning the
    // offset and composite values; conceptually these were actually added in
    // step 5 above but we didn't have a keyframe object then.
    scoped_refptr<StringKeyframe> keyframe = StringKeyframe::Create();
    if (processed_keyframe.base_keyframe.hasOffset()) {
      keyframe->SetOffset(processed_keyframe.base_keyframe.offset());
    }

    // 8.1. For each property-value pair in frame, parse the property value
    // using the syntax specified for that property.
    for (const auto& pair : processed_keyframe.property_value_pairs) {
      // TODO(crbug.com/777971): Make parsing of property values spec-compliant.
      SetKeyframeValue(element, document, *keyframe.get(), pair.first,
                       pair.second, execution_context);
    }

    if (processed_keyframe.base_keyframe.hasComposite()) {
      keyframe->SetComposite(ResolveCompositeOperationForKeyframe(
          EffectModel::StringToCompositeOperation(
              processed_keyframe.base_keyframe.composite()),
          keyframe));
    }

    // 8.2. Let the timing function of frame be the result of parsing the
    // “easing” property on frame using the CSS syntax defined for the easing
    // property of the AnimationEffectTimingReadOnly interface.
    //
    // If parsing the “easing” property fails, throw a TypeError and abort this
    // procedure.
    scoped_refptr<TimingFunction> timing_function =
        AnimationInputHelpers::ParseTimingFunction(
            processed_keyframe.base_keyframe.easing(), &document,
            exception_state);
    if (!timing_function)
      return {};
    keyframe->SetEasing(timing_function);

    keyframes.push_back(keyframe);
  }

  DCHECK(!exception_state.HadException());
  return keyframes;
}

// Extracts the values for a given property in the input keyframes. As per the
// spec property values for the object-notation form have type (DOMString or
// sequence<DOMString>).
static bool GetPropertyIndexedKeyframeValues(
    const Dictionary& keyframe_dictionary,
    const String& property,
    ScriptState* script_state,
    ExceptionState& exception_state,
    Vector<String>& result) {
  DCHECK(result.IsEmpty());

  // By spec, we are only allowed to access a given (property, value) pair once.
  // This is observable by the web client, so we take care to adhere to that.
  v8::Local<v8::Value> v8_value;
  if (!keyframe_dictionary.Get(property, v8_value)) {
    // TODO(crbug.com/666661): Get() should rethrow internal exceptions.
    return false;
  }

  StringOrStringSequence string_or_string_sequence;
  V8StringOrStringSequence::ToImpl(
      script_state->GetIsolate(), v8_value, string_or_string_sequence,
      UnionTypeConversionMode::kNotNullable, exception_state);
  if (exception_state.HadException())
    return false;

  if (string_or_string_sequence.IsString())
    result.push_back(string_or_string_sequence.GetAsString());
  else
    result = string_or_string_sequence.GetAsStringSequence();

  return true;
}

// Implements the procedure to "process a keyframes argument" from the
// web-animations spec for an object form keyframes argument.
//
// See https://drafts.csswg.org/web-animations/#processing-a-keyframes-argument
StringKeyframeVector ConvertObjectForm(Element* element,
                                       Document& document,
                                       const Dictionary& dictionary,
                                       ScriptState* script_state,
                                       ExceptionState& exception_state) {
  // We implement much of this procedure out of order from the way the spec is
  // written, to avoid repeatedly going over the list of keyframes.
  // The web-observable behavior should be the same as the spec.

  // Extract the offset, easing, and composite as per step 1 of the 'procedure
  // to process a keyframe-like object'.
  BasePropertyIndexedKeyframe property_indexed_keyframe;
  V8BasePropertyIndexedKeyframe::ToImpl(
      dictionary.GetIsolate(), dictionary.V8Value(), property_indexed_keyframe,
      exception_state);
  if (exception_state.HadException())
    return {};

  Vector<WTF::Optional<double>> offsets;
  if (property_indexed_keyframe.offset().IsNull())
    offsets.push_back(WTF::nullopt);
  else if (property_indexed_keyframe.offset().IsDouble())
    offsets.push_back(property_indexed_keyframe.offset().GetAsDouble());
  else
    offsets = property_indexed_keyframe.offset().GetAsDoubleOrNullSequence();

  // The web-animations spec explicitly states that easings should be kept as
  // DOMStrings here and not parsed into timing functions until later.
  Vector<String> easings;
  if (property_indexed_keyframe.easing().IsString())
    easings.push_back(property_indexed_keyframe.easing().GetAsString());
  else
    easings = property_indexed_keyframe.easing().GetAsStringSequence();

  Vector<WTF::Optional<EffectModel::CompositeOperation>> composite_operations =
      ParseCompositeProperty(property_indexed_keyframe);

  // Next extract all animatable properties from the input argument and iterate
  // through them, processing each as a list of values for that property. This
  // implements both steps 2-7 of the 'procedure to process a keyframe-like
  // object' and step 5.2 of the 'procedure to process a keyframes argument'.

  const Vector<String>& keyframe_properties =
      dictionary.GetPropertyNames(exception_state);
  if (exception_state.HadException())
    return {};

  // Steps 5.2 - 5.4 state that the user agent is to:
  //
  //   * Create sets of 'property keyframes' with no offset.
  //   * Calculate computed offsets for each set of keyframes individually.
  //   * Join the sets together and merge those with identical computed offsets.
  //
  // This is equivalent to just keeping a hashmap from computed offset to a
  // single keyframe, which simplifies the parsing logic.
  HashMap<double, scoped_refptr<StringKeyframe>> keyframes;

  for (const auto& property : keyframe_properties) {
    if (property == "offset" || property == "composite" ||
        property == "easing") {
      continue;
    }

    // By spec, we are not allowed to access any non-animatable property.
    if (!IsAnimatableKeyframeAttribute(property, element, document))
      continue;

    Vector<String> values;
    if (!GetPropertyIndexedKeyframeValues(dictionary, property, script_state,
                                          exception_state, values)) {
      return {};
    }

    // Now create a keyframe (or retrieve and augment an existing one) for each
    // value this property maps to. As explained above, this loop performs both
    // the initial creation and merging mentioned in the spec.
    size_t num_keyframes = values.size();
    ExecutionContext* execution_context = ExecutionContext::From(script_state);
    for (size_t i = 0; i < num_keyframes; ++i) {
      // As all offsets are null for these 'property keyframes', the computed
      // offset is just the fractional position of each keyframe in the array.
      //
      // The only special case is that when there is only one keyframe the sole
      // computed offset is defined as 1.
      double computed_offset =
          (num_keyframes == 1) ? 1 : i / double(num_keyframes - 1);

      auto result = keyframes.insert(computed_offset, nullptr);
      if (result.is_new_entry)
        result.stored_value->value = StringKeyframe::Create();

      SetKeyframeValue(element, document, *result.stored_value->value.get(),
                       property, values[i], execution_context);
    }
  }

  // 5.3 Sort processed keyframes by the computed keyframe offset of each
  // keyframe in increasing order.
  Vector<double> keys;
  for (const auto& key : keyframes.Keys())
    keys.push_back(key);
  std::sort(keys.begin(), keys.end());

  // Steps 5.5 - 5.12 deal with assigning the user-specified offset, easing, and
  // composite properties to the keyframes.
  //
  // This loop also implements steps 6, 7, and 8 of the spec. Because nothing is
  // user-observable at this point, we can operate out of order. Note that this
  // may result in us throwing a different order of TypeErrors than other user
  // agents[1], but as all exceptions are TypeErrors this is not observable by
  // the web client.
  //
  // [1] E.g. if the offsets are [2, 0] we will throw due to the first offset
  //     being > 1 before we throw due to the offsets not being loosely ordered.
  StringKeyframeVector results;
  double previous_offset = 0.0;
  for (size_t i = 0; i < keys.size(); i++) {
    auto keyframe = keyframes.at(keys[i]);

    if (i < offsets.size()) {
      WTF::Optional<double> offset = offsets[i];
      // 6. If processed keyframes is not loosely sorted by offset, throw a
      // TypeError and abort these steps.
      if (offset.has_value()) {
        if (offset.value() < previous_offset) {
          exception_state.ThrowTypeError(
              "Offsets must be montonically non-decreasing.");
          return {};
        }
        previous_offset = offset.value();
      }

      // 7. If there exist any keyframe in processed keyframes whose keyframe
      // offset is non-null and less than zero or greater than one, throw a
      // TypeError and abort these steps.
      if (offset.has_value() && (offset.value() < 0 || offset.value() > 1)) {
        exception_state.ThrowTypeError(
            "Offsets must be null or in the range [0,1].");
        return {};
      }

      keyframe->SetOffset(offset);
    }

    // At this point in the code we have read all the properties we will read
    // from the input object, so it is safe to parse the easing strings. See the
    // note on step 8.2.
    if (!easings.IsEmpty()) {
      // 5.9 If easings has fewer items than property keyframes, repeat the
      // elements in easings successively starting from the beginning of the
      // list until easings has as many items as property keyframes.
      const String& easing = easings[i % easings.size()];

      // 8.2 Let the timing function of frame be the result of parsing the
      // "easing" property on frame using the CSS syntax defined for the easing
      // property of the AnimationEffectTimingReadOnly interface.
      //
      // If parsing the “easing” property fails, throw a TypeError and abort
      // this procedure.
      scoped_refptr<TimingFunction> timing_function =
          AnimationInputHelpers::ParseTimingFunction(easing, &document,
                                                     exception_state);
      if (!timing_function)
        return {};

      keyframe->SetEasing(timing_function);
    }

    if (!composite_operations.IsEmpty()) {
      // 5.12.2 As with easings, if composite modes has fewer items than
      // property keyframes, repeat the elements in composite modes successively
      // starting from the beginning of the list until composite modes has as
      // many items as property keyframes.
      WTF::Optional<EffectModel::CompositeOperation> composite =
          composite_operations[i % composite_operations.size()];
      if (composite) {
        keyframe->SetComposite(
            ResolveCompositeOperationForKeyframe(composite.value(), keyframe));
      }
    }

    results.push_back(keyframe);
  }

  // Step 8 of the spec is done above (or will be): parsing property values
  // according to syntax for the property (discarding with console warning on
  // fail) and parsing each easing property.
  // TODO(crbug.com/777971): Fix parsing of property values to adhere to spec.

  // 9. Parse each of the values in unused easings using the CSS syntax defined
  // for easing property of the AnimationEffectTimingReadOnly interface, and if
  // any of the values fail to parse, throw a TypeError and abort this
  // procedure.
  for (size_t i = results.size(); i < easings.size(); i++) {
    scoped_refptr<TimingFunction> timing_function =
        AnimationInputHelpers::ParseTimingFunction(easings[i], &document,
                                                   exception_state);
    if (!timing_function)
      return {};
  }

  DCHECK(!exception_state.HadException());
  return results;
}

bool HasAdditiveCompositeCSSKeyframe(
    const KeyframeEffectModelBase::KeyframeGroupMap& keyframe_groups) {
  for (const auto& keyframe_group : keyframe_groups) {
    PropertyHandle property = keyframe_group.key;
    if (!property.IsCSSProperty())
      continue;
    for (const auto& keyframe : keyframe_group.value->Keyframes()) {
      if (keyframe->Composite() == EffectModel::kCompositeAdd)
        return true;
    }
  }
  return false;
}
}  // namespace

KeyframeEffectModelBase* EffectInput::Convert(
    Element* element,
    const ScriptValue& keyframes,
    EffectModel::CompositeOperation composite,
    ScriptState* script_state,
    ExceptionState& exception_state) {
  StringKeyframeVector parsed_keyframes =
      ParseKeyframesArgument(element, keyframes, script_state, exception_state);
  if (exception_state.HadException())
    return nullptr;

  composite = ResolveCompositeOperation(composite, parsed_keyframes);

  StringKeyframeEffectModel* keyframe_effect_model =
      StringKeyframeEffectModel::Create(parsed_keyframes, composite,
                                        LinearTimingFunction::Shared());

  if (!RuntimeEnabledFeatures::CSSAdditiveAnimationsEnabled()) {
    // This should be enforced by the parsing code.
    DCHECK(!HasAdditiveCompositeCSSKeyframe(
        keyframe_effect_model->GetPropertySpecificKeyframeGroups()));
  }

  DCHECK(!exception_state.HadException());
  return keyframe_effect_model;
}

StringKeyframeVector EffectInput::ParseKeyframesArgument(
    Element* element,
    const ScriptValue& keyframes,
    ScriptState* script_state,
    ExceptionState& exception_state) {
  // Per the spec, a null keyframes object maps to a valid but empty sequence.
  if (keyframes.IsNull())
    return {};

  v8::Isolate* isolate = script_state->GetIsolate();
  Dictionary dictionary(isolate, keyframes.V8Value(), exception_state);
  if (exception_state.HadException())
    return {};

  // TODO(crbug.com/816934): Get spec to specify what parsing context to use.
  Document& document = element
                           ? element->GetDocument()
                           : *ToDocument(ExecutionContext::From(script_state));

  StringKeyframeVector parsed_keyframes;
  DictionaryIterator iterator =
      dictionary.GetIterator(ExecutionContext::From(script_state));
  if (iterator.IsNull()) {
    parsed_keyframes = ConvertObjectForm(element, document, dictionary,
                                         script_state, exception_state);
  } else {
    parsed_keyframes = ConvertArrayForm(element, document, iterator,
                                        script_state, exception_state);
  }

  if (!ValidatePartialKeyframes(parsed_keyframes)) {
    exception_state.ThrowDOMException(kNotSupportedError,
                                      "Partial keyframes are not supported.");
    return {};
  }
  return parsed_keyframes;
}

EffectModel::CompositeOperation EffectInput::ResolveCompositeOperation(
    EffectModel::CompositeOperation composite,
    const StringKeyframeVector& keyframes) {
  EffectModel::CompositeOperation result = composite;
  for (const scoped_refptr<StringKeyframe>& keyframe : keyframes) {
    // Replace is always supported, so we can early-exit if and when we have
    // that as our composite value.
    if (result == EffectModel::kCompositeReplace)
      break;
    result = ResolveCompositeOperationForKeyframe(result, keyframe);
  }
  return result;
}
}  // namespace blink
