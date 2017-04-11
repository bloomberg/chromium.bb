/*
* Copyright (C) 2009 Google Inc. All rights reserved.
* Copyright (C) 2012 Ericsson AB. All rights reserved.
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

#ifndef V8Binding_h
#define V8Binding_h

#include "bindings/core/v8/DOMDataStore.h"
#include "bindings/core/v8/DOMWrapperWorld.h"
#include "bindings/core/v8/ExceptionMessages.h"
#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/NativeValueTraits.h"
#include "bindings/core/v8/ScriptState.h"
#include "bindings/core/v8/ScriptValue.h"
#include "bindings/core/v8/ScriptWrappable.h"
#include "bindings/core/v8/V8BindingMacros.h"
#include "bindings/core/v8/V8PerIsolateData.h"
#include "bindings/core/v8/V8ScriptRunner.h"
#include "bindings/core/v8/V8StringResource.h"
#include "bindings/core/v8/V8ThrowException.h"
#include "bindings/core/v8/V8ValueCache.h"
#include "core/CoreExport.h"
#include "core/dom/NotShared.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/text/AtomicString.h"
#include "platform/wtf/text/StringView.h"
#include "v8/include/v8.h"

namespace blink {

class DOMWindow;
class EventListener;
class EventTarget;
class ExceptionState;
class ExecutionContext;
class FlexibleArrayBufferView;
class Frame;
class LocalDOMWindow;
class LocalFrame;
class NodeFilter;
class XPathNSResolver;

template <typename T>
struct V8TypeOf {
  STATIC_ONLY(V8TypeOf);
  // |Type| provides C++ -> V8 type conversion for DOM wrappers.
  // The Blink binding code generator will generate specialized version of
  // V8TypeOf for each wrapper class.
  typedef void Type;
};

template <typename CallbackInfo, typename S>
inline void V8SetReturnValue(const CallbackInfo& info,
                             const v8::Persistent<S>& handle) {
  info.GetReturnValue().Set(handle);
}

template <typename CallbackInfo, typename S>
inline void V8SetReturnValue(const CallbackInfo& info,
                             const v8::Local<S> handle) {
  info.GetReturnValue().Set(handle);
}

template <typename CallbackInfo, typename S>
inline void V8SetReturnValue(const CallbackInfo& info,
                             v8::MaybeLocal<S> maybe) {
  if (LIKELY(!maybe.IsEmpty()))
    info.GetReturnValue().Set(maybe.ToLocalChecked());
}

template <typename CallbackInfo>
inline void V8SetReturnValue(const CallbackInfo& info, bool value) {
  info.GetReturnValue().Set(value);
}

template <typename CallbackInfo>
inline void V8SetReturnValue(const CallbackInfo& info, double value) {
  info.GetReturnValue().Set(value);
}

template <typename CallbackInfo>
inline void V8SetReturnValue(const CallbackInfo& info, int32_t value) {
  info.GetReturnValue().Set(value);
}

template <typename CallbackInfo>
inline void V8SetReturnValue(const CallbackInfo& info, uint32_t value) {
  info.GetReturnValue().Set(value);
}

template <typename CallbackInfo>
inline void V8SetReturnValueBool(const CallbackInfo& info, bool v) {
  info.GetReturnValue().Set(v);
}

template <typename CallbackInfo>
inline void V8SetReturnValueInt(const CallbackInfo& info, int v) {
  info.GetReturnValue().Set(v);
}

template <typename CallbackInfo>
inline void V8SetReturnValueUnsigned(const CallbackInfo& info, unsigned v) {
  info.GetReturnValue().Set(v);
}

template <typename CallbackInfo>
inline void V8SetReturnValueNull(const CallbackInfo& info) {
  info.GetReturnValue().SetNull();
}

template <typename CallbackInfo>
inline void V8SetReturnValueUndefined(const CallbackInfo& info) {
  info.GetReturnValue().SetUndefined();
}

template <typename CallbackInfo>
inline void V8SetReturnValueEmptyString(const CallbackInfo& info) {
  info.GetReturnValue().SetEmptyString();
}

template <typename CallbackInfo>
inline void V8SetReturnValueString(const CallbackInfo& info,
                                   const String& string,
                                   v8::Isolate* isolate) {
  if (string.IsNull()) {
    V8SetReturnValueEmptyString(info);
    return;
  }
  V8PerIsolateData::From(isolate)->GetStringCache()->SetReturnValueFromString(
      info.GetReturnValue(), string.Impl());
}

template <typename CallbackInfo>
inline void V8SetReturnValueStringOrNull(const CallbackInfo& info,
                                         const String& string,
                                         v8::Isolate* isolate) {
  if (string.IsNull()) {
    V8SetReturnValueNull(info);
    return;
  }
  V8PerIsolateData::From(isolate)->GetStringCache()->SetReturnValueFromString(
      info.GetReturnValue(), string.Impl());
}

template <typename CallbackInfo>
inline void V8SetReturnValue(const CallbackInfo& callback_info,
                             ScriptWrappable* impl,
                             v8::Local<v8::Object> creation_context) {
  if (UNLIKELY(!impl)) {
    V8SetReturnValueNull(callback_info);
    return;
  }
  if (DOMDataStore::SetReturnValue(callback_info.GetReturnValue(), impl))
    return;
  v8::Local<v8::Object> wrapper =
      impl->Wrap(callback_info.GetIsolate(), creation_context);
  V8SetReturnValue(callback_info, wrapper);
}

template <typename CallbackInfo>
inline void V8SetReturnValue(const CallbackInfo& callback_info,
                             ScriptWrappable* impl) {
  V8SetReturnValue(callback_info, impl, callback_info.Holder());
}

template <typename CallbackInfo>
inline void V8SetReturnValue(const CallbackInfo& callback_info, Node* impl) {
  V8SetReturnValue(callback_info, ScriptWrappable::FromNode(impl));
}

// Special versions for DOMWindow and EventTarget

template <typename CallbackInfo>
inline void V8SetReturnValue(const CallbackInfo& callback_info,
                             DOMWindow* impl) {
  V8SetReturnValue(callback_info, ToV8(impl, callback_info.Holder(),
                                       callback_info.GetIsolate()));
}

template <typename CallbackInfo>
inline void V8SetReturnValue(const CallbackInfo& callback_info,
                             EventTarget* impl) {
  V8SetReturnValue(callback_info, ToV8(impl, callback_info.Holder(),
                                       callback_info.GetIsolate()));
}

template <typename CallbackInfo, typename T>
inline void V8SetReturnValue(const CallbackInfo& callback_info,
                             PassRefPtr<T> impl) {
  V8SetReturnValue(callback_info, impl.Get());
}

template <typename CallbackInfo, typename T>
inline void V8SetReturnValue(const CallbackInfo& callbackInfo,
                             NotShared<T> notShared) {
  V8SetReturnValue(callbackInfo, notShared.View());
}

template <typename CallbackInfo>
inline void V8SetReturnValueForMainWorld(const CallbackInfo& callback_info,
                                         ScriptWrappable* impl) {
  ASSERT(DOMWrapperWorld::Current(callback_info.GetIsolate()).IsMainWorld());
  if (UNLIKELY(!impl)) {
    V8SetReturnValueNull(callback_info);
    return;
  }
  if (DOMDataStore::SetReturnValueForMainWorld(callback_info.GetReturnValue(),
                                               impl))
    return;
  v8::Local<v8::Object> wrapper =
      impl->Wrap(callback_info.GetIsolate(), callback_info.Holder());
  V8SetReturnValue(callback_info, wrapper);
}

template <typename CallbackInfo>
inline void V8SetReturnValueForMainWorld(const CallbackInfo& callback_info,
                                         Node* impl) {
  // Since EventTarget has a special version of ToV8 and V8EventTarget.h
  // defines its own v8SetReturnValue family, which are slow, we need to
  // override them with optimized versions for Node and its subclasses.
  // Without this overload, v8SetReturnValueForMainWorld for Node would be
  // very slow.
  //
  // class hierarchy:
  //     ScriptWrappable <-- EventTarget <--+-- Node <-- ...
  //                                        +-- Window
  // overloads:
  //     v8SetReturnValueForMainWorld(ScriptWrappable*)
  //         Optimized and very fast.
  //     v8SetReturnValueForMainWorld(EventTarget*)
  //         Uses custom toV8 function and slow.
  //     v8SetReturnValueForMainWorld(Node*)
  //         Optimized and very fast.
  //     v8SetReturnValueForMainWorld(Window*)
  //         Uses custom toV8 function and slow.
  V8SetReturnValueForMainWorld(callback_info, ScriptWrappable::FromNode(impl));
}

// Special versions for DOMWindow and EventTarget

template <typename CallbackInfo>
inline void V8SetReturnValueForMainWorld(const CallbackInfo& callback_info,
                                         DOMWindow* impl) {
  V8SetReturnValue(callback_info, ToV8(impl, callback_info.Holder(),
                                       callback_info.GetIsolate()));
}

template <typename CallbackInfo>
inline void V8SetReturnValueForMainWorld(const CallbackInfo& callback_info,
                                         EventTarget* impl) {
  V8SetReturnValue(callback_info, ToV8(impl, callback_info.Holder(),
                                       callback_info.GetIsolate()));
}

template <typename CallbackInfo, typename T>
inline void V8SetReturnValueForMainWorld(const CallbackInfo& callback_info,
                                         PassRefPtr<T> impl) {
  V8SetReturnValueForMainWorld(callback_info, impl.Get());
}

template <typename CallbackInfo>
inline void V8SetReturnValueFast(const CallbackInfo& callback_info,
                                 ScriptWrappable* impl,
                                 const ScriptWrappable* wrappable) {
  if (UNLIKELY(!impl)) {
    V8SetReturnValueNull(callback_info);
    return;
  }
  if (DOMDataStore::SetReturnValueFast(callback_info.GetReturnValue(), impl,
                                       callback_info.Holder(), wrappable))
    return;
  v8::Local<v8::Object> wrapper =
      impl->Wrap(callback_info.GetIsolate(), callback_info.Holder());
  V8SetReturnValue(callback_info, wrapper);
}

template <typename CallbackInfo>
inline void V8SetReturnValueFast(const CallbackInfo& callback_info,
                                 Node* impl,
                                 const ScriptWrappable* wrappable) {
  V8SetReturnValueFast(callback_info, ScriptWrappable::FromNode(impl),
                       wrappable);
}

// Special versions for DOMWindow and EventTarget

template <typename CallbackInfo>
inline void V8SetReturnValueFast(const CallbackInfo& callback_info,
                                 DOMWindow* impl,
                                 const ScriptWrappable*) {
  V8SetReturnValue(callback_info, ToV8(impl, callback_info.Holder(),
                                       callback_info.GetIsolate()));
}

template <typename CallbackInfo>
inline void V8SetReturnValueFast(const CallbackInfo& callback_info,
                                 EventTarget* impl,
                                 const ScriptWrappable*) {
  V8SetReturnValue(callback_info, ToV8(impl, callback_info.Holder(),
                                       callback_info.GetIsolate()));
}

template <typename CallbackInfo, typename T, typename Wrappable>
inline void V8SetReturnValueFast(const CallbackInfo& callback_info,
                                 PassRefPtr<T> impl,
                                 const Wrappable* wrappable) {
  V8SetReturnValueFast(callback_info, impl.Get(), wrappable);
}

template <typename CallbackInfo, typename T>
inline void V8SetReturnValueFast(const CallbackInfo& callback_info,
                                 const v8::Local<T> handle,
                                 const ScriptWrappable*) {
  V8SetReturnValue(callback_info, handle);
}

template <typename CallbackInfo, typename T>
inline void V8SetReturnValueFast(const CallbackInfo& callbackInfo,
                                 NotShared<T> notShared,
                                 const ScriptWrappable* wrappable) {
  V8SetReturnValueFast(callbackInfo, notShared.View(), wrappable);
}

// Convert v8::String to a WTF::String. If the V8 string is not already
// an external string then it is transformed into an external string at this
// point to avoid repeated conversions.
inline String ToCoreString(v8::Local<v8::String> value) {
  return V8StringToWebCoreString<String>(value, kExternalize);
}

inline String ToCoreStringWithNullCheck(v8::Local<v8::String> value) {
  if (value.IsEmpty() || value->IsNull())
    return String();
  return ToCoreString(value);
}

inline String ToCoreStringWithUndefinedOrNullCheck(
    v8::Local<v8::String> value) {
  if (value.IsEmpty() || value->IsNull() || value->IsUndefined())
    return String();
  return ToCoreString(value);
}

inline AtomicString ToCoreAtomicString(v8::Local<v8::String> value) {
  return V8StringToWebCoreString<AtomicString>(value, kExternalize);
}

// This method will return a null String if the v8::Value does not contain a
// v8::String.  It will not call ToString() on the v8::Value. If you want
// ToString() to be called, please use the TONATIVE_FOR_V8STRINGRESOURCE_*()
// macros instead.
inline String ToCoreStringWithUndefinedOrNullCheck(v8::Local<v8::Value> value) {
  if (value.IsEmpty() || !value->IsString())
    return String();
  return ToCoreString(value.As<v8::String>());
}

// Convert a string to a V8 string.

inline v8::Local<v8::String> V8String(v8::Isolate* isolate,
                                      const StringView& string) {
  DCHECK(isolate);
  if (string.IsNull())
    return v8::String::Empty(isolate);
  if (StringImpl* impl = string.SharedImpl())
    return V8PerIsolateData::From(isolate)->GetStringCache()->V8ExternalString(
        isolate, impl);
  if (string.Is8Bit())
    return v8::String::NewFromOneByte(
               isolate, reinterpret_cast<const uint8_t*>(string.Characters8()),
               v8::NewStringType::kNormal, static_cast<int>(string.length()))
        .ToLocalChecked();
  return v8::String::NewFromTwoByte(
             isolate, reinterpret_cast<const uint16_t*>(string.Characters16()),
             v8::NewStringType::kNormal, static_cast<int>(string.length()))
      .ToLocalChecked();
}

// As above, for string literals. The compiler doesn't optimize away the is8Bit
// and sharedImpl checks for string literals in the StringView version.
inline v8::Local<v8::String> V8String(v8::Isolate* isolate,
                                      const char* string) {
  DCHECK(isolate);
  if (!string || string[0] == '\0')
    return v8::String::Empty(isolate);
  return v8::String::NewFromOneByte(isolate,
                                    reinterpret_cast<const uint8_t*>(string),
                                    v8::NewStringType::kNormal, strlen(string))
      .ToLocalChecked();
}

inline v8::Local<v8::Value> V8StringOrNull(v8::Isolate* isolate,
                                           const AtomicString& string) {
  if (string.IsNull())
    return v8::Null(isolate);
  return V8PerIsolateData::From(isolate)->GetStringCache()->V8ExternalString(
      isolate, string.Impl());
}

inline v8::Local<v8::String> V8AtomicString(v8::Isolate* isolate,
                                            const StringView& string) {
  DCHECK(isolate);
  if (string.Is8Bit())
    return v8::String::NewFromOneByte(
               isolate, reinterpret_cast<const uint8_t*>(string.Characters8()),
               v8::NewStringType::kInternalized,
               static_cast<int>(string.length()))
        .ToLocalChecked();
  return v8::String::NewFromTwoByte(
             isolate, reinterpret_cast<const uint16_t*>(string.Characters16()),
             v8::NewStringType::kInternalized,
             static_cast<int>(string.length()))
      .ToLocalChecked();
}

// As above, for string literals. The compiler doesn't optimize away the is8Bit
// check for string literals in the StringView version.
inline v8::Local<v8::String> V8AtomicString(v8::Isolate* isolate,
                                            const char* string) {
  DCHECK(isolate);
  if (!string || string[0] == '\0')
    return v8::String::Empty(isolate);
  return v8::String::NewFromOneByte(
             isolate, reinterpret_cast<const uint8_t*>(string),
             v8::NewStringType::kInternalized, strlen(string))
      .ToLocalChecked();
}

inline v8::Local<v8::String> V8StringFromUtf8(v8::Isolate* isolate,
                                              const char* bytes,
                                              int length) {
  DCHECK(isolate);
  return v8::String::NewFromUtf8(isolate, bytes, v8::NewStringType::kNormal,
                                 length)
      .ToLocalChecked();
}

inline v8::Local<v8::Value> V8Undefined() {
  return v8::Local<v8::Value>();
}

// Conversion flags, used in toIntXX/toUIntXX.
enum IntegerConversionConfiguration {
  kNormalConversion,
  kEnforceRange,
  kClamp
};

// Convert a value to a boolean.
CORE_EXPORT bool ToBooleanSlow(v8::Isolate*,
                               v8::Local<v8::Value>,
                               ExceptionState&);
inline bool ToBoolean(v8::Isolate* isolate,
                      v8::Local<v8::Value> value,
                      ExceptionState& exception_state) {
  if (LIKELY(value->IsBoolean()))
    return value.As<v8::Boolean>()->Value();
  return ToBooleanSlow(isolate, value, exception_state);
}

// Convert a value to a 8-bit signed integer. The conversion fails if the
// value cannot be converted to a number or the range violated per WebIDL:
// http://www.w3.org/TR/WebIDL/#es-byte
CORE_EXPORT int8_t ToInt8(v8::Isolate*,
                          v8::Local<v8::Value>,
                          IntegerConversionConfiguration,
                          ExceptionState&);

// Convert a value to a 8-bit unsigned integer. The conversion fails if the
// value cannot be converted to a number or the range violated per WebIDL:
// http://www.w3.org/TR/WebIDL/#es-octet
CORE_EXPORT uint8_t ToUInt8(v8::Isolate*,
                            v8::Local<v8::Value>,
                            IntegerConversionConfiguration,
                            ExceptionState&);

// Convert a value to a 16-bit signed integer. The conversion fails if the
// value cannot be converted to a number or the range violated per WebIDL:
// http://www.w3.org/TR/WebIDL/#es-short
CORE_EXPORT int16_t ToInt16(v8::Isolate*,
                            v8::Local<v8::Value>,
                            IntegerConversionConfiguration,
                            ExceptionState&);

// Convert a value to a 16-bit unsigned integer. The conversion fails if the
// value cannot be converted to a number or the range violated per WebIDL:
// http://www.w3.org/TR/WebIDL/#es-unsigned-short
CORE_EXPORT uint16_t ToUInt16(v8::Isolate*,
                              v8::Local<v8::Value>,
                              IntegerConversionConfiguration,
                              ExceptionState&);

// Convert a value to a 32-bit signed integer. The conversion fails if the
// value cannot be converted to a number or the range violated per WebIDL:
// http://www.w3.org/TR/WebIDL/#es-long
CORE_EXPORT int32_t ToInt32Slow(v8::Isolate*,
                                v8::Local<v8::Value>,
                                IntegerConversionConfiguration,
                                ExceptionState&);
inline int32_t ToInt32(v8::Isolate* isolate,
                       v8::Local<v8::Value> value,
                       IntegerConversionConfiguration configuration,
                       ExceptionState& exception_state) {
  // Fast case. The value is already a 32-bit integer.
  if (LIKELY(value->IsInt32()))
    return value.As<v8::Int32>()->Value();
  return ToInt32Slow(isolate, value, configuration, exception_state);
}

// Convert a value to a 32-bit unsigned integer. The conversion fails if the
// value cannot be converted to a number or the range violated per WebIDL:
// http://www.w3.org/TR/WebIDL/#es-unsigned-long
CORE_EXPORT uint32_t ToUInt32Slow(v8::Isolate*,
                                  v8::Local<v8::Value>,
                                  IntegerConversionConfiguration,
                                  ExceptionState&);
inline uint32_t ToUInt32(v8::Isolate* isolate,
                         v8::Local<v8::Value> value,
                         IntegerConversionConfiguration configuration,
                         ExceptionState& exception_state) {
  // Fast case. The value is already a 32-bit unsigned integer.
  if (LIKELY(value->IsUint32()))
    return value.As<v8::Uint32>()->Value();

  // Fast case. The value is a 32-bit signed integer with NormalConversion
  // configuration.
  if (LIKELY(value->IsInt32() && configuration == kNormalConversion))
    return value.As<v8::Int32>()->Value();

  return ToUInt32Slow(isolate, value, configuration, exception_state);
}

// Convert a value to a 64-bit signed integer. The conversion fails if the
// value cannot be converted to a number or the range violated per WebIDL:
// http://www.w3.org/TR/WebIDL/#es-long-long
CORE_EXPORT int64_t ToInt64Slow(v8::Isolate*,
                                v8::Local<v8::Value>,
                                IntegerConversionConfiguration,
                                ExceptionState&);
inline int64_t ToInt64(v8::Isolate* isolate,
                       v8::Local<v8::Value> value,
                       IntegerConversionConfiguration configuration,
                       ExceptionState& exception_state) {
  // Clamping not supported for int64_t/long long int. See
  // Source/wtf/MathExtras.h.
  ASSERT(configuration != kClamp);

  // Fast case. The value is a 32-bit integer.
  if (LIKELY(value->IsInt32()))
    return value.As<v8::Int32>()->Value();

  return ToInt64Slow(isolate, value, configuration, exception_state);
}

// Convert a value to a 64-bit unsigned integer. The conversion fails if the
// value cannot be converted to a number or the range violated per WebIDL:
// http://www.w3.org/TR/WebIDL/#es-unsigned-long-long
CORE_EXPORT uint64_t ToUInt64Slow(v8::Isolate*,
                                  v8::Local<v8::Value>,
                                  IntegerConversionConfiguration,
                                  ExceptionState&);
inline uint64_t ToUInt64(v8::Isolate* isolate,
                         v8::Local<v8::Value> value,
                         IntegerConversionConfiguration configuration,
                         ExceptionState& exception_state) {
  // Fast case. The value is a 32-bit unsigned integer.
  if (LIKELY(value->IsUint32()))
    return value.As<v8::Uint32>()->Value();

  if (LIKELY(value->IsInt32() && configuration == kNormalConversion))
    return value.As<v8::Int32>()->Value();

  return ToUInt64Slow(isolate, value, configuration, exception_state);
}

// Convert a value to a double precision float, which might fail.
CORE_EXPORT double ToDoubleSlow(v8::Isolate*,
                                v8::Local<v8::Value>,
                                ExceptionState&);
inline double ToDouble(v8::Isolate* isolate,
                       v8::Local<v8::Value> value,
                       ExceptionState& exception_state) {
  if (LIKELY(value->IsNumber()))
    return value.As<v8::Number>()->Value();
  return ToDoubleSlow(isolate, value, exception_state);
}

// Convert a value to a double precision float, throwing on non-finite values.
CORE_EXPORT double ToRestrictedDouble(v8::Isolate*,
                                      v8::Local<v8::Value>,
                                      ExceptionState&);

// Convert a value to a single precision float, which might fail.
inline float ToFloat(v8::Isolate* isolate,
                     v8::Local<v8::Value> value,
                     ExceptionState& exception_state) {
  return static_cast<float>(ToDouble(isolate, value, exception_state));
}

// Convert a value to a single precision float, throwing on non-finite values.
CORE_EXPORT float ToRestrictedFloat(v8::Isolate*,
                                    v8::Local<v8::Value>,
                                    ExceptionState&);

// Converts a value to a String, throwing if any code unit is outside 0-255.
CORE_EXPORT String ToByteString(v8::Isolate*,
                                v8::Local<v8::Value>,
                                ExceptionState&);

// Converts a value to a String, replacing unmatched UTF-16 surrogates with
// replacement characters.
CORE_EXPORT String ToUSVString(v8::Isolate*,
                               v8::Local<v8::Value>,
                               ExceptionState&);

inline v8::Local<v8::Boolean> V8Boolean(bool value, v8::Isolate* isolate) {
  return value ? v8::True(isolate) : v8::False(isolate);
}

inline double ToCoreDate(v8::Isolate* isolate,
                         v8::Local<v8::Value> object,
                         ExceptionState& exception_state) {
  if (object->IsNull())
    return std::numeric_limits<double>::quiet_NaN();
  if (!object->IsDate()) {
    exception_state.ThrowTypeError("The provided value is not a Date.");
    return 0;
  }
  return object.As<v8::Date>()->ValueOf();
}

inline v8::MaybeLocal<v8::Value> V8DateOrNaN(v8::Isolate* isolate,
                                             double value) {
  ASSERT(isolate);
  return v8::Date::New(isolate->GetCurrentContext(), value);
}

// FIXME: Remove the special casing for NodeFilter and XPathNSResolver.
NodeFilter* ToNodeFilter(v8::Local<v8::Value>,
                         v8::Local<v8::Object>,
                         ScriptState*);
XPathNSResolver* ToXPathNSResolver(ScriptState*, v8::Local<v8::Value>);

bool ToV8Sequence(v8::Local<v8::Value>,
                  uint32_t& length,
                  v8::Isolate*,
                  ExceptionState&);

template <typename T>
HeapVector<Member<T>> ToMemberNativeArray(v8::Local<v8::Value> value,
                                          int argument_index,
                                          v8::Isolate* isolate,
                                          ExceptionState& exception_state) {
  v8::Local<v8::Value> v8_value(v8::Local<v8::Value>::New(isolate, value));
  uint32_t length = 0;
  if (value->IsArray()) {
    length = v8::Local<v8::Array>::Cast(v8_value)->Length();
  } else if (!ToV8Sequence(value, length, isolate, exception_state)) {
    if (!exception_state.HadException())
      exception_state.ThrowTypeError(
          ExceptionMessages::NotAnArrayTypeArgumentOrValue(argument_index));
    return HeapVector<Member<T>>();
  }

  using VectorType = HeapVector<Member<T>>;
  if (length > VectorType::MaxCapacity()) {
    exception_state.ThrowRangeError("Array length exceeds supported limit.");
    return VectorType();
  }

  VectorType result;
  result.ReserveInitialCapacity(length);
  v8::Local<v8::Object> object = v8::Local<v8::Object>::Cast(v8_value);
  v8::TryCatch block(isolate);
  for (uint32_t i = 0; i < length; ++i) {
    v8::Local<v8::Value> element;
    if (!V8Call(object->Get(isolate->GetCurrentContext(), i), element, block)) {
      exception_state.RethrowV8Exception(block.Exception());
      return VectorType();
    }
    if (V8TypeOf<T>::Type::hasInstance(element, isolate)) {
      v8::Local<v8::Object> element_object =
          v8::Local<v8::Object>::Cast(element);
      result.UncheckedAppend(V8TypeOf<T>::Type::toImpl(element_object));
    } else {
      exception_state.ThrowTypeError("Invalid Array element type");
      return VectorType();
    }
  }
  return result;
}

template <typename T>
HeapVector<Member<T>> ToMemberNativeArray(v8::Local<v8::Value> value,
                                          const String& property_name,
                                          v8::Isolate* isolate,
                                          ExceptionState& exception_state) {
  v8::Local<v8::Value> v8_value(v8::Local<v8::Value>::New(isolate, value));
  uint32_t length = 0;
  if (value->IsArray()) {
    length = v8::Local<v8::Array>::Cast(v8_value)->Length();
  } else if (!ToV8Sequence(value, length, isolate, exception_state)) {
    if (!exception_state.HadException())
      exception_state.ThrowTypeError(
          ExceptionMessages::NotASequenceTypeProperty(property_name));
    return HeapVector<Member<T>>();
  }

  using VectorType = HeapVector<Member<T>>;
  if (length > VectorType::MaxCapacity()) {
    exception_state.ThrowRangeError("Array length exceeds supported limit.");
    return VectorType();
  }

  VectorType result;
  result.ReserveInitialCapacity(length);
  v8::Local<v8::Object> object = v8::Local<v8::Object>::Cast(v8_value);
  v8::TryCatch block(isolate);
  for (uint32_t i = 0; i < length; ++i) {
    v8::Local<v8::Value> element;
    if (!V8Call(object->Get(isolate->GetCurrentContext(), i), element, block)) {
      exception_state.RethrowV8Exception(block.Exception());
      return VectorType();
    }
    if (V8TypeOf<T>::Type::hasInstance(element, isolate)) {
      v8::Local<v8::Object> element_object =
          v8::Local<v8::Object>::Cast(element);
      result.UncheckedAppend(V8TypeOf<T>::Type::toImpl(element_object));
    } else {
      exception_state.ThrowTypeError("Invalid Array element type");
      return VectorType();
    }
  }
  return result;
}

// Converts a JavaScript value to an array as per the Web IDL specification:
// http://www.w3.org/TR/2012/CR-WebIDL-20120419/#es-array
template <typename VectorType,
          typename ValueType = typename VectorType::ValueType>
VectorType ToImplArray(v8::Local<v8::Value> value,
                       int argument_index,
                       v8::Isolate* isolate,
                       ExceptionState& exception_state) {
  typedef NativeValueTraits<ValueType> TraitsType;

  uint32_t length = 0;
  if (value->IsArray()) {
    length = v8::Local<v8::Array>::Cast(value)->Length();
  } else if (!ToV8Sequence(value, length, isolate, exception_state)) {
    if (!exception_state.HadException())
      exception_state.ThrowTypeError(
          ExceptionMessages::NotAnArrayTypeArgumentOrValue(argument_index));
    return VectorType();
  }

  if (length > VectorType::MaxCapacity()) {
    exception_state.ThrowRangeError("Array length exceeds supported limit.");
    return VectorType();
  }

  VectorType result;
  result.ReserveInitialCapacity(length);
  v8::Local<v8::Object> object = v8::Local<v8::Object>::Cast(value);
  v8::TryCatch block(isolate);
  for (uint32_t i = 0; i < length; ++i) {
    v8::Local<v8::Value> element;
    if (!V8Call(object->Get(isolate->GetCurrentContext(), i), element, block)) {
      exception_state.RethrowV8Exception(block.Exception());
      return VectorType();
    }
    result.UncheckedAppend(
        TraitsType::NativeValue(isolate, element, exception_state));
    if (exception_state.HadException())
      return VectorType();
  }
  return result;
}

template <typename VectorType>
VectorType ToImplArray(const Vector<ScriptValue>& value,
                       v8::Isolate* isolate,
                       ExceptionState& exception_state) {
  using ValueType = typename VectorType::ValueType;
  using TraitsType = NativeValueTraits<ValueType>;

  if (value.size() > VectorType::MaxCapacity()) {
    exception_state.ThrowRangeError("Array length exceeds supported limit.");
    return VectorType();
  }

  VectorType result;
  result.ReserveInitialCapacity(value.size());
  for (unsigned i = 0; i < value.size(); ++i) {
    result.UncheckedAppend(
        TraitsType::NativeValue(isolate, value[i].V8Value(), exception_state));
    if (exception_state.HadException())
      return VectorType();
  }
  return result;
}

template <typename VectorType>
VectorType ToImplArguments(const v8::FunctionCallbackInfo<v8::Value>& info,
                           int start_index,
                           ExceptionState& exception_state) {
  using ValueType = typename VectorType::ValueType;
  using TraitsType = NativeValueTraits<ValueType>;

  int length = info.Length();
  VectorType result;
  if (start_index < length) {
    if (static_cast<size_t>(length - start_index) > VectorType::MaxCapacity()) {
      exception_state.ThrowRangeError("Array length exceeds supported limit.");
      return VectorType();
    }
    result.ReserveInitialCapacity(length - start_index);
    for (int i = start_index; i < length; ++i) {
      result.UncheckedAppend(
          TraitsType::NativeValue(info.GetIsolate(), info[i], exception_state));
      if (exception_state.HadException())
        return VectorType();
    }
  }
  return result;
}

// Gets an iterator from an Object.
CORE_EXPORT v8::Local<v8::Object> GetEsIterator(v8::Isolate*,
                                                v8::Local<v8::Object>,
                                                ExceptionState&);

// Validates that the passed object is a sequence type per WebIDL spec
// http://www.w3.org/TR/2012/CR-WebIDL-20120419/#es-sequence
inline bool ToV8Sequence(v8::Local<v8::Value> value,
                         uint32_t& length,
                         v8::Isolate* isolate,
                         ExceptionState& exception_state) {
  // Attempt converting to a sequence if the value is not already an array but
  // is any kind of object except for a native Date object or a native RegExp
  // object.
  ASSERT(!value->IsArray());
  // FIXME: Do we really need to special case Date and RegExp object?
  // https://www.w3.org/Bugs/Public/show_bug.cgi?id=22806
  if (!value->IsObject() || value->IsDate() || value->IsRegExp()) {
    // The caller is responsible for reporting a TypeError.
    return false;
  }

  v8::Local<v8::Object> object = v8::Local<v8::Object>::Cast(value);
  v8::Local<v8::String> length_symbol = V8AtomicString(isolate, "length");

  // FIXME: The specification states that the length property should be used as
  // fallback, if value is not a platform object that supports indexed
  // properties. If it supports indexed properties, length should actually be
  // one greater than value's maximum indexed property index.
  v8::TryCatch block(isolate);
  v8::Local<v8::Value> length_value;
  if (!V8Call(object->Get(isolate->GetCurrentContext(), length_symbol),
              length_value, block)) {
    exception_state.RethrowV8Exception(block.Exception());
    return false;
  }

  if (length_value->IsUndefined() || length_value->IsNull()) {
    // The caller is responsible for reporting a TypeError.
    return false;
  }

  uint32_t sequence_length;
  if (!V8Call(length_value->Uint32Value(isolate->GetCurrentContext()),
              sequence_length, block)) {
    exception_state.RethrowV8Exception(block.Exception());
    return false;
  }

  length = sequence_length;
  return true;
}

// TODO(rakuco): remove the specializations below (and consequently the
// non-IDLBase version of NativeValueTraitsBase) once we manage to convert all
// uses of NativeValueTraits to types that derive from IDLBase or for generated
// IDL interfaces/dictionaries/unions.
template <>
struct NativeValueTraits<String> {
  static inline String NativeValue(v8::Isolate* isolate,
                                   v8::Local<v8::Value> value,
                                   ExceptionState& exception_state) {
    V8StringResource<> string_value(value);
    if (!string_value.Prepare(exception_state))
      return String();
    return string_value;
  }
};

template <>
struct NativeValueTraits<AtomicString> {
  static inline AtomicString NativeValue(v8::Isolate* isolate,
                                         v8::Local<v8::Value> value,
                                         ExceptionState& exception_state) {
    V8StringResource<> string_value(value);
    if (!string_value.Prepare(exception_state))
      return AtomicString();
    return string_value;
  }
};

template <>
struct NativeValueTraits<int> {
  static inline int NativeValue(v8::Isolate* isolate,
                                v8::Local<v8::Value> value,
                                ExceptionState& exception_state) {
    return ToInt32(isolate, value, kNormalConversion, exception_state);
  }
};

template <>
struct NativeValueTraits<unsigned> {
  static inline unsigned NativeValue(v8::Isolate* isolate,
                                     v8::Local<v8::Value> value,
                                     ExceptionState& exception_state) {
    return ToUInt32(isolate, value, kNormalConversion, exception_state);
  }
};

template <>
struct NativeValueTraits<float> {
  static inline float NativeValue(v8::Isolate* isolate,
                                  v8::Local<v8::Value> value,
                                  ExceptionState& exception_state) {
    return ToFloat(isolate, value, exception_state);
  }
};

template <>
struct NativeValueTraits<double> {
  static inline double NativeValue(v8::Isolate* isolate,
                                   v8::Local<v8::Value> value,
                                   ExceptionState& exception_state) {
    return ToDouble(isolate, value, exception_state);
  }
};

template <>
struct NativeValueTraits<v8::Local<v8::Value>> {
  static inline v8::Local<v8::Value> NativeValue(
      v8::Isolate* isolate,
      v8::Local<v8::Value> value,
      ExceptionState& exception_state) {
    return value;
  }
};

template <>
struct NativeValueTraits<ScriptValue> {
  static inline ScriptValue NativeValue(v8::Isolate* isolate,
                                        v8::Local<v8::Value> value,
                                        ExceptionState& exception_state) {
    return ScriptValue(ScriptState::Current(isolate), value);
  }
};

template <typename T>
struct NativeValueTraits<Vector<T>> {
  static inline Vector<T> NativeValue(v8::Isolate* isolate,
                                      v8::Local<v8::Value> value,
                                      ExceptionState& exception_state) {
    return ToImplArray<Vector<T>>(value, 0, isolate, exception_state);
  }
};

CORE_EXPORT v8::Isolate* ToIsolate(ExecutionContext*);
CORE_EXPORT v8::Isolate* ToIsolate(LocalFrame*);

CORE_EXPORT DOMWindow* ToDOMWindow(v8::Isolate*, v8::Local<v8::Value>);
LocalDOMWindow* ToLocalDOMWindow(v8::Local<v8::Context>);
LocalDOMWindow* EnteredDOMWindow(v8::Isolate*);
CORE_EXPORT LocalDOMWindow* CurrentDOMWindow(v8::Isolate*);
CORE_EXPORT ExecutionContext* ToExecutionContext(v8::Local<v8::Context>);
CORE_EXPORT void RegisterToExecutionContextForModules(ExecutionContext* (
    *to_execution_context_for_modules)(v8::Local<v8::Context>));
CORE_EXPORT ExecutionContext* CurrentExecutionContext(v8::Isolate*);

// Returns a V8 context associated with a ExecutionContext and a
// DOMWrapperWorld.  This method returns an empty context if there is no frame
// or the frame is already detached.
CORE_EXPORT v8::Local<v8::Context> ToV8Context(ExecutionContext*,
                                               DOMWrapperWorld&);
// Returns a V8 context associated with a Frame and a DOMWrapperWorld.
// This method returns an empty context if the frame is already detached.
CORE_EXPORT v8::Local<v8::Context> ToV8Context(LocalFrame*, DOMWrapperWorld&);
// Like toV8Context but also returns the context if the frame is already
// detached.
CORE_EXPORT v8::Local<v8::Context> ToV8ContextEvenIfDetached(LocalFrame*,
                                                             DOMWrapperWorld&);

// These methods can return nullptr if the context associated with the
// ScriptState has already been detached.
CORE_EXPORT ScriptState* ToScriptState(LocalFrame*, DOMWrapperWorld&);
// Do not use this method unless you are sure you should use the main world's
// ScriptState
CORE_EXPORT ScriptState* ToScriptStateForMainWorld(LocalFrame*);

// Returns the frame object of the window object associated with
// a context, if the window is currently being displayed in a Frame.
CORE_EXPORT LocalFrame* ToLocalFrameIfNotDetached(v8::Local<v8::Context>);

// If 'storage' is non-null, it must be large enough to copy all bytes in the
// array buffer view into it.  Use allocateFlexibleArrayBufferStorage(v8Value)
// to allocate it using alloca() in the callers stack frame.
CORE_EXPORT void ToFlexibleArrayBufferView(v8::Isolate*,
                                           v8::Local<v8::Value>,
                                           FlexibleArrayBufferView&,
                                           void* storage = nullptr);

// Converts a V8 value to an array (an IDL sequence) as per the WebIDL
// specification: http://heycam.github.io/webidl/#es-sequence
template <typename VectorType>
VectorType ToImplSequence(v8::Isolate* isolate,
                          v8::Local<v8::Value> value,
                          ExceptionState& exception_state) {
  using ValueType = typename VectorType::ValueType;

  if (!value->IsObject() || value->IsRegExp()) {
    exception_state.ThrowTypeError(
        "The provided value cannot be converted to a sequence.");
    return VectorType();
  }

  v8::TryCatch block(isolate);
  v8::Local<v8::Object> iterator =
      GetEsIterator(isolate, value.As<v8::Object>(), exception_state);
  if (exception_state.HadException())
    return VectorType();

  v8::Local<v8::String> next_key = V8String(isolate, "next");
  v8::Local<v8::String> value_key = V8String(isolate, "value");
  v8::Local<v8::String> done_key = V8String(isolate, "done");
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  VectorType result;
  while (true) {
    v8::Local<v8::Value> next;
    if (!iterator->Get(context, next_key).ToLocal(&next)) {
      exception_state.RethrowV8Exception(block.Exception());
      return VectorType();
    }
    // TODO(bashi): Support callable objects.
    if (!next->IsObject() || !next.As<v8::Object>()->IsFunction()) {
      exception_state.ThrowTypeError("Iterator.next should be callable.");
      return VectorType();
    }
    v8::Local<v8::Value> next_result;
    if (!V8ScriptRunner::CallFunction(next.As<v8::Function>(),
                                      ToExecutionContext(context), iterator, 0,
                                      nullptr, isolate)
             .ToLocal(&next_result)) {
      exception_state.RethrowV8Exception(block.Exception());
      return VectorType();
    }
    if (!next_result->IsObject()) {
      exception_state.ThrowTypeError(
          "Iterator.next() did not return an object.");
      return VectorType();
    }
    v8::Local<v8::Object> result_object = next_result.As<v8::Object>();
    v8::Local<v8::Value> element;
    v8::Local<v8::Value> done;
    if (!result_object->Get(context, value_key).ToLocal(&element) ||
        !result_object->Get(context, done_key).ToLocal(&done)) {
      exception_state.RethrowV8Exception(block.Exception());
      return VectorType();
    }
    v8::Local<v8::Boolean> done_boolean;
    if (!done->ToBoolean(context).ToLocal(&done_boolean)) {
      exception_state.RethrowV8Exception(block.Exception());
      return VectorType();
    }
    if (done_boolean->Value())
      break;
    result.push_back(NativeValueTraits<ValueType>::NativeValue(
        isolate, element, exception_state));
  }
  return result;
}

// If the current context causes out of memory, JavaScript setting
// is disabled and it returns true.
bool HandleOutOfMemory();

inline bool IsUndefinedOrNull(v8::Local<v8::Value> value) {
  return value.IsEmpty() || value->IsNull() || value->IsUndefined();
}
v8::Local<v8::Function> GetBoundFunction(v8::Local<v8::Function>);

// FIXME: This will be soon embedded in the generated code.
template <typename Collection>
static void IndexedPropertyEnumerator(
    const v8::PropertyCallbackInfo<v8::Array>& info) {
  Collection* collection =
      ToScriptWrappable(info.Holder())->ToImpl<Collection>();
  int length = collection->length();
  v8::Local<v8::Array> properties = v8::Array::New(info.GetIsolate(), length);
  v8::Local<v8::Context> context = info.GetIsolate()->GetCurrentContext();
  for (int i = 0; i < length; ++i) {
    v8::Local<v8::Integer> integer = v8::Integer::New(info.GetIsolate(), i);
    if (!V8CallBoolean(properties->CreateDataProperty(context, i, integer)))
      return;
  }
  V8SetReturnValue(info, properties);
}

CORE_EXPORT bool IsValidEnum(const String& value,
                             const char** valid_values,
                             size_t length,
                             const String& enum_name,
                             ExceptionState&);
CORE_EXPORT bool IsValidEnum(const Vector<String>& values,
                             const char** valid_values,
                             size_t length,
                             const String& enum_name,
                             ExceptionState&);

// These methods store hidden values into an array that is stored in the
// internal field of a DOM wrapper.
bool AddHiddenValueToArray(v8::Isolate*,
                           v8::Local<v8::Object>,
                           v8::Local<v8::Value>,
                           int cache_index);
void RemoveHiddenValueFromArray(v8::Isolate*,
                                v8::Local<v8::Object>,
                                v8::Local<v8::Value>,
                                int cache_index);
CORE_EXPORT void MoveEventListenerToNewWrapper(v8::Isolate*,
                                               v8::Local<v8::Object>,
                                               EventListener* old_value,
                                               v8::Local<v8::Value> new_value,
                                               int cache_index);

// Result values for platform object 'deleter' methods,
// http://www.w3.org/TR/WebIDL/#delete
enum DeleteResult { kDeleteSuccess, kDeleteReject, kDeleteUnknownProperty };

// Freeze a V8 object. The type of the first parameter and the return value is
// intentionally v8::Value so that this function can wrap ToV8().
// If the argument isn't an object, this will crash.
CORE_EXPORT v8::Local<v8::Value> FreezeV8Object(v8::Local<v8::Value>,
                                                v8::Isolate*);

CORE_EXPORT v8::Local<v8::Value> FromJSONString(v8::Isolate*,
                                                const String& stringified_json,
                                                ExceptionState&);

// Ensure that a typed array value is not backed by a SharedArrayBuffer. If it
// is, an exception will be thrown. The return value will use the NotShared
// wrapper type.
template <typename NotSharedType>
NotSharedType ToNotShared(v8::Isolate* isolate,
                          v8::Local<v8::Value> value,
                          ExceptionState& exception_state) {
  using DOMTypedArray = typename NotSharedType::TypedArrayType;
  DOMTypedArray* dom_typed_array =
      V8TypeOf<DOMTypedArray>::Type::toImplWithTypeCheck(isolate, value);
  if (dom_typed_array && dom_typed_array->IsShared()) {
    exception_state.ThrowTypeError(
        "The provided ArrayBufferView value must not be shared.");
    return NotSharedType();
  }
  return NotSharedType(dom_typed_array);
}

}  // namespace blink

#endif  // V8Binding_h
