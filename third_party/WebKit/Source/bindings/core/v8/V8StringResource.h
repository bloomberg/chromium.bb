/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef V8StringResource_h
#define V8StringResource_h

#include "bindings/core/v8/ExceptionState.h"
#include "core/CoreExport.h"
#include "platform/wtf/Allocator.h"
#include "platform/wtf/Threading.h"
#include "platform/wtf/text/AtomicString.h"
#include "v8/include/v8.h"

namespace blink {

// WebCoreStringResource is a helper class for v8ExternalString. It is used
// to manage the life-cycle of the underlying buffer of the external string.
class WebCoreStringResourceBase {
  USING_FAST_MALLOC(WebCoreStringResourceBase);
  WTF_MAKE_NONCOPYABLE(WebCoreStringResourceBase);

 public:
  explicit WebCoreStringResourceBase(const String& string)
      : plain_string_(string) {
#if DCHECK_IS_ON()
    thread_id_ = WTF::CurrentThread();
#endif
    DCHECK(!string.IsNull());
    v8::Isolate::GetCurrent()->AdjustAmountOfExternalAllocatedMemory(
        string.CharactersSizeInBytes());
  }

  explicit WebCoreStringResourceBase(const AtomicString& string)
      : plain_string_(string.GetString()), atomic_string_(string) {
#if DCHECK_IS_ON()
    thread_id_ = WTF::CurrentThread();
#endif
    DCHECK(!string.IsNull());
    v8::Isolate::GetCurrent()->AdjustAmountOfExternalAllocatedMemory(
        string.CharactersSizeInBytes());
  }

  virtual ~WebCoreStringResourceBase() {
#if DCHECK_IS_ON()
    DCHECK_EQ(thread_id_, WTF::CurrentThread());
#endif
    int64_t reduced_external_memory = plain_string_.CharactersSizeInBytes();
    if (plain_string_.Impl() != atomic_string_.Impl() &&
        !atomic_string_.IsNull())
      reduced_external_memory += atomic_string_.CharactersSizeInBytes();
    v8::Isolate::GetCurrent()->AdjustAmountOfExternalAllocatedMemory(
        -reduced_external_memory);
  }

  const String& WebcoreString() { return plain_string_; }

  const AtomicString& GetAtomicString() {
#if DCHECK_IS_ON()
    DCHECK_EQ(thread_id_, WTF::CurrentThread());
#endif
    if (atomic_string_.IsNull()) {
      atomic_string_ = AtomicString(plain_string_);
      DCHECK(!atomic_string_.IsNull());
      if (plain_string_.Impl() != atomic_string_.Impl())
        v8::Isolate::GetCurrent()->AdjustAmountOfExternalAllocatedMemory(
            atomic_string_.CharactersSizeInBytes());
    }
    return atomic_string_;
  }

 protected:
  // A shallow copy of the string. Keeps the string buffer alive until the V8
  // engine garbage collects it.
  String plain_string_;
  // If this string is atomic or has been made atomic earlier the
  // atomic string is held here. In the case where the string starts
  // off non-atomic and becomes atomic later it is necessary to keep
  // the original string alive because v8 may keep derived pointers
  // into that string.
  AtomicString atomic_string_;

 private:
#if DCHECK_IS_ON()
  WTF::ThreadIdentifier thread_id_;
#endif
};

class WebCoreStringResource16 final
    : public WebCoreStringResourceBase,
      public v8::String::ExternalStringResource {
  WTF_MAKE_NONCOPYABLE(WebCoreStringResource16);

 public:
  explicit WebCoreStringResource16(const String& string)
      : WebCoreStringResourceBase(string) {
    DCHECK(!string.Is8Bit());
  }

  explicit WebCoreStringResource16(const AtomicString& string)
      : WebCoreStringResourceBase(string) {
    DCHECK(!string.Is8Bit());
  }

  size_t length() const override { return plain_string_.Impl()->length(); }
  const uint16_t* data() const override {
    return reinterpret_cast<const uint16_t*>(
        plain_string_.Impl()->Characters16());
  }
};

class WebCoreStringResource8 final
    : public WebCoreStringResourceBase,
      public v8::String::ExternalOneByteStringResource {
  WTF_MAKE_NONCOPYABLE(WebCoreStringResource8);

 public:
  explicit WebCoreStringResource8(const String& string)
      : WebCoreStringResourceBase(string) {
    DCHECK(string.Is8Bit());
  }

  explicit WebCoreStringResource8(const AtomicString& string)
      : WebCoreStringResourceBase(string) {
    DCHECK(string.Is8Bit());
  }

  size_t length() const override { return plain_string_.Impl()->length(); }
  const char* data() const override {
    return reinterpret_cast<const char*>(plain_string_.Impl()->Characters8());
  }
};

enum ExternalMode { kExternalize, kDoNotExternalize };

template <typename StringType>
CORE_EXPORT StringType V8StringToWebCoreString(v8::Local<v8::String>,
                                               ExternalMode);
CORE_EXPORT String Int32ToWebCoreString(int value);

// V8StringResource is an adapter class that converts V8 values to Strings
// or AtomicStrings as appropriate, using multiple typecast operators.
enum V8StringResourceMode {
  kDefaultMode,
  kTreatNullAsEmptyString,
  kTreatNullAsNullString,
  kTreatNullAndUndefinedAsNullString
};

template <V8StringResourceMode Mode = kDefaultMode>
class V8StringResource {
  STACK_ALLOCATED();

 public:
  V8StringResource() : mode_(kExternalize) {}

  V8StringResource(v8::Local<v8::Value> object)
      : v8_object_(object), mode_(kExternalize) {}

  V8StringResource(const String& string)
      : mode_(kExternalize), string_(string) {}

  void operator=(v8::Local<v8::Value> object) { v8_object_ = object; }

  void operator=(const String& string) { SetString(string); }

  void operator=(std::nullptr_t) { SetString(String()); }

  bool Prepare() {  // DEPRECATED
    if (PrepareFast())
      return true;

    return v8_object_->ToString(v8::Isolate::GetCurrent()->GetCurrentContext())
        .ToLocal(&v8_object_);
  }

  bool Prepare(v8::Isolate* isolate, ExceptionState& exception_state) {
    return PrepareFast() || PrepareSlow(isolate, exception_state);
  }

  bool Prepare(ExceptionState& exception_state) {  // DEPRECATED
    return PrepareFast() ||
           PrepareSlow(v8::Isolate::GetCurrent(), exception_state);
  }

  operator String() const { return ToString<String>(); }
  operator AtomicString() const { return ToString<AtomicString>(); }

 private:
  bool PrepareFast() {
    if (v8_object_.IsEmpty())
      return true;

    if (!IsValid()) {
      SetString(FallbackString());
      return true;
    }

    if (LIKELY(v8_object_->IsString()))
      return true;

    if (LIKELY(v8_object_->IsInt32())) {
      SetString(Int32ToWebCoreString(v8_object_.As<v8::Int32>()->Value()));
      return true;
    }

    mode_ = kDoNotExternalize;
    return false;
  }

  bool PrepareSlow(v8::Isolate* isolate, ExceptionState& exception_state) {
    v8::TryCatch try_catch(isolate);
    if (!v8_object_->ToString(isolate->GetCurrentContext())
             .ToLocal(&v8_object_)) {
      exception_state.RethrowV8Exception(try_catch.Exception());
      return false;
    }
    return true;
  }

  bool IsValid() const;
  String FallbackString() const;

  void SetString(const String& string) {
    string_ = string;
    v8_object_.Clear();  // To signal that String is ready.
  }

  template <class StringType>
  StringType ToString() const {
    if (LIKELY(!v8_object_.IsEmpty()))
      return V8StringToWebCoreString<StringType>(
          const_cast<v8::Local<v8::Value>*>(&v8_object_)->As<v8::String>(),
          mode_);

    return StringType(string_);
  }

  v8::Local<v8::Value> v8_object_;
  ExternalMode mode_;
  String string_;
};

template <>
inline bool V8StringResource<kDefaultMode>::IsValid() const {
  return true;
}

template <>
inline String V8StringResource<kDefaultMode>::FallbackString() const {
  NOTREACHED();
  return String();
}

template <>
inline bool V8StringResource<kTreatNullAsEmptyString>::IsValid() const {
  return !v8_object_->IsNull();
}

template <>
inline String V8StringResource<kTreatNullAsEmptyString>::FallbackString()
    const {
  return g_empty_string;
}

template <>
inline bool V8StringResource<kTreatNullAsNullString>::IsValid() const {
  return !v8_object_->IsNull();
}

template <>
inline String V8StringResource<kTreatNullAsNullString>::FallbackString() const {
  return String();
}

template <>
inline bool V8StringResource<kTreatNullAndUndefinedAsNullString>::IsValid()
    const {
  return !v8_object_->IsNull() && !v8_object_->IsUndefined();
}

template <>
inline String
V8StringResource<kTreatNullAndUndefinedAsNullString>::FallbackString() const {
  return String();
}

}  // namespace blink

#endif  // V8StringResource_h
