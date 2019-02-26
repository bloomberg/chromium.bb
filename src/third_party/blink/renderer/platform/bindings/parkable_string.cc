// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/bindings/parkable_string.h"

#include <string>

#include "base/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/single_thread_task_runner.h"
#include "base/timer/elapsed_timer.h"
#include "base/trace_event/trace_event.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/platform/bindings/parkable_string_manager.h"
#include "third_party/blink/renderer/platform/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/scheduler/public/background_scheduler.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"
#include "third_party/blink/renderer/platform/wtf/address_sanitizer.h"
#include "third_party/blink/renderer/platform/wtf/thread_specific.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "third_party/zlib/google/compression_utils.h"

namespace blink {

namespace {

void RecordParkingAction(ParkableStringImpl::ParkingAction action) {
  UMA_HISTOGRAM_ENUMERATION("Memory.MovableStringParkingAction", action);
}

void RecordStatistics(size_t size,
                      base::TimeDelta duration,
                      ParkableStringImpl::ParkingAction action) {
  size_t throughput_mb_s =
      static_cast<size_t>(size / duration.InSecondsF()) / 1000000;
  size_t size_kb = size / 1000;
  if (action == ParkableStringImpl::ParkingAction::kParkedInBackground) {
    UMA_HISTOGRAM_COUNTS_10000("Memory.ParkableString.Compression.SizeKb",
                               size_kb);
    // Size is at least 10kB, and at most ~1MB, and compression throughput
    // ranges from single-digit MB/s to ~40MB/s depending on the CPU, hence
    // the range.
    UMA_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES(
        "Memory.ParkableString.Compression.Latency", duration,
        base::TimeDelta::FromMicroseconds(500), base::TimeDelta::FromSeconds(1),
        100);
    UMA_HISTOGRAM_COUNTS_1000(
        "Memory.ParkableString.Compression.ThroughputMBps", throughput_mb_s);
  } else {
    UMA_HISTOGRAM_COUNTS_10000("Memory.ParkableString.Decompression.SizeKb",
                               size_kb);
    UMA_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES(
        "Memory.ParkableString.Decompression.Latency", duration,
        base::TimeDelta::FromMicroseconds(500), base::TimeDelta::FromSeconds(1),
        100);
    // Decompression speed can go up to >500MB/s.
    UMA_HISTOGRAM_COUNTS_1000(
        "Memory.ParkableString.Decompression.ThroughputMBps", throughput_mb_s);
  }
}

void AsanPoisonString(const String& string) {
#if defined(ADDRESS_SANITIZER)
  if (string.IsNull())
    return;
  // Since |string| is not deallocated, it remains in the per-thread
  // AtomicStringTable, where its content can be accessed for equality
  // comparison for instance, triggering a poisoned memory access.
  // See crbug.com/883344 for an example.
  if (string.Impl()->IsAtomic())
    return;

  ASAN_POISON_MEMORY_REGION(string.Bytes(), string.CharactersSizeInBytes());
#endif  // defined(ADDRESS_SANITIZER)
}

void AsanUnpoisonString(const String& string) {
#if defined(ADDRESS_SANITIZER)
  if (string.IsNull())
    return;

  ASAN_UNPOISON_MEMORY_REGION(string.Bytes(), string.CharactersSizeInBytes());
#endif  // defined(ADDRESS_SANITIZER)
}

}  // namespace

// Created and destroyed on the same thread, accessed on a background thread as
// well. |string|'s reference counting is *not* thread-safe, hence |string|'s
// reference count must *not* change on the background thread.
struct CompressionTaskParams final {
  CompressionTaskParams(
      scoped_refptr<ParkableStringImpl> string,
      const void* data,
      size_t size,
      scoped_refptr<base::SingleThreadTaskRunner> callback_task_runner)
      : string(string),
        data(data),
        size(size),
        callback_task_runner(std::move(callback_task_runner)) {
    DCHECK(IsMainThread());
  }

  ~CompressionTaskParams() { DCHECK(IsMainThread()); }

  const scoped_refptr<ParkableStringImpl> string;
  const void* data;
  const size_t size;
  const scoped_refptr<base::SingleThreadTaskRunner> callback_task_runner;

  CompressionTaskParams(CompressionTaskParams&&) = delete;
  DISALLOW_COPY_AND_ASSIGN(CompressionTaskParams);
};

// Valid transitions are:
// 1. kUnparked -> kParkingInProgress: Parking started asynchronously
// 2. kParkingInProgress -> kUnparked: Parking did not complete
// 3. kParkingInProgress -> kParked: Parking completed normally
// 4. kParked -> kUnparked: String has been unparked.
//
// See |Park()| for (1), |OnParkingCompleteOnMainThread()| for 2-3, and
// |Unpark()| for (4).
enum class ParkableStringImpl::State { kUnparked, kParkingInProgress, kParked };

ParkableStringImpl::ParkableStringImpl(scoped_refptr<StringImpl>&& impl,
                                       ParkableState parkable)
    : mutex_(),
      lock_depth_(0),
      state_(State::kUnparked),
      string_(std::move(impl)),
      compressed_(nullptr),
      may_be_parked_(parkable == ParkableState::kParkable),
      is_8bit_(string_.Is8Bit()),
      length_(string_.length())
#if DCHECK_IS_ON()
      ,
      owning_thread_(CurrentThread())
#endif
{
  DCHECK(!string_.IsNull());
}

ParkableStringImpl::~ParkableStringImpl() {
  AssertOnValidThread();
#if DCHECK_IS_ON()
  {
    MutexLocker locker(mutex_);
    DCHECK_EQ(0, lock_depth_);
  }
#endif
  AsanUnpoisonString(string_);
  DCHECK(state_ == State::kParked || state_ == State::kUnparked);

  if (may_be_parked_)
    ParkableStringManager::Instance().Remove(this, string_.Impl());
}

void ParkableStringImpl::Lock() {
  MutexLocker locker(mutex_);
  lock_depth_ += 1;
}

void ParkableStringImpl::Unlock() {
  MutexLocker locker(mutex_);
  DCHECK_GT(lock_depth_, 0);
  lock_depth_ -= 1;

#if defined(ADDRESS_SANITIZER) && DCHECK_IS_ON()
  // There are no external references to the data, nobody should touch the data.
  //
  // Note: Only poison the memory if this is on the owning thread, as this is
  // otherwise racy. Indeed |Unlock()| may be called on any thread, and
  // the owning thread may concurrently call |ToString()|. It is then allowed
  // to use the string until the end of the current owning thread task.
  // Requires DCHECK_IS_ON() for the |owning_thread_| check.
  //
  // Checking the owning thread first as CanParkNow() can only be called from
  // the owning thread.
  if (owning_thread_ == CurrentThread() && CanParkNow()) {
    AsanPoisonString(string_);
  }
#endif  // defined(ADDRESS_SANITIZER) && DCHECK_IS_ON()
}

const String& ParkableStringImpl::ToString() {
  AssertOnValidThread();
  MutexLocker locker(mutex_);
  AsanUnpoisonString(string_);

  Unpark();
  return string_;
}

unsigned ParkableStringImpl::CharactersSizeInBytes() const {
  AssertOnValidThread();
  return length_ * (is_8bit() ? sizeof(LChar) : sizeof(UChar));
}

bool ParkableStringImpl::Park(ParkingMode mode) {
  AssertOnValidThread();
  MutexLocker locker(mutex_);
  DCHECK(may_be_parked_);
  if (state_ == State::kUnparked && CanParkNow()) {
    // Parking can proceed synchronously.
    if (has_compressed_data()) {
      RecordParkingAction(ParkingAction::kParkedInBackground);
      state_ = State::kParked;
      ParkableStringManager::Instance().OnParked(this, string_.Impl());

      // Must unpoison the memory before releasing it.
      AsanUnpoisonString(string_);
      string_ = String();
    } else if (mode == ParkingMode::kAlways) {
      // |string_|'s data should not be touched except in the compression task.
      AsanPoisonString(string_);
      // |params| keeps |this| alive until |OnParkingCompleteOnMainThread()|.
      auto params = std::make_unique<CompressionTaskParams>(
          this, string_.Bytes(), string_.CharactersSizeInBytes(),
          Thread::Current()->GetTaskRunner());
      background_scheduler::PostOnBackgroundThread(
          FROM_HERE, CrossThreadBind(&ParkableStringImpl::CompressInBackground,
                                     WTF::Passed(std::move(params))));
      state_ = State::kParkingInProgress;
    }
  }

  return state_ == State::kParked || state_ == State::kParkingInProgress;
}

bool ParkableStringImpl::is_parked() const {
  return state_ == State::kParked;
}

bool ParkableStringImpl::CanParkNow() const {
  mutex_.AssertAcquired();
  // Can park iff:
  // - the string is eligible to parking
  // - There are no external reference to |string_|. Since |this| holds a
  //   reference to it, then we are the only one.
  // - |this| is not locked.
  return may_be_parked_ && string_.Impl()->HasOneRef() && lock_depth_ == 0;
}

void ParkableStringImpl::Unpark() {
  AssertOnValidThread();
  mutex_.AssertAcquired();
  if (state_ != State::kParked)
    return;

  TRACE_EVENT1("blink", "ParkableStringImpl::Unpark", "size",
               CharactersSizeInBytes());
  DCHECK(compressed_);
  base::ElapsedTimer timer;

  base::StringPiece compressed_string_piece(
      reinterpret_cast<const char*>(compressed_->data()),
      compressed_->size() * sizeof(uint8_t));
  String uncompressed;
  base::StringPiece uncompressed_string_piece;
  size_t size = CharactersSizeInBytes();
  if (is_8bit()) {
    LChar* data;
    uncompressed = String::CreateUninitialized(length(), data);
    uncompressed_string_piece =
        base::StringPiece(reinterpret_cast<const char*>(data), size);
  } else {
    UChar* data;
    uncompressed = String::CreateUninitialized(length(), data);
    uncompressed_string_piece =
        base::StringPiece(reinterpret_cast<const char*>(data), size);
  }

  // If decompression fails, this is either because:
  // 1. The output buffer is too small
  // 2. Compressed data is corrupted
  // 3. Cannot allocate memory in zlib
  //
  // (1-2) are data corruption, and (3) is OOM. In all cases, we cannot recover
  // the string we need, nothing else to do than to abort.
  CHECK(compression::GzipUncompress(compressed_string_piece,
                                    uncompressed_string_piece));
  string_ = uncompressed;
  state_ = State::kUnparked;
  ParkableStringManager::Instance().OnUnparked(this, string_.Impl());

  bool backgrounded =
      ParkableStringManager::Instance().IsRendererBackgrounded();
  auto action = backgrounded ? ParkingAction::kUnparkedInBackground
                             : ParkingAction::kUnparkedInForeground;
  RecordParkingAction(action);
  RecordStatistics(CharactersSizeInBytes(), timer.Elapsed(), action);
}

void ParkableStringImpl::OnParkingCompleteOnMainThread(
    std::unique_ptr<CompressionTaskParams> params,
    std::unique_ptr<Vector<uint8_t>> compressed) {
  MutexLocker locker(mutex_);
  DCHECK_EQ(State::kParkingInProgress, state_);
  // Between |Park()| and now, things may have happened:
  // 1. |ToString()| or
  // 2. |Lock()| may have been called.
  //
  // We only care about "surviving" calls, that is iff the string returned by
  // |ToString()| is still alive, or whether we are still locked. Since this
  // function is protected by the lock, no concurrent modifications can occur.
  //
  // Finally, since this is a distinct task from any one that can call
  // |ToString()|, the invariant that the pointer stays valid until the next
  // task is preserved.
  if (CanParkNow() && compressed) {
    RecordParkingAction(ParkingAction::kParkedInBackground);
    state_ = State::kParked;
    compressed_ = std::move(compressed);
    ParkableStringManager::Instance().OnParked(this, string_.Impl());

    // Must unpoison the memory before releasing it.
    AsanUnpoisonString(string_);
    string_ = String();
  } else {
    state_ = State::kUnparked;
  }
}

// static
void ParkableStringImpl::CompressInBackground(
    std::unique_ptr<CompressionTaskParams> params) {
  TRACE_EVENT1("blink", "ParkableStringImpl::CompressInBackground", "size",
               params->size);

  base::ElapsedTimer timer;
#if defined(ADDRESS_SANITIZER)
  // Lock the string to prevent a concurrent |Unlock()| on the main thread from
  // poisoning the string in the meantime.
  params->string->Lock();
#endif  // defined(ADDRESS_SANITIZER)
  // Compression touches the string.
  AsanUnpoisonString(params->string->string_);
  base::StringPiece data(reinterpret_cast<const char*>(params->data),
                         params->size);
  std::string compressed_string;
  bool ok = compression::GzipCompress(data, &compressed_string);

  std::unique_ptr<Vector<uint8_t>> compressed = nullptr;
  if (ok && compressed_string.size() < params->size) {
    compressed = std::make_unique<Vector<uint8_t>>();
    compressed->Append(
        reinterpret_cast<const uint8_t*>(compressed_string.c_str()),
        compressed_string.size());
  }
#if defined(ADDRESS_SANITIZER)
  params->string->Unlock();
#endif  // defined(ADDRESS_SANITIZER)

  auto* task_runner = params->callback_task_runner.get();
  size_t size = params->size;
  PostCrossThreadTask(
      *task_runner, FROM_HERE,
      CrossThreadBind(
          [](std::unique_ptr<CompressionTaskParams> params,
             std::unique_ptr<Vector<uint8_t>> compressed) {
            auto* string = params->string.get();
            string->OnParkingCompleteOnMainThread(std::move(params),
                                                  std::move(compressed));
          },
          WTF::Passed(std::move(params)), WTF::Passed(std::move(compressed))));
  RecordStatistics(size, timer.Elapsed(), ParkingAction::kParkedInBackground);
}

ParkableString::ParkableString(scoped_refptr<StringImpl>&& impl) {
  if (!impl) {
    impl_ = nullptr;
    return;
  }

  bool is_parkable = ParkableStringManager::ShouldPark(*impl);
  if (is_parkable) {
    impl_ = ParkableStringManager::Instance().Add(std::move(impl));
  } else {
    impl_ = base::MakeRefCounted<ParkableStringImpl>(
        std::move(impl), ParkableStringImpl::ParkableState::kNotParkable);
  }
}

ParkableString::~ParkableString() = default;

void ParkableString::Lock() const {
  if (impl_)
    impl_->Lock();
}

void ParkableString::Unlock() const {
  if (impl_)
    impl_->Unlock();
}

bool ParkableString::Is8Bit() const {
  return impl_->is_8bit();
}

String ParkableString::ToString() const {
  return impl_ ? impl_->ToString() : String();
}

wtf_size_t ParkableString::CharactersSizeInBytes() const {
  return impl_ ? impl_->CharactersSizeInBytes() : 0;
}

}  // namespace blink
