// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_BINDINGS_PARKABLE_STRING_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_BINDINGS_PARKABLE_STRING_H_

#include <map>
#include <set>
#include <utility>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/ref_counted.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

// ParkableString represents a string that may be parked in memory, that it its
// underlying memory address may change. Its content can be retrieved with the
// |ToString()| method.
// As a consequence, the inner pointer should never be cached, and only touched
// through a string returned by the |ToString()| method.
//
// As with WTF::AtomicString, this class is *not* thread-safe, and strings
// created on a thread must always be used on the same thread.
//
// Implementation note: the string content is not parked yet, it is merely
// used to gather statistics.

namespace blink {

class PLATFORM_EXPORT ParkableStringImpl final
    : public RefCounted<ParkableStringImpl> {
 public:
  // Histogram buckets, exported for testing.
  enum class ParkingAction {
    kParkedInBackground = 0,
    kUnparkedInBackground = 1,
    kUnparkedInForeground = 2,
    kMaxValue = kUnparkedInForeground
  };

  ParkableStringImpl();
  explicit ParkableStringImpl(scoped_refptr<StringImpl>&& impl);
  ~ParkableStringImpl();

  // The returned string may be used as a normal one, as long as the
  // returned value (or a copy of it) is alive.
  const String& ToString();

  // See the matching String methods.
  bool Is8Bit() const;
  bool IsNull() const;
  unsigned length() const { return string_.length(); }
  unsigned CharactersSizeInBytes() const;

  // A parked string cannot be accessed until it has been |Unpark()|-ed.
  // Returns true iff the string has been parked.
  bool Park();
  // Returns true if the string is parked.
  bool is_parked() const { return is_parked_; }

 private:
  void Unpark();

  String string_;
  bool is_parked_;

  DISALLOW_COPY_AND_ASSIGN(ParkableStringImpl);
};

class PLATFORM_EXPORT ParkableString final {
 public:
  ParkableString() : impl_(base::MakeRefCounted<ParkableStringImpl>(nullptr)) {}
  explicit ParkableString(scoped_refptr<StringImpl>&& impl);
  ParkableString(const ParkableString& rhs) : impl_(rhs.impl_) {}
  ~ParkableString();

  // See the matching String methods.
  bool Is8Bit() const;
  bool IsNull() const;
  unsigned length() const { return impl_->length(); }
  ParkableStringImpl* Impl() const { return impl_.get(); }
  // Returns an unparked version of the string.
  // The string is guaranteed to be valid for
  // max(lifetime of a copy of the returned reference, current thread task).
  const String& ToString() const;
  unsigned CharactersSizeInBytes() const;

  // Causes the string to be unparked. Note that the pointer must not be
  // cached.
  const LChar* Characters8() const { return ToString().Characters8(); }
  const UChar* Characters16() const { return ToString().Characters16(); }

 private:
  scoped_refptr<ParkableStringImpl> impl_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_BINDINGS_PARKABLE_STRING_H_
