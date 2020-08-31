// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_BINDINGS_PARKABLE_STRING_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_BINDINGS_PARKABLE_STRING_H_

#include <memory>
#include <utility>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/thread_annotations.h"
#include "third_party/blink/renderer/platform/disk_data_allocator.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/ref_counted.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/threading.h"
#include "third_party/blink/renderer/platform/wtf/threading_primitives.h"

// ParkableString represents a string that may be parked in memory, that it its
// underlying memory address may change. Its content can be retrieved with the
// |ToString()| method.
// As a consequence, the inner pointer should never be cached, and only touched
// through a string returned by the |ToString()| method.
//
// As with WTF::AtomicString, this class is *not* thread-safe, and strings
// created on a thread must always be used on the same thread.

namespace blink {

class WebProcessMemoryDump;
struct BackgroundTaskParams;

// A parked string is parked by calling |Park()|, and unparked by calling
// |ToString()| on a parked string.
// |Lock()| does *not* unpark a string, and |ToString()| must be called on
// a single thread, the one on which the string was created. Only |Lock()|
// and |Unlock()| can be called from any thread.
class PLATFORM_EXPORT ParkableStringImpl final
    : public RefCounted<ParkableStringImpl> {
 public:
  enum class ParkingMode { kSynchronousOnly, kCompress, kToDisk };
  enum class AgeOrParkResult {
    kSuccessOrTransientFailure,
    kNonTransientFailure
  };
  enum class Age { kYoung = 0, kOld = 1, kVeryOld = 2 };

  constexpr static size_t kDigestSize = 32;  // SHA256.
  using SecureDigest = Vector<uint8_t, kDigestSize>;
  // Computes a secure hash of a |string|, to be passed to |MakeParkable()|.
  static std::unique_ptr<SecureDigest> HashString(StringImpl* string);

  // Not all ParkableStringImpls are actually parkable.
  static scoped_refptr<ParkableStringImpl> MakeNonParkable(
      scoped_refptr<StringImpl>&& impl);
  // |digest| is as returned by |HashString()|, hence not nullptr.
  static scoped_refptr<ParkableStringImpl> MakeParkable(
      scoped_refptr<StringImpl>&& impl,
      std::unique_ptr<SecureDigest> digest);

  ~ParkableStringImpl();

  void Lock();
  void Unlock();

  // The returned string may be used as a normal one, as long as the
  // returned value (or a copy of it) is alive.
  const String& ToString();

  // See the matching String methods.
  bool is_8bit() const {
    if (!may_be_parked())
      return string_.Is8Bit();

    return metadata_->is_8bit_;
  }
  unsigned length() const {
    if (!may_be_parked())
      return string_.length();

    return metadata_->length_;
  }
  unsigned CharactersSizeInBytes() const;
  size_t MemoryFootprintForDump() const;

  // Returns true iff the string can be parked. This does not mean that the
  // string can be parked now, merely that it is eligible to be parked at some
  // point.
  bool may_be_parked() const { return !!metadata_; }

  // Note: Public member functions below must only be called on strings for
  // which |may_be_parked()| returns true. Otherwise, these will either trigger
  // a DCHECK() or crash.

  // Tries to either age or park a string:
  //
  // - If the string is already old, tries to park it.
  // - If it is very old and parked, tries to write it to disk.
  // - Otherwise, tries to age it.
  //
  // The action doesn't necessarily succeed. either due to a temporary
  // or potentially lasting condition.
  //
  // As parking may be synchronous, this can call back into
  // ParkableStringManager.
  AgeOrParkResult MaybeAgeOrParkString();

  // A parked string cannot be accessed until it has been |Unpark()|-ed.
  //
  // Parking may be synchronous, and will be if compressed data is already
  // available. If |mode| is |kIfCompressedDataExists|, then parking will always
  // be synchronous.
  //
  // Must not be called if |may_be_parked()| returns false.
  //
  // Returns true if the string is being parked or has been parked.
  bool Park(ParkingMode mode);

  // Returns true if the string is parked.
  bool is_parked() const;
  bool is_on_disk() const;
  // Returns whether synchronous parking is possible, that is the string was
  // parked in the past.
  bool has_compressed_data() const { return !!metadata_->compressed_; }
  bool has_on_disk_data() const { return !!metadata_->on_disk_metadata_; }

  // Returns the compressed size, must not be called unless the string has a
  // compressed representation.
  size_t compressed_size() const {
    DCHECK(has_compressed_data());
    return metadata_->compressed_->size();
  }

  // Returns the on-disk size, must not be called unless the string has data
  // on-disk.
  size_t on_disk_size() const {
    DCHECK(has_on_disk_data());
    return metadata_->on_disk_metadata_->size();
  }

  Age age_for_testing() {
    MutexLocker locker(metadata_->mutex_);
    return metadata_->age_;
  }

  bool background_task_in_progress_for_testing() const {
    return metadata_->background_task_in_progress_;
  }

  const SecureDigest* digest() const {
    AssertOnValidThread();
    DCHECK(metadata_);
    return &metadata_->digest_;
  }

 private:
  enum class State : uint8_t;
  enum class Status : uint8_t;
  friend class ParkableStringManager;

  // |digest| is as returned by calling HashString() on |impl|, or nullptr for
  // a non-parkable instance.
  ParkableStringImpl(scoped_refptr<StringImpl>&& impl,
                     std::unique_ptr<SecureDigest> digest);

  // Note: Private member  functions below must only be called on strings for
  // which |may_be_parked()| returns true. Otherwise, these will either trigger
  // a DCHECK() or crash.

  // Doesn't make the string young. May be called from any thread.
  void LockWithoutMakingYoung() {
    MutexLocker locker(metadata_->mutex_);
    metadata_->lock_depth_ += 1;
  }

  void MakeYoung() EXCLUSIVE_LOCKS_REQUIRED(metadata_->mutex_) {
    metadata_->age_ = Age::kYoung;
  }
  // Whether the string is referenced or locked. The return value is valid as
  // long as |mutex_| is held.
  Status CurrentStatus() const EXCLUSIVE_LOCKS_REQUIRED(metadata_->mutex_);
  bool CanParkNow() const EXCLUSIVE_LOCKS_REQUIRED(metadata_->mutex_);
  bool ParkInternal(ParkingMode mode)
      EXCLUSIVE_LOCKS_REQUIRED(metadata_->mutex_);
  void Unpark() EXCLUSIVE_LOCKS_REQUIRED(metadata_->mutex_);
  String UnparkInternal() EXCLUSIVE_LOCKS_REQUIRED(metadata_->mutex_);

  void PostBackgroundCompressionTask();
  static void CompressInBackground(std::unique_ptr<BackgroundTaskParams>);
  // Called on the main thread after compression is done.
  // |params| is the same as the one passed to
  // |PostBackgroundCompressionTask()|,
  // |compressed| is the compressed data, nullptr if compression failed.
  // |parking_thread_time| is the CPU time used by the background compression
  // task.
  void OnParkingCompleteOnMainThread(
      std::unique_ptr<BackgroundTaskParams> params,
      std::unique_ptr<Vector<uint8_t>> compressed,
      base::TimeDelta parking_thread_time);

  void PostBackgroundWritingTask() EXCLUSIVE_LOCKS_REQUIRED(metadata_->mutex_);
  static void WriteToDiskInBackground(std::unique_ptr<BackgroundTaskParams>);
  // Called on the main thread after writing is done.
  // |params| is the same as the one passed to PostBackgroundWritingTask()|,
  // |metadata| is the on-disk metadata, nullptr if writing failed.
  void OnWritingCompleteOnMainThread(
      std::unique_ptr<BackgroundTaskParams> params,
      std::unique_ptr<DiskDataAllocator::Metadata> metadata);

  void DiscardUncompressedData();
  void DiscardCompressedData();

  int lock_depth_for_testing() {
    MutexLocker locker_(metadata_->mutex_);
    return metadata_->lock_depth_;
  }

  // Metadata only used for parkable ParkableStrings.
  struct ParkableMetadata {
    ParkableMetadata(String string, std::unique_ptr<SecureDigest> digest);

    Mutex mutex_;
    int lock_depth_ GUARDED_BY(mutex_);

    // Main thread only.
    State state_;
    bool background_task_in_progress_;
    std::unique_ptr<Vector<uint8_t>> compressed_;
    std::unique_ptr<DiskDataAllocator::Metadata> on_disk_metadata_;
    const SecureDigest digest_;

    // A string can be young, old or very old. It starts young, and ages with
    // |MaybeAgeOrParkString()|.
    //
    // Transitions are:
    // Young -> Old -> Very old: By calling |MaybeAgeOrParkString()|.
    // (Old | Very Old) -> Young: When the string is accessed, either by
    //                            |Lock()|-ing it or calling |ToString()|.
    //
    // Thread safety: it is typically not safe to guard only one part of a
    // bitfield with a mutex, but this is correct here, as the other members are
    // const (and never change).
    Age age_ : 3 GUARDED_BY(mutex_);
    const bool is_8bit_ : 1;
    const unsigned length_;

    DISALLOW_COPY_AND_ASSIGN(ParkableMetadata);
  };

  String string_;
  const std::unique_ptr<ParkableMetadata> metadata_;

#if DCHECK_IS_ON()
  const base::PlatformThreadId owning_thread_;
#endif

  void AssertOnValidThread() const {
#if DCHECK_IS_ON()
    DCHECK_EQ(owning_thread_, CurrentThread());
#endif
  }

 public:
  FRIEND_TEST_ALL_PREFIXES(ParkableStringTest, Equality);
  FRIEND_TEST_ALL_PREFIXES(ParkableStringTest, EqualityNoUnparking);
  FRIEND_TEST_ALL_PREFIXES(ParkableStringTest, LockUnlock);
  FRIEND_TEST_ALL_PREFIXES(ParkableStringTest, LockParkedString);
  FRIEND_TEST_ALL_PREFIXES(ParkableStringTest, ReportMemoryDump);
  FRIEND_TEST_ALL_PREFIXES(ParkableStringTest, MemoryFootprintForDump);

  DISALLOW_COPY_AND_ASSIGN(ParkableStringImpl);
};

#if !DCHECK_IS_ON()
// 3 pointers:
// - vtable (from RefCounted)
// - string_.Impl()
// - metadata_
static_assert(sizeof(ParkableStringImpl) == 3 * sizeof(void*),
              "ParkableStringImpl should not be too large");
#endif

class PLATFORM_EXPORT ParkableString final {
  DISALLOW_NEW();

 public:
  ParkableString() : impl_(nullptr) {}
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

  void OnMemoryDump(WebProcessMemoryDump* pmd, const String& name) const;

  // See the matching String methods.
  bool Is8Bit() const;
  bool IsNull() const { return !impl_; }
  unsigned length() const { return impl_ ? impl_->length() : 0; }
  bool may_be_parked() const { return impl_ && impl_->may_be_parked(); }

  ParkableStringImpl* Impl() const { return impl_ ? impl_.get() : nullptr; }
  // Returns an unparked version of the string.
  // The string is guaranteed to be valid for
  // max(lifetime of a copy of the returned reference, current thread task).
  const String& ToString() const;
  wtf_size_t CharactersSizeInBytes() const;

  // Causes the string to be unparked. Note that the pointer must not be
  // cached.
  const LChar* Characters8() const { return ToString().Characters8(); }
  const UChar* Characters16() const { return ToString().Characters16(); }

 private:
  scoped_refptr<ParkableStringImpl> impl_;
};

static_assert(sizeof(ParkableString) == sizeof(void*),
              "ParkableString should be small");

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_BINDINGS_PARKABLE_STRING_H_
