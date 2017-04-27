// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef StringResource_h
#define StringResource_h

#include "platform/PlatformExport.h"
#include "platform/wtf/Allocator.h"
#include "platform/wtf/Threading.h"
#include "platform/wtf/text/AtomicString.h"
#include "v8/include/v8.h"

namespace blink {

// StringResource is a helper class for V8ExternalString. It is used
// to manage the life-cycle of the underlying buffer of the external string.
class StringResourceBase {
  USING_FAST_MALLOC(StringResourceBase);
  WTF_MAKE_NONCOPYABLE(StringResourceBase);

 public:
  explicit StringResourceBase(const String& string) : plain_string_(string) {
#if DCHECK_IS_ON()
    thread_id_ = WTF::CurrentThread();
#endif
    DCHECK(!string.IsNull());
    v8::Isolate::GetCurrent()->AdjustAmountOfExternalAllocatedMemory(
        string.CharactersSizeInBytes());
  }

  explicit StringResourceBase(const AtomicString& string)
      : plain_string_(string.GetString()), atomic_string_(string) {
#if DCHECK_IS_ON()
    thread_id_ = WTF::CurrentThread();
#endif
    DCHECK(!string.IsNull());
    v8::Isolate::GetCurrent()->AdjustAmountOfExternalAllocatedMemory(
        string.CharactersSizeInBytes());
  }

  virtual ~StringResourceBase() {
#if DCHECK_IS_ON()
    DCHECK(thread_id_ == WTF::CurrentThread());
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
    DCHECK(thread_id_ == WTF::CurrentThread());
#endif
    if (atomic_string_.IsNull()) {
      atomic_string_ = AtomicString(plain_string_);
      DCHECK(!atomic_string_.IsNull());
      if (plain_string_.Impl() != atomic_string_.Impl()) {
        v8::Isolate::GetCurrent()->AdjustAmountOfExternalAllocatedMemory(
            atomic_string_.CharactersSizeInBytes());
      }
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

class StringResource16 final : public StringResourceBase,
                               public v8::String::ExternalStringResource {
  WTF_MAKE_NONCOPYABLE(StringResource16);

 public:
  explicit StringResource16(const String& string) : StringResourceBase(string) {
    DCHECK(!string.Is8Bit());
  }

  explicit StringResource16(const AtomicString& string)
      : StringResourceBase(string) {
    DCHECK(!string.Is8Bit());
  }

  size_t length() const override { return plain_string_.Impl()->length(); }
  const uint16_t* data() const override {
    return reinterpret_cast<const uint16_t*>(
        plain_string_.Impl()->Characters16());
  }
};

class StringResource8 final : public StringResourceBase,
                              public v8::String::ExternalOneByteStringResource {
  WTF_MAKE_NONCOPYABLE(StringResource8);

 public:
  explicit StringResource8(const String& string) : StringResourceBase(string) {
    DCHECK(string.Is8Bit());
  }

  explicit StringResource8(const AtomicString& string)
      : StringResourceBase(string) {
    DCHECK(string.Is8Bit());
  }

  size_t length() const override { return plain_string_.Impl()->length(); }
  const char* data() const override {
    return reinterpret_cast<const char*>(plain_string_.Impl()->Characters8());
  }
};

enum ExternalMode { kExternalize, kDoNotExternalize };

template <typename StringType>
PLATFORM_EXPORT StringType V8StringToWebCoreString(v8::Local<v8::String>,
                                                   ExternalMode);
PLATFORM_EXPORT String Int32ToWebCoreString(int value);

}  // namespace blink

#endif  // StringResource_h
