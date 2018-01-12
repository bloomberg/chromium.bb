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
#include "bindings/core/v8/DictionaryHelperForBindings.h"
#include "bindings/core/v8/V8BasePropertyIndexedKeyframe.h"
#include "bindings/core/v8/string_or_string_sequence.h"
#include "core/animation/AnimationInputHelpers.h"
#include "core/animation/BasePropertyIndexedKeyframe.h"
#include "core/animation/CompositorAnimations.h"
#include "core/animation/KeyframeEffectModel.h"
#include "core/animation/StringKeyframe.h"
#include "core/css/CSSStyleSheet.h"
#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "core/dom/ExceptionCode.h"
#include "core/frame/FrameConsole.h"
#include "core/frame/LocalFrame.h"
#include "core/inspector/ConsoleMessage.h"
#include "platform/wtf/ASCIICType.h"
#include "platform/wtf/HashSet.h"
#include "platform/wtf/NonCopyingSort.h"

namespace blink {

namespace {

// Converts the composite property of a BasePropertyIndexedKeyframe into a
// vector of EffectModel::CompositeOperation enums.
//
// If the composite property cannot be extracted or parsed for some reason, an
// exception will be thrown in |exception_state|.
Vector<EffectModel::CompositeOperation> ParseCompositeProperty(
    const BasePropertyIndexedKeyframe& keyframe,
    ExceptionState& exception_state) {
  const CompositeOperationOrCompositeOperationSequence& composite =
      keyframe.composite();
  if (composite.IsCompositeOperation()) {
    EffectModel::CompositeOperation composite_operation;
    if (!EffectModel::StringToCompositeOperation(
            composite.GetAsCompositeOperation(), composite_operation,
            &exception_state)) {
      DCHECK(exception_state.HadException());
      return {};
    }
    return {composite_operation};
  }

  Vector<EffectModel::CompositeOperation> result;
  for (const String& composite_operation_string :
       composite.GetAsCompositeOperationSequence()) {
    EffectModel::CompositeOperation composite_operation;
    if (!EffectModel::StringToCompositeOperation(composite_operation_string,
                                                 composite_operation,
                                                 &exception_state)) {
      DCHECK(exception_state.HadException());
      return {};
    }
    result.push_back(composite_operation);
  }
  return result;
}

// Validates the value of |offset| and throws an exception if out of range.
bool CheckOffset(double offset,
                 double last_offset,
                 ExceptionState& exception_state) {
  // Keyframes with offsets outside the range [0.0, 1.0] are an error.
  if (std::isnan(offset)) {
    exception_state.ThrowTypeError("Non numeric offset provided");
    return false;
  }

  if (offset < 0 || offset > 1) {
    exception_state.ThrowTypeError("Offsets provided outside the range [0, 1]");
    return false;
  }

  if (offset < last_offset) {
    exception_state.ThrowTypeError(
        "Keyframes with specified offsets are not sorted");
    return false;
  }

  return true;
}

void SetKeyframeValue(Element& element,
                      StringKeyframe& keyframe,
                      const String& property,
                      const String& value,
                      ExecutionContext* execution_context) {
  StyleSheetContents* style_sheet_contents =
      element.GetDocument().ElementSheet().Contents();
  CSSPropertyID css_property =
      AnimationInputHelpers::KeyframeAttributeToCSSProperty(
          property, element.GetDocument());
  if (css_property != CSSPropertyInvalid) {
    MutableCSSPropertyValueSet::SetResult set_result =
        css_property == CSSPropertyVariable
            ? keyframe.SetCSSPropertyValue(
                  AtomicString(property),
                  element.GetDocument().GetPropertyRegistry(), value,
                  element.GetDocument().GetSecureContextMode(),
                  style_sheet_contents)
            : keyframe.SetCSSPropertyValue(
                  css_property, value,
                  element.GetDocument().GetSecureContextMode(),
                  style_sheet_contents);
    if (!set_result.did_parse && execution_context) {
      Document& document = ToDocument(*execution_context);
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
        css_property, value, element.GetDocument().GetSecureContextMode(),
        style_sheet_contents);
    return;
  }
  const QualifiedName* svg_attribute =
      AnimationInputHelpers::KeyframeAttributeToSVGAttribute(property, element);
  if (svg_attribute)
    keyframe.SetSVGAttributeValue(*svg_attribute, value);
}

KeyframeEffectModelBase* CreateEmptyEffectModel(
    EffectModel::CompositeOperation composite) {
  return StringKeyframeEffectModel::Create(StringKeyframeVector(), composite);
}

KeyframeEffectModelBase* CreateEffectModel(
    Element& element,
    const StringKeyframeVector& keyframes,
    EffectModel::CompositeOperation composite,
    ExceptionState& exception_state) {
  StringKeyframeEffectModel* keyframe_effect_model =
      StringKeyframeEffectModel::Create(keyframes, composite,
                                        LinearTimingFunction::Shared());
  if (!RuntimeEnabledFeatures::CSSAdditiveAnimationsEnabled()) {
    for (const auto& keyframe_group :
         keyframe_effect_model->GetPropertySpecificKeyframeGroups()) {
      PropertyHandle property = keyframe_group.key;
      if (!property.IsCSSProperty())
        continue;

      for (const auto& keyframe : keyframe_group.value->Keyframes()) {
        if (keyframe->IsNeutral()) {
          exception_state.ThrowDOMException(
              kNotSupportedError, "Partial keyframes are not supported.");
          return CreateEmptyEffectModel(composite);
        }
        if (keyframe->Composite() != EffectModel::kCompositeReplace) {
          exception_state.ThrowDOMException(
              kNotSupportedError, "Additive animations are not supported.");
          return CreateEmptyEffectModel(composite);
        }
      }
    }
  }

  DCHECK(!exception_state.HadException());
  return keyframe_effect_model;
}

bool ExhaustDictionaryIterator(DictionaryIterator& iterator,
                               ExecutionContext* execution_context,
                               ExceptionState& exception_state,
                               Vector<Dictionary>& result) {
  while (iterator.Next(execution_context, exception_state)) {
    Dictionary dictionary;
    if (!iterator.ValueAsDictionary(dictionary, exception_state)) {
      exception_state.ThrowTypeError("Keyframes must be objects.");
      return false;
    }
    result.push_back(dictionary);
  }
  return !exception_state.HadException();
}

}  // namespace

// Implements "Processing a keyframes argument" from the web-animations spec.
// https://drafts.csswg.org/web-animations/#processing-a-keyframes-argument
KeyframeEffectModelBase* EffectInput::Convert(
    Element* element,
    const ScriptValue& keyframes,
    EffectModel::CompositeOperation composite,
    ScriptState* script_state,
    ExceptionState& exception_state) {
  // Per the spec, a null keyframes object maps to a valid but empty sequence.
  // TODO(crbug.com/772014): The element is allowed to be null; remove check.
  if (keyframes.IsNull() || !element)
    return CreateEmptyEffectModel(composite);

  v8::Isolate* isolate = script_state->GetIsolate();
  Dictionary dictionary(isolate, keyframes.V8Value(), exception_state);
  if (exception_state.HadException())
    return CreateEmptyEffectModel(composite);

  ExecutionContext* execution_context = ExecutionContext::From(script_state);
  auto iterator = dictionary.GetIterator(execution_context);
  if (iterator.IsNull()) {
    return ConvertObjectForm(*element, dictionary, composite, script_state,
                             exception_state);
  }

  // TODO(alancutter): Convert keyframes during iteration rather than after
  // to match spec.
  Vector<Dictionary> keyframe_dictionaries;
  if (ExhaustDictionaryIterator(iterator, execution_context, exception_state,
                                keyframe_dictionaries)) {
    return ConvertArrayForm(*element, keyframe_dictionaries, composite,
                            execution_context, exception_state);
  }

  DCHECK(exception_state.HadException());
  return CreateEmptyEffectModel(composite);
}

KeyframeEffectModelBase* EffectInput::ConvertArrayForm(
    Element& element,
    const Vector<Dictionary>& keyframe_dictionaries,
    EffectModel::CompositeOperation composite,
    ExecutionContext* execution_context,
    ExceptionState& exception_state) {
  StringKeyframeVector keyframes;
  double last_offset = 0;

  for (const Dictionary& keyframe_dictionary : keyframe_dictionaries) {
    scoped_refptr<StringKeyframe> keyframe = StringKeyframe::Create();

    Nullable<double> offset;
    if (DictionaryHelper::Get(keyframe_dictionary, "offset", offset) &&
        !offset.IsNull()) {
      if (!CheckOffset(offset.Get(), last_offset, exception_state))
        return CreateEmptyEffectModel(composite);

      last_offset = offset.Get();
      keyframe->SetOffset(offset.Get());
    }

    String composite_string;
    DictionaryHelper::Get(keyframe_dictionary, "composite", composite_string);
    if (composite_string == "add")
      keyframe->SetComposite(EffectModel::kCompositeAdd);
    else if (composite_string == "replace")
      keyframe->SetComposite(EffectModel::kCompositeReplace);
    // TODO(crbug.com/788440): Support "accumulate" keyframe composition.

    String timing_function_string;
    if (DictionaryHelper::Get(keyframe_dictionary, "easing",
                              timing_function_string)) {
      scoped_refptr<TimingFunction> timing_function =
          AnimationInputHelpers::ParseTimingFunction(
              timing_function_string, &element.GetDocument(), exception_state);
      if (!timing_function)
        return CreateEmptyEffectModel(composite);
      keyframe->SetEasing(timing_function);
    }

    const Vector<String>& keyframe_properties =
        keyframe_dictionary.GetPropertyNames(exception_state);
    if (exception_state.HadException())
      return CreateEmptyEffectModel(composite);
    for (const auto& property : keyframe_properties) {
      if (property == "offset" || property == "composite" ||
          property == "easing") {
        continue;
      }

      Vector<String> values;
      if (DictionaryHelper::Get(keyframe_dictionary, property, values)) {
        exception_state.ThrowTypeError(
            "Lists of values not permitted in array-form list of keyframes");
        return CreateEmptyEffectModel(composite);
      }

      String value;
      DictionaryHelper::Get(keyframe_dictionary, property, value);

      SetKeyframeValue(element, *keyframe.get(), property, value,
                       execution_context);
    }
    keyframes.push_back(keyframe);
  }

  DCHECK(!exception_state.HadException());

  return CreateEffectModel(element, keyframes, composite, exception_state);
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
  if (!keyframe_dictionary.Get(property, v8_value))
    return false;

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
KeyframeEffectModelBase* EffectInput::ConvertObjectForm(
    Element& element,
    const Dictionary& dictionary,
    EffectModel::CompositeOperation composite,
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
    return CreateEmptyEffectModel(composite);

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

  Vector<EffectModel::CompositeOperation> composite_operations =
      ParseCompositeProperty(property_indexed_keyframe, exception_state);
  if (exception_state.HadException())
    return CreateEmptyEffectModel(composite);

  // Next extract all animatable properties from the input argument and iterate
  // through them, processing each as a list of values for that property. This
  // implements both steps 2-7 of the 'procedure to process a keyframe-like
  // object' and step 5.2 of the 'procedure to process a keyframes argument'.

  const Vector<String>& keyframe_properties =
      dictionary.GetPropertyNames(exception_state);
  if (exception_state.HadException())
    return CreateEmptyEffectModel(composite);

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

    Vector<String> values;
    if (!GetPropertyIndexedKeyframeValues(dictionary, property, script_state,
                                          exception_state, values))
      return CreateEmptyEffectModel(composite);

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

      SetKeyframeValue(element, *result.stored_value->value.get(), property,
                       values[i], execution_context);
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
          return CreateEmptyEffectModel(composite);
        }
        previous_offset = offset.value();
      }

      // 7. If there exist any keyframe in processed keyframes whose keyframe
      // offset is non-null and less than zero or greater than one, throw a
      // TypeError and abort these steps.
      if (offset.has_value() && (offset.value() < 0 || offset.value() > 1)) {
        exception_state.ThrowTypeError(
            "Offsets must be null or in the range [0,1].");
        return CreateEmptyEffectModel(composite);
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
          AnimationInputHelpers::ParseTimingFunction(
              easing, &element.GetDocument(), exception_state);
      if (!timing_function)
        return CreateEmptyEffectModel(composite);

      keyframe->SetEasing(timing_function);
    }

    if (!composite_operations.IsEmpty()) {
      // 5.12.2 As with easings, if composite modes has fewer items than
      // property keyframes, repeat the elements in composite modes successively
      // starting from the beginning of the list until composite modes has as
      // many items as property keyframes.
      keyframe->SetComposite(
          composite_operations[i % composite_operations.size()]);
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
        AnimationInputHelpers::ParseTimingFunction(
            easings[i], &element.GetDocument(), exception_state);
    if (!timing_function)
      return CreateEmptyEffectModel(composite);
  }

  DCHECK(!exception_state.HadException());

  return CreateEffectModel(element, results, composite, exception_state);
}

}  // namespace blink
