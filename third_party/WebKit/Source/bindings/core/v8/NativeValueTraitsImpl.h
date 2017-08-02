// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NativeValueTraitsImpl_h
#define NativeValueTraitsImpl_h

#include "bindings/core/v8/IDLTypes.h"
#include "bindings/core/v8/NativeValueTraits.h"
#include "bindings/core/v8/V8BindingForCore.h"
#include "core/CoreExport.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

// Boolean
template <>
struct CORE_EXPORT NativeValueTraits<IDLBoolean>
    : public NativeValueTraitsBase<IDLBoolean> {
  static bool NativeValue(v8::Isolate* isolate,
                          v8::Local<v8::Value> value,
                          ExceptionState& exception_state) {
    return ToBoolean(isolate, value, exception_state);
  }
};

// Integers
//
// All integer specializations offer a second nativeValue() besides the default
// one: it takes an IntegerConversionConfiguration argument to let callers
// specify how the integers should be converted. The default nativeValue()
// overload will always use NormalConversion.
template <>
struct CORE_EXPORT NativeValueTraits<IDLByte>
    : public NativeValueTraitsBase<IDLByte> {
  static int8_t NativeValue(v8::Isolate* isolate,
                            v8::Local<v8::Value> value,
                            ExceptionState& exception_state) {
    return NativeValue(isolate, value, exception_state, kNormalConversion);
  }

  static int8_t NativeValue(v8::Isolate* isolate,
                            v8::Local<v8::Value> value,
                            ExceptionState& exception_state,
                            IntegerConversionConfiguration conversion_mode) {
    return ToInt8(isolate, value, conversion_mode, exception_state);
  }
};

template <>
struct CORE_EXPORT NativeValueTraits<IDLOctet>
    : public NativeValueTraitsBase<IDLOctet> {
  static uint8_t NativeValue(v8::Isolate* isolate,
                             v8::Local<v8::Value> value,
                             ExceptionState& exception_state) {
    return NativeValue(isolate, value, exception_state, kNormalConversion);
  }

  static uint8_t NativeValue(v8::Isolate* isolate,
                             v8::Local<v8::Value> value,
                             ExceptionState& exception_state,
                             IntegerConversionConfiguration conversion_mode) {
    return ToUInt8(isolate, value, conversion_mode, exception_state);
  }
};

template <>
struct CORE_EXPORT NativeValueTraits<IDLShort>
    : public NativeValueTraitsBase<IDLShort> {
  static int16_t NativeValue(v8::Isolate* isolate,
                             v8::Local<v8::Value> value,
                             ExceptionState& exception_state) {
    return NativeValue(isolate, value, exception_state, kNormalConversion);
  }

  static int16_t NativeValue(v8::Isolate* isolate,
                             v8::Local<v8::Value> value,
                             ExceptionState& exception_state,
                             IntegerConversionConfiguration conversion_mode) {
    return ToInt16(isolate, value, conversion_mode, exception_state);
  }
};

template <>
struct CORE_EXPORT NativeValueTraits<IDLUnsignedShort>
    : public NativeValueTraitsBase<IDLUnsignedShort> {
  static uint16_t NativeValue(v8::Isolate* isolate,
                              v8::Local<v8::Value> value,
                              ExceptionState& exception_state) {
    return NativeValue(isolate, value, exception_state, kNormalConversion);
  }

  static uint16_t NativeValue(v8::Isolate* isolate,
                              v8::Local<v8::Value> value,
                              ExceptionState& exception_state,
                              IntegerConversionConfiguration conversion_mode) {
    return ToUInt16(isolate, value, conversion_mode, exception_state);
  }
};

template <>
struct CORE_EXPORT NativeValueTraits<IDLLong>
    : public NativeValueTraitsBase<IDLLong> {
  static int32_t NativeValue(v8::Isolate* isolate,
                             v8::Local<v8::Value> value,
                             ExceptionState& exception_state) {
    return NativeValue(isolate, value, exception_state, kNormalConversion);
  }

  static int32_t NativeValue(v8::Isolate* isolate,
                             v8::Local<v8::Value> value,
                             ExceptionState& exception_state,
                             IntegerConversionConfiguration conversion_mode) {
    return ToInt32(isolate, value, conversion_mode, exception_state);
  }
};

template <>
struct CORE_EXPORT NativeValueTraits<IDLUnsignedLong>
    : public NativeValueTraitsBase<IDLUnsignedLong> {
  static uint32_t NativeValue(v8::Isolate* isolate,
                              v8::Local<v8::Value> value,
                              ExceptionState& exception_state) {
    return NativeValue(isolate, value, exception_state, kNormalConversion);
  }

  static uint32_t NativeValue(v8::Isolate* isolate,
                              v8::Local<v8::Value> value,
                              ExceptionState& exception_state,
                              IntegerConversionConfiguration conversion_mode) {
    return ToUInt32(isolate, value, conversion_mode, exception_state);
  }
};

template <>
struct CORE_EXPORT NativeValueTraits<IDLLongLong>
    : public NativeValueTraitsBase<IDLLongLong> {
  static int64_t NativeValue(v8::Isolate* isolate,
                             v8::Local<v8::Value> value,
                             ExceptionState& exception_state) {
    return NativeValue(isolate, value, exception_state, kNormalConversion);
  }

  static int64_t NativeValue(v8::Isolate* isolate,
                             v8::Local<v8::Value> value,
                             ExceptionState& exception_state,
                             IntegerConversionConfiguration conversion_mode) {
    return ToInt64(isolate, value, conversion_mode, exception_state);
  }
};

template <>
struct CORE_EXPORT NativeValueTraits<IDLUnsignedLongLong>
    : public NativeValueTraitsBase<IDLUnsignedLongLong> {
  static uint64_t NativeValue(v8::Isolate* isolate,
                              v8::Local<v8::Value> value,
                              ExceptionState& exception_state) {
    return NativeValue(isolate, value, exception_state, kNormalConversion);
  }

  static uint64_t NativeValue(v8::Isolate* isolate,
                              v8::Local<v8::Value> value,
                              ExceptionState& exception_state,
                              IntegerConversionConfiguration conversion_mode) {
    return ToUInt64(isolate, value, conversion_mode, exception_state);
  }
};

// Strings
template <>
struct CORE_EXPORT NativeValueTraits<IDLByteString>
    : public NativeValueTraitsBase<IDLByteString> {
  static String NativeValue(v8::Isolate* isolate,
                            v8::Local<v8::Value> value,
                            ExceptionState& exception_state) {
    return ToByteString(isolate, value, exception_state);
  }
};

template <>
struct CORE_EXPORT NativeValueTraits<IDLString>
    : public NativeValueTraitsBase<IDLString> {
  static String NativeValue(v8::Isolate* isolate,
                            v8::Local<v8::Value> value,
                            ExceptionState& exception_state) {
    return NativeValue<V8StringResourceMode::kDefaultMode>(isolate, value,
                                                           exception_state);
  }

  template <V8StringResourceMode Mode = kDefaultMode>
  static String NativeValue(v8::Isolate* isolate,
                            v8::Local<v8::Value> value,
                            ExceptionState& exception_state) {
    V8StringResource<Mode> string(value);
    if (!string.Prepare(isolate, exception_state))
      return String();
    return string;
  }
};

template <>
struct CORE_EXPORT NativeValueTraits<IDLUSVString>
    : public NativeValueTraitsBase<IDLUSVString> {
  static String NativeValue(v8::Isolate* isolate,
                            v8::Local<v8::Value> value,
                            ExceptionState& exception_state) {
    return ToUSVString(isolate, value, exception_state);
  }
};

// Floats and doubles
template <>
struct CORE_EXPORT NativeValueTraits<IDLDouble>
    : public NativeValueTraitsBase<IDLDouble> {
  static double NativeValue(v8::Isolate* isolate,
                            v8::Local<v8::Value> value,
                            ExceptionState& exception_state) {
    return ToRestrictedDouble(isolate, value, exception_state);
  }
};

template <>
struct CORE_EXPORT NativeValueTraits<IDLUnrestrictedDouble>
    : public NativeValueTraitsBase<IDLUnrestrictedDouble> {
  static double NativeValue(v8::Isolate* isolate,
                            v8::Local<v8::Value> value,
                            ExceptionState& exception_state) {
    return ToDouble(isolate, value, exception_state);
  }
};

template <>
struct CORE_EXPORT NativeValueTraits<IDLFloat>
    : public NativeValueTraitsBase<IDLFloat> {
  static float NativeValue(v8::Isolate* isolate,
                           v8::Local<v8::Value> value,
                           ExceptionState& exception_state) {
    return ToRestrictedFloat(isolate, value, exception_state);
  }
};

template <>
struct CORE_EXPORT NativeValueTraits<IDLUnrestrictedFloat>
    : public NativeValueTraitsBase<IDLUnrestrictedFloat> {
  static float NativeValue(v8::Isolate* isolate,
                           v8::Local<v8::Value> value,
                           ExceptionState& exception_state) {
    return ToFloat(isolate, value, exception_state);
  }
};

// Promises
template <>
struct CORE_EXPORT NativeValueTraits<IDLPromise>
    : public NativeValueTraitsBase<IDLPromise> {
  static ScriptPromise NativeValue(v8::Isolate* isolate,
                                   v8::Local<v8::Value> value,
                                   ExceptionState& exception_state) {
    return NativeValue(isolate, value);
  }

  static ScriptPromise NativeValue(v8::Isolate* isolate,
                                   v8::Local<v8::Value> value) {
    return ScriptPromise::Cast(ScriptState::Current(isolate), value);
  }
};

// Type-specific overloads
template <>
struct CORE_EXPORT NativeValueTraits<IDLDate>
    : public NativeValueTraitsBase<IDLDate> {
  static double NativeValue(v8::Isolate* isolate,
                            v8::Local<v8::Value> value,
                            ExceptionState& exception_state) {
    return ToCoreDate(isolate, value, exception_state);
  }
};

// Sequences
template <typename T>
struct NativeValueTraits<IDLSequence<T>>
    : public NativeValueTraitsBase<IDLSequence<T>> {
  // Nondependent types need to be explicitly qualified to be accessible.
  using typename NativeValueTraitsBase<IDLSequence<T>>::ImplType;

  // https://heycam.github.io/webidl/#es-sequence
  static ImplType NativeValue(v8::Isolate* isolate,
                              v8::Local<v8::Value> value,
                              ExceptionState& exception_state) {
    if (!value->IsObject()) {
      exception_state.ThrowTypeError(
          "The provided value cannot be converted to a sequence.");
      return ImplType();
    }

    ImplType result;
    // TODO(rakuco): Checking for IsArray() may not be enough. Other engines
    // also prefer regular array iteration over a custom @@iterator when the
    // latter is defined, but it is not clear if this is a valid optimization.
    if (value->IsArray()) {
      ConvertSequenceFast(isolate, value.As<v8::Array>(), exception_state,
                          result);
    } else {
      ConvertSequenceSlow(isolate, value.As<v8::Object>(), exception_state,
                          result);
    }

    if (exception_state.HadException())
      return ImplType();
    return result;
  }

 private:
  // Fast case: we're interating over an Array that adheres to
  // %ArrayIteratorPrototype%'s protocol.
  static void ConvertSequenceFast(v8::Isolate* isolate,
                                  v8::Local<v8::Array> v8_array,
                                  ExceptionState& exception_state,
                                  ImplType& result) {
    const uint32_t length = v8_array->Length();
    if (length > ImplType::MaxCapacity()) {
      exception_state.ThrowRangeError("Array length exceeds supported limit.");
      return;
    }
    result.ReserveInitialCapacity(length);
    v8::TryCatch block(isolate);
    for (uint32_t i = 0; i < length; ++i) {
      v8::Local<v8::Value> element;
      if (!v8_array->Get(isolate->GetCurrentContext(), i).ToLocal(&element)) {
        exception_state.RethrowV8Exception(block.Exception());
        return;
      }
      result.UncheckedAppend(
          NativeValueTraits<T>::NativeValue(isolate, element, exception_state));
      if (exception_state.HadException())
        return;
    }
  }

  // Slow case: follow WebIDL's "Creating a sequence from an iterable" steps to
  // iterate through each element.
  // https://heycam.github.io/webidl/#create-sequence-from-iterable
  static void ConvertSequenceSlow(v8::Isolate* isolate,
                                  v8::Local<v8::Object> v8_object,
                                  ExceptionState& exception_state,
                                  ImplType& result) {
    v8::TryCatch block(isolate);

    v8::Local<v8::Object> iterator =
        GetEsIterator(isolate, v8_object, exception_state);
    if (exception_state.HadException())
      return;

    v8::Local<v8::String> next_key = V8AtomicString(isolate, "next");
    v8::Local<v8::String> value_key = V8AtomicString(isolate, "value");
    v8::Local<v8::String> done_key = V8AtomicString(isolate, "done");
    v8::Local<v8::Context> context = isolate->GetCurrentContext();
    while (true) {
      v8::Local<v8::Value> next;
      if (!iterator->Get(context, next_key).ToLocal(&next)) {
        exception_state.RethrowV8Exception(block.Exception());
        return;
      }
      if (!next->IsFunction()) {
        exception_state.ThrowTypeError("Iterator.next should be callable.");
        return;
      }
      v8::Local<v8::Value> next_result;
      if (!V8ScriptRunner::CallFunction(next.As<v8::Function>(),
                                        ToExecutionContext(context), iterator,
                                        0, nullptr, isolate)
               .ToLocal(&next_result)) {
        exception_state.RethrowV8Exception(block.Exception());
        return;
      }
      if (!next_result->IsObject()) {
        exception_state.ThrowTypeError(
            "Iterator.next() did not return an object.");
        return;
      }
      v8::Local<v8::Object> result_object = next_result.As<v8::Object>();
      v8::Local<v8::Value> element;
      v8::Local<v8::Value> done;
      if (!result_object->Get(context, value_key).ToLocal(&element) ||
          !result_object->Get(context, done_key).ToLocal(&done)) {
        exception_state.RethrowV8Exception(block.Exception());
        return;
      }
      bool done_boolean;
      if (!done->BooleanValue(context).To(&done_boolean)) {
        exception_state.RethrowV8Exception(block.Exception());
        return;
      }
      if (done_boolean)
        break;
      result.emplace_back(
          NativeValueTraits<T>::NativeValue(isolate, element, exception_state));
      if (exception_state.HadException())
        return;
    }
  }
};

// Records
template <typename K, typename V>
struct NativeValueTraits<IDLRecord<K, V>>
    : public NativeValueTraitsBase<IDLRecord<K, V>> {
  // Nondependent types need to be explicitly qualified to be accessible.
  using typename NativeValueTraitsBase<IDLRecord<K, V>>::ImplType;

  // Converts a JavaScript value |O| to an IDL record<K, V> value. In C++, a
  // record is represented as a Vector<std::pair<k, v>> (or a HeapVector if
  // necessary). See https://heycam.github.io/webidl/#es-record.
  static ImplType NativeValue(v8::Isolate* isolate,
                              v8::Local<v8::Value> v8_value,
                              ExceptionState& exception_state) {
    v8::Local<v8::Context> context = isolate->GetCurrentContext();

    // "1. If Type(O) is not Object, throw a TypeError."
    if (!v8_value->IsObject()) {
      exception_state.ThrowTypeError(
          "Only objects can be converted to record<K,V> types");
      return ImplType();
    }
    v8::Local<v8::Object> v8_object = v8::Local<v8::Object>::Cast(v8_value);
    v8::TryCatch block(isolate);

    // "3. Let keys be ? O.[[OwnPropertyKeys]]()."
    v8::Local<v8::Array> keys;
    // While we could pass v8::ONLY_ENUMERABLE below, doing so breaks
    // web-platform-tests' headers-record.html and deviates from the spec
    // algorithm.
    if (!v8_object
             ->GetOwnPropertyNames(context,
                                   static_cast<v8::PropertyFilter>(
                                       v8::PropertyFilter::ALL_PROPERTIES))
             .ToLocal(&keys)) {
      exception_state.RethrowV8Exception(block.Exception());
      return ImplType();
    }
    if (keys->Length() > ImplType::MaxCapacity()) {
      exception_state.ThrowRangeError("Array length exceeds supported limit.");
      return ImplType();
    }

    // "2. Let result be a new empty instance of record<K, V>."
    ImplType result;
    result.ReserveInitialCapacity(keys->Length());

    // The conversion algorithm needs a data structure with fast insertion at
    // the end while at the same time requiring fast checks for previous insert
    // of a given key. |seenKeys| is a key/position in |result| map that aids in
    // the latter part.
    HashMap<String, size_t> seen_keys;

    for (uint32_t i = 0; i < keys->Length(); ++i) {
      // "4. Repeat, for each element key of keys in List order:"
      v8::Local<v8::Value> key;
      if (!keys->Get(context, i).ToLocal(&key)) {
        exception_state.RethrowV8Exception(block.Exception());
        return ImplType();
      }

      // V8's GetOwnPropertyNames() does not convert numeric property indices
      // to strings, so we have to do it ourselves.
      if (!key->IsName())
        key = key->ToString(context).ToLocalChecked();

      // "4.1. Let desc be ? O.[[GetOwnProperty]](key)."
      v8::Local<v8::Value> desc;
      if (!v8_object->GetOwnPropertyDescriptor(context, key.As<v8::Name>())
               .ToLocal(&desc)) {
        exception_state.RethrowV8Exception(block.Exception());
        return ImplType();
      }

      // "4.2. If desc is not undefined and desc.[[Enumerable]] is true:"
      // We can call ToLocalChecked() and ToChecked() here because
      // GetOwnPropertyDescriptor is responsible for catching any exceptions
      // and failures, and if we got to this point of the code we have a proper
      // object that was not created by a user.
      if (desc->IsUndefined())
        continue;
      DCHECK(desc->IsObject());
      v8::Local<v8::Value> enumerable =
          v8::Local<v8::Object>::Cast(desc)
              ->Get(context, V8String(isolate, "enumerable"))
              .ToLocalChecked();
      if (!enumerable->BooleanValue(context).ToChecked())
        continue;

      // "4.2.1. Let typedKey be key converted to an IDL value of type K."
      String typed_key =
          NativeValueTraits<K>::NativeValue(isolate, key, exception_state);
      if (exception_state.HadException())
        return ImplType();

      // "4.2.2. Let value be ? Get(O, key)."
      v8::Local<v8::Value> value;
      if (!v8_object->Get(context, key).ToLocal(&value)) {
        exception_state.RethrowV8Exception(block.Exception());
        return ImplType();
      }

      // "4.2.3. Let typedValue be value converted to an IDL value of type V."
      typename ImplType::ValueType::second_type typed_value =
          NativeValueTraits<V>::NativeValue(isolate, value, exception_state);
      if (exception_state.HadException())
        return ImplType();

      if (seen_keys.Contains(typed_key)) {
        // "4.2.4. If typedKey is already a key in result, set its value to
        //         typedValue.
        //         Note: This can happen when O is a proxy object."
        const size_t pos = seen_keys.at(typed_key);
        result[pos] = std::make_pair(typed_key, typed_value);
      } else {
        // "4.2.5. Otherwise, append to result a mapping (typedKey,
        // typedValue)."
        // Note we can take this shortcut because we are always appending.
        const size_t pos = result.size();
        seen_keys.Set(typed_key, pos);
        result.UncheckedAppend(std::make_pair(typed_key, typed_value));
      }
    }
    // "5. Return result."
    return result;
  }
};

}  // namespace blink

#endif  // NativeValueTraitsImpl_h
