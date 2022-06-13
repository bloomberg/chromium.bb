// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_BINDINGS_STRING_RESOURCE_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_BINDINGS_STRING_RESOURCE_H_

#include "base/dcheck_is_on.h"
#include "third_party/blink/renderer/platform/bindings/parkable_string.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "third_party/blink/renderer/platform/wtf/threading.h"
#include "v8/include/v8.h"

namespace blink {

// StringResource is a helper class for V8ExternalString. It is used
// to manage the life-cycle of the underlying buffer of the external string.
class StringResourceBase {
  USING_FAST_MALLOC(StringResourceBase);

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
      : atomic_string_(string) {
#if DCHECK_IS_ON()
    thread_id_ = WTF::CurrentThread();
#endif
    DCHECK(!string.IsNull());
    v8::Isolate::GetCurrent()->AdjustAmountOfExternalAllocatedMemory(
        string.CharactersSizeInBytes());
  }

  explicit StringResourceBase(const ParkableString& string)
      : parkable_string_(string) {
#if DCHECK_IS_ON()
    thread_id_ = WTF::CurrentThread();
#endif
    // TODO(lizeb): This is only true without compression.
    DCHECK(!string.IsNull());
    v8::Isolate::GetCurrent()->AdjustAmountOfExternalAllocatedMemory(
        string.CharactersSizeInBytes());
  }

  StringResourceBase(const StringResourceBase&) = delete;
  StringResourceBase& operator=(const StringResourceBase&) = delete;

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

  String GetWTFString() {
    if (!parkable_string_.IsNull()) {
      DCHECK(plain_string_.IsNull());
      DCHECK(atomic_string_.IsNull());
      return parkable_string_.ToString();
    }
    return String(GetStringImpl());
  }

  AtomicString GetAtomicString() {
#if DCHECK_IS_ON()
    DCHECK(thread_id_ == WTF::CurrentThread());
#endif
    if (!parkable_string_.IsNull()) {
      DCHECK(plain_string_.IsNull());
      DCHECK(atomic_string_.IsNull());
      return AtomicString(parkable_string_.ToString());
    }
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
  StringImpl* GetStringImpl() const {
    if (!plain_string_.IsNull())
      return plain_string_.Impl();
    DCHECK(!atomic_string_.IsNull());
    return atomic_string_.Impl();
  }

  const ParkableString& GetParkableString() const { return parkable_string_; }

 private:
  // If this StringResourceBase was initialized from a String then plain_string_
  // will be non-null. If the string becomes atomic later, the atomic version
  // of the string will be held in atomic_string_. When that happens, it is
  // necessary to keep the original string alive because v8 may keep derived
  // pointers into that string.
  // If this StringResourceBase was initialized from an AtomicString then
  // plain_string_ will be null and atomic_string_ will be non-null.
  String plain_string_;
  AtomicString atomic_string_;

  // If this string is parkable, its value is held here, and the other
  // members above are null.
  ParkableString parkable_string_;

#if DCHECK_IS_ON()
  base::PlatformThreadId thread_id_;
#endif
};

// Even though StringResource{8,16}Base are effectively empty in release mode,
// they are needed as they serve as a common ancestor to Parkable and regular
// strings.
//
// See the comment in |ToBlinkString()|'s implementation for the rationale.
class StringResource16Base : public StringResourceBase,
                             public v8::String::ExternalStringResource {
 public:
  explicit StringResource16Base(const String& string)
      : StringResourceBase(string) {
    DCHECK(!string.Is8Bit());
  }

  explicit StringResource16Base(const AtomicString& string)
      : StringResourceBase(string) {
    DCHECK(!string.Is8Bit());
  }

  explicit StringResource16Base(const ParkableString& parkable_string)
      : StringResourceBase(parkable_string) {
    DCHECK(!parkable_string.Is8Bit());
  }

  StringResource16Base(const StringResource16Base&) = delete;
  StringResource16Base& operator=(const StringResource16Base&) = delete;
};

class StringResource16 final : public StringResource16Base {
 public:
  explicit StringResource16(const String& string)
      : StringResource16Base(string) {}

  explicit StringResource16(const AtomicString& string)
      : StringResource16Base(string) {}

  StringResource16(const StringResource16&) = delete;
  StringResource16& operator=(const StringResource16&) = delete;

  size_t length() const override { return GetStringImpl()->length(); }
  const uint16_t* data() const override {
    return reinterpret_cast<const uint16_t*>(GetStringImpl()->Characters16());
  }
};

class ParkableStringResource16 final : public StringResource16Base {
 public:
  explicit ParkableStringResource16(const ParkableString& string)
      : StringResource16Base(string) {}

  ParkableStringResource16(const ParkableStringResource16&) = delete;
  ParkableStringResource16& operator=(const ParkableStringResource16&) = delete;

  bool IsCacheable() const override {
    return !GetParkableString().may_be_parked();
  }

  void Lock() const override { GetParkableString().Lock(); }

  void Unlock() const override { GetParkableString().Unlock(); }

  size_t length() const override { return GetParkableString().length(); }

  const uint16_t* data() const override {
    return reinterpret_cast<const uint16_t*>(
        GetParkableString().Characters16());
  }
};

class StringResource8Base : public StringResourceBase,
                            public v8::String::ExternalOneByteStringResource {
 public:
  explicit StringResource8Base(const String& string)
      : StringResourceBase(string) {
    DCHECK(string.Is8Bit());
  }

  explicit StringResource8Base(const AtomicString& string)
      : StringResourceBase(string) {
    DCHECK(string.Is8Bit());
  }

  explicit StringResource8Base(const ParkableString& parkable_string)
      : StringResourceBase(parkable_string) {
    DCHECK(parkable_string.Is8Bit());
  }

  StringResource8Base(const StringResource8Base&) = delete;
  StringResource8Base& operator=(const StringResource8Base&) = delete;
};

class StringResource8 final : public StringResource8Base {
 public:
  explicit StringResource8(const String& string)
      : StringResource8Base(string) {}

  explicit StringResource8(const AtomicString& string)
      : StringResource8Base(string) {}

  StringResource8(const StringResource8&) = delete;
  StringResource8& operator=(const StringResource8&) = delete;

  size_t length() const override { return GetStringImpl()->length(); }
  const char* data() const override {
    return reinterpret_cast<const char*>(GetStringImpl()->Characters8());
  }
};

class ParkableStringResource8 final : public StringResource8Base {
 public:
  explicit ParkableStringResource8(const ParkableString& string)
      : StringResource8Base(string) {}

  ParkableStringResource8(const ParkableStringResource8&) = delete;
  ParkableStringResource8& operator=(const ParkableStringResource8&) = delete;

  bool IsCacheable() const override {
    return !GetParkableString().may_be_parked();
  }

  void Lock() const override { GetParkableString().Lock(); }

  void Unlock() const override { GetParkableString().Unlock(); }

  size_t length() const override { return GetParkableString().length(); }

  const char* data() const override {
    return reinterpret_cast<const char*>(GetParkableString().Characters8());
  }
};

enum ExternalMode { kExternalize, kDoNotExternalize };

template <typename StringType>
PLATFORM_EXPORT StringType ToBlinkString(v8::Local<v8::String>, ExternalMode);

// This method is similar to ToBlinkString() except when the underlying
// v8::String cannot be externalized (often happens with short strings like "id"
// on 64-bit platforms where V8 uses pointer compression) the v8::String is
// copied into the given StringView::StackBackingStore which avoids creating an
// AtomicString unnecessarily.
PLATFORM_EXPORT StringView ToBlinkStringView(v8::Local<v8::String>,
                                             StringView::StackBackingStore&,
                                             ExternalMode);

PLATFORM_EXPORT String ToBlinkString(int value);

// The returned StringView is guaranteed to be valid as long as `backing_store`
// and `v8_string` are alive.
PLATFORM_EXPORT StringView
ToBlinkStringView(v8::Local<v8::String> v8_string,
                  StringView::StackBackingStore& backing_store,
                  ExternalMode external);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_BINDINGS_STRING_RESOURCE_H_
