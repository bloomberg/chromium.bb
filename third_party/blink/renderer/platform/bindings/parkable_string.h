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
#include "third_party/blink/renderer/platform/wtf/threading_primitives.h"

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

// A parked string is parked by calling |Park()|, and unparked by calling
// |ToString()| on a parked string.
// |Lock()| does *not* unpark a string, and |ToString()| must be called on
// a single thread, the one on which the string was created. Only |Lock()|
// and |Unlock()| can be called from any thread.
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

  enum class ParkableState { kParkable, kNotParkable };

  ParkableStringImpl();
  // Not all parkable strings can actually be parked. If |parkable| is
  // kNotParkable, then one cannot call |Park()|, and the underlying StringImpl
  // will not move.
  ParkableStringImpl(scoped_refptr<StringImpl>&& impl, ParkableState parkable);
  ~ParkableStringImpl();

  void Lock();
  void Unlock();

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
  // Returns true iff the string can be parked.
  bool is_parkable() const { return is_parkable_; }

 private:
  void Unpark();
  // Returns true if the string is parked.
  bool is_parked() const { return is_parked_; }

  Mutex mutex_;  // protects the variables below.
  int lock_depth_;
  String string_;
  bool is_parked_;

  const bool is_parkable_;

  FRIEND_TEST_ALL_PREFIXES(ParkableStringTest, Park);
  FRIEND_TEST_ALL_PREFIXES(ParkableStringTest, Unpark);
  FRIEND_TEST_ALL_PREFIXES(ParkableStringTest, LockUnlock);
  FRIEND_TEST_ALL_PREFIXES(ParkableStringTest, LockParkedString);
  FRIEND_TEST_ALL_PREFIXES(ParkableStringTest, TableSimple);
  FRIEND_TEST_ALL_PREFIXES(ParkableStringTest, TableMultiple);
  DISALLOW_COPY_AND_ASSIGN(ParkableStringImpl);
};

class PLATFORM_EXPORT ParkableString final {
 public:
  ParkableString() : impl_(base::MakeRefCounted<ParkableStringImpl>()) {}
  explicit ParkableString(scoped_refptr<StringImpl>&& impl);
  ParkableString(const ParkableString& rhs) : impl_(rhs.impl_) {}
  ~ParkableString();

  // Locks a string. A string is unlocked when the number of Lock()/Unlock()
  // calls match. A locked string cannot be parked.
  // Can be called from any thread.
  void Lock() const;

  // Unlocks a string.
  // Can be called from any thread.
  void Unlock() const;

  // See the matching String methods.
  bool Is8Bit() const;
  bool IsNull() const;
  unsigned length() const { return impl_->length(); }
  bool is_parkable() const { return impl_ && impl_->is_parkable(); }

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
