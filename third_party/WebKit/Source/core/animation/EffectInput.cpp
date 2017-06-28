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

#include "bindings/core/v8/Dictionary.h"
#include "bindings/core/v8/DictionaryHelperForBindings.h"
#include "bindings/core/v8/DictionarySequenceOrDictionary.h"
#include "core/animation/AnimationInputHelpers.h"
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

bool CompareKeyframes(const RefPtr<StringKeyframe>& a,
                      const RefPtr<StringKeyframe>& b) {
  return a->Offset() < b->Offset();
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
    MutableStylePropertySet::SetResult set_result =
        css_property == CSSPropertyVariable
            ? keyframe.SetCSSPropertyValue(
                  AtomicString(property),
                  element.GetDocument().GetPropertyRegistry(), value,
                  style_sheet_contents)
            : keyframe.SetCSSPropertyValue(css_property, value,
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
    keyframe.SetPresentationAttributeValue(css_property, value,
                                           style_sheet_contents);
    return;
  }
  const QualifiedName* svg_attribute =
      AnimationInputHelpers::KeyframeAttributeToSVGAttribute(property, element);
  if (svg_attribute)
    keyframe.SetSVGAttributeValue(*svg_attribute, value);
}

EffectModel* CreateEffectModelFromKeyframes(
    Element& element,
    const StringKeyframeVector& keyframes,
    ExceptionState& exception_state) {
  StringKeyframeEffectModel* keyframe_effect_model =
      StringKeyframeEffectModel::Create(keyframes,
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
          return nullptr;
        }
        if (keyframe->Composite() != EffectModel::kCompositeReplace) {
          exception_state.ThrowDOMException(
              kNotSupportedError, "Additive animations are not supported.");
          return nullptr;
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

// Spec: http://w3c.github.io/web-animations/#processing-a-keyframes-argument
EffectModel* EffectInput::Convert(
    Element* element,
    const DictionarySequenceOrDictionary& effect_input,
    ExecutionContext* execution_context,
    ExceptionState& exception_state) {
  if (effect_input.isNull() || !element)
    return nullptr;

  if (effect_input.isDictionarySequence()) {
    return ConvertArrayForm(*element, effect_input.getAsDictionarySequence(),
                            execution_context, exception_state);
  }

  const Dictionary& dictionary = effect_input.getAsDictionary();
  DictionaryIterator iterator = dictionary.GetIterator(execution_context);
  if (!iterator.IsNull()) {
    // TODO(alancutter): Convert keyframes during iteration rather than after to
    // match spec.
    Vector<Dictionary> keyframe_dictionaries;
    if (ExhaustDictionaryIterator(iterator, execution_context, exception_state,
                                  keyframe_dictionaries)) {
      return ConvertArrayForm(*element, keyframe_dictionaries,
                              execution_context, exception_state);
    }
    return nullptr;
  }

  return ConvertObjectForm(*element, dictionary, execution_context,
                           exception_state);
}

EffectModel* EffectInput::ConvertArrayForm(
    Element& element,
    const Vector<Dictionary>& keyframe_dictionaries,
    ExecutionContext* execution_context,
    ExceptionState& exception_state) {
  StringKeyframeVector keyframes;
  double last_offset = 0;

  for (const Dictionary& keyframe_dictionary : keyframe_dictionaries) {
    RefPtr<StringKeyframe> keyframe = StringKeyframe::Create();

    Nullable<double> offset;
    if (DictionaryHelper::Get(keyframe_dictionary, "offset", offset) &&
        !offset.IsNull()) {
      if (!CheckOffset(offset.Get(), last_offset, exception_state))
        return nullptr;

      last_offset = offset.Get();
      keyframe->SetOffset(offset.Get());
    }

    String composite_string;
    DictionaryHelper::Get(keyframe_dictionary, "composite", composite_string);
    if (composite_string == "add")
      keyframe->SetComposite(EffectModel::kCompositeAdd);
    // TODO(alancutter): Support "accumulate" keyframe composition.

    String timing_function_string;
    if (DictionaryHelper::Get(keyframe_dictionary, "easing",
                              timing_function_string)) {
      RefPtr<TimingFunction> timing_function =
          AnimationInputHelpers::ParseTimingFunction(
              timing_function_string, &element.GetDocument(), exception_state);
      if (!timing_function)
        return nullptr;
      keyframe->SetEasing(timing_function);
    }

    const Vector<String>& keyframe_properties =
        keyframe_dictionary.GetPropertyNames(exception_state);
    if (exception_state.HadException())
      return nullptr;
    for (const auto& property : keyframe_properties) {
      if (property == "offset" || property == "composite" ||
          property == "easing") {
        continue;
      }

      Vector<String> values;
      if (DictionaryHelper::Get(keyframe_dictionary, property, values)) {
        exception_state.ThrowTypeError(
            "Lists of values not permitted in array-form list of keyframes");
        return nullptr;
      }

      String value;
      DictionaryHelper::Get(keyframe_dictionary, property, value);

      SetKeyframeValue(element, *keyframe.Get(), property, value,
                       execution_context);
    }
    keyframes.push_back(keyframe);
  }

  DCHECK(!exception_state.HadException());

  return CreateEffectModelFromKeyframes(element, keyframes, exception_state);
}

static bool GetPropertyIndexedKeyframeValues(
    const Dictionary& keyframe_dictionary,
    const String& property,
    ExecutionContext* execution_context,
    ExceptionState& exception_state,
    Vector<String>& result) {
  DCHECK(result.IsEmpty());

  // Array of strings.
  if (DictionaryHelper::Get(keyframe_dictionary, property, result))
    return true;

  Dictionary values_dictionary;
  if (!keyframe_dictionary.Get(property, values_dictionary) ||
      values_dictionary.IsUndefinedOrNull()) {
    // Non-object.
    String value;
    DictionaryHelper::Get(keyframe_dictionary, property, value);
    result.push_back(value);
    return true;
  }

  DictionaryIterator iterator =
      values_dictionary.GetIterator(execution_context);
  if (iterator.IsNull()) {
    // Non-iterable object.
    String value;
    DictionaryHelper::Get(keyframe_dictionary, property, value);
    result.push_back(value);
    return true;
  }

  // Iterable object.
  while (iterator.Next(execution_context, exception_state)) {
    String value;
    if (!iterator.ValueAsString(value)) {
      exception_state.ThrowTypeError(
          "Unable to read keyframe value as string.");
      return false;
    }
    result.push_back(value);
  }
  return !exception_state.HadException();
}

EffectModel* EffectInput::ConvertObjectForm(
    Element& element,
    const Dictionary& keyframe_dictionary,
    ExecutionContext* execution_context,
    ExceptionState& exception_state) {
  StringKeyframeVector keyframes;

  String timing_function_string;
  RefPtr<TimingFunction> timing_function = nullptr;
  if (DictionaryHelper::Get(keyframe_dictionary, "easing",
                            timing_function_string)) {
    timing_function = AnimationInputHelpers::ParseTimingFunction(
        timing_function_string, &element.GetDocument(), exception_state);
    if (!timing_function)
      return nullptr;
  }

  Nullable<double> offset;
  if (DictionaryHelper::Get(keyframe_dictionary, "offset", offset) &&
      !offset.IsNull()) {
    if (!CheckOffset(offset.Get(), 0.0, exception_state))
      return nullptr;
  }

  String composite_string;
  DictionaryHelper::Get(keyframe_dictionary, "composite", composite_string);

  const Vector<String>& keyframe_properties =
      keyframe_dictionary.GetPropertyNames(exception_state);
  if (exception_state.HadException())
    return nullptr;
  for (const auto& property : keyframe_properties) {
    if (property == "offset" || property == "composite" ||
        property == "easing") {
      continue;
    }

    Vector<String> values;
    if (!GetPropertyIndexedKeyframeValues(keyframe_dictionary, property,
                                          execution_context, exception_state,
                                          values))
      return nullptr;

    size_t num_keyframes = values.size();
    for (size_t i = 0; i < num_keyframes; ++i) {
      RefPtr<StringKeyframe> keyframe = StringKeyframe::Create();

      if (!offset.IsNull())
        keyframe->SetOffset(offset.Get());
      else if (num_keyframes == 1)
        keyframe->SetOffset(1.0);
      else
        keyframe->SetOffset(i / (num_keyframes - 1.0));

      if (timing_function)
        keyframe->SetEasing(timing_function);

      if (composite_string == "add")
        keyframe->SetComposite(EffectModel::kCompositeAdd);
      // TODO(alancutter): Support "accumulate" keyframe composition.

      SetKeyframeValue(element, *keyframe.Get(), property, values[i],
                       execution_context);
      keyframes.push_back(keyframe);
    }
  }

  std::sort(keyframes.begin(), keyframes.end(), CompareKeyframes);

  DCHECK(!exception_state.HadException());

  return CreateEffectModelFromKeyframes(element, keyframes, exception_state);
}

}  // namespace blink
