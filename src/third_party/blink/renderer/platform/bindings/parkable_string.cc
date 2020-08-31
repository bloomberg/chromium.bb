// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/bindings/parkable_string.h"

#include "base/allocator/partition_allocator/partition_alloc.h"
#include "base/bind.h"
#include "base/check_op.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/process/memory.h"
#include "base/single_thread_task_runner.h"
#include "base/timer/elapsed_timer.h"
#include "base/trace_event/trace_event.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/platform/bindings/parkable_string_manager.h"
#include "third_party/blink/renderer/platform/crypto.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/web_process_memory_dump.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"
#include "third_party/blink/renderer/platform/scheduler/public/worker_pool.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/allocator/partitions.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/sanitizers.h"
#include "third_party/blink/renderer/platform/wtf/thread_specific.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "third_party/zlib/google/compression_utils.h"

namespace blink {

namespace {

ParkableStringImpl::Age MakeOlder(ParkableStringImpl::Age age) {
  switch (age) {
    case ParkableStringImpl::Age::kYoung:
      return ParkableStringImpl::Age::kOld;
    case ParkableStringImpl::Age::kOld:
    case ParkableStringImpl::Age::kVeryOld:
      return ParkableStringImpl::Age::kVeryOld;
  }
}

enum class ParkingAction { kParked, kUnparked, kWritten, kRead };

void RecordStatistics(size_t size,
                      base::TimeDelta duration,
                      ParkingAction action) {
  size_t throughput_mb_s =
      static_cast<size_t>(size / duration.InSecondsF()) / 1000000;
  size_t size_kb = size / 1000;

  const char *size_histogram, *latency_histogram, *throughput_histogram;
  switch (action) {
    case ParkingAction::kParked:
      size_histogram = "Memory.ParkableString.Compression.SizeKb";
      latency_histogram = "Memory.ParkableString.Compression.Latency";
      throughput_histogram = "Memory.ParkableString.Compression.ThroughputMBps";
      break;
    case ParkingAction::kUnparked:
      size_histogram = "Memory.ParkableString.Decompression.SizeKb";
      latency_histogram = "Memory.ParkableString.Decompression.Latency";
      throughput_histogram =
          "Memory.ParkableString.Decompression.ThroughputMBps";
      break;
    case ParkingAction::kWritten:
      size_histogram = "Memory.ParkableString.Write.SizeKb";
      latency_histogram = "Memory.ParkableString.Write.Latency";
      throughput_histogram = "Memory.ParkableString.Write.ThroughputMBps";
      break;
    case ParkingAction::kRead:
      size_histogram = "Memory.ParkableString.Read.SizeKb";
      latency_histogram = "Memory.ParkableString.Read.Latency";
      throughput_histogram = "Memory.ParkableString.Read.ThroughputMBps";
      break;
  }

  // Size should be <1MiB in most cases.
  base::UmaHistogramCounts1000(size_histogram, size_kb);
  // Size is at least 10kB, and at most ~10MB, and throughput ranges from
  // single-digit MB/s to ~1000MB/s depending on the CPU/disk, hence the ranges.
  base::UmaHistogramCustomMicrosecondsTimes(
      latency_histogram, duration, base::TimeDelta::FromMicroseconds(500),
      base::TimeDelta::FromSeconds(1), 100);
  base::UmaHistogramCounts1000(throughput_histogram, throughput_mb_s);
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

// Char buffer allocated using PartitionAlloc, may be nullptr.
class NullableCharBuffer final {
  STACK_ALLOCATED();

 public:
  explicit NullableCharBuffer(size_t size) {
    data_ =
        reinterpret_cast<char*>(WTF::Partitions::BufferPartition()->AllocFlags(
            base::PartitionAllocReturnNull, size, "NullableCharBuffer"));
    size_ = size;
  }

  ~NullableCharBuffer() {
    if (data_)
      WTF::Partitions::BufferPartition()->Free(data_);
  }

  // May return nullptr.
  char* data() const { return data_; }
  size_t size() const { return size_; }

 private:
  char* data_;
  size_t size_;

  DISALLOW_COPY_AND_ASSIGN(NullableCharBuffer);
};

}  // namespace

// Created and destroyed on the same thread, accessed on a background thread as
// well. |string|'s reference counting is *not* thread-safe, hence |string|'s
// reference count must *not* change on the background thread.
struct BackgroundTaskParams final {
  BackgroundTaskParams(
      scoped_refptr<ParkableStringImpl> string,
      const void* data,
      size_t size,
      scoped_refptr<base::SingleThreadTaskRunner> callback_task_runner)
      : callback_task_runner(callback_task_runner),
        string(string),
        data(data),
        size(size) {}

  ~BackgroundTaskParams() { DCHECK(IsMainThread()); }

  const scoped_refptr<base::SingleThreadTaskRunner> callback_task_runner;
  const scoped_refptr<ParkableStringImpl> string;
  const void* data;
  const size_t size;

  BackgroundTaskParams(BackgroundTaskParams&&) = delete;
  DISALLOW_COPY_AND_ASSIGN(BackgroundTaskParams);
};

// Valid transitions are:
//
// Compression:
// 1. kUnparked -> kParked: Parking completed normally
// 4. kParked -> kUnparked: String has been unparked.
//
// Disk:
// 1. kParked -> kOnDisk: Writing completed successfully
// 4. kOnDisk -> kUnParked: The string is requested, triggering a read and
//    decompression
//
// Since parking and disk writing are not synchronous operations the first time,
// when the asynchronous background task is posted,
// |background_task_in_progress_| is set to true. This prevents further string
// aging, and protects against concurrent background tasks.
//
// Each state can be combined with a string that is either old or
// young. Examples below:
// - kUnParked:
//   - (Very) Old: old strings are not necessarily parked
//   - Young: a string starts young and unparked.
// - kParked:
//   - (Very) Old: Parked, and not touched nor locked since then
//   - Young: Lock() makes a string young but doesn't unpark it.
// - kOnDisk:
//   - Very Old: On disk, and not touched nor locked since then
//   - Young: Lock() makes a string young but doesn't unpark it.
enum class ParkableStringImpl::State : uint8_t { kUnparked, kParked, kOnDisk };

// Current "ownership" status of the underlying data.
//
// - kUnreferencedExternally: |string_| is not referenced externally, and the
//   class is free to change it.
// - kTooManyReferences: |string_| has multiple references pointing to it,
//   cannot change it.
// - kLocked: |this| is locked.
enum class ParkableStringImpl::Status : uint8_t {
  kUnreferencedExternally,
  kTooManyReferences,
  kLocked
};

ParkableStringImpl::ParkableMetadata::ParkableMetadata(
    String string,
    std::unique_ptr<SecureDigest> digest)
    : mutex_(),
      lock_depth_(0),
      state_(State::kUnparked),
      background_task_in_progress_(false),
      compressed_(nullptr),
      digest_(*digest),
      age_(Age::kYoung),
      is_8bit_(string.Is8Bit()),
      length_(string.length()) {}

// static
std::unique_ptr<ParkableStringImpl::SecureDigest>
ParkableStringImpl::HashString(StringImpl* string) {
  DigestValue digest_result;
  bool ok = ComputeDigest(kHashAlgorithmSha256,
                          static_cast<const char*>(string->Bytes()),
                          string->CharactersSizeInBytes(), digest_result);

  // The only case where this can return false in BoringSSL is an allocation
  // failure of the temporary data required for hashing. In this case, there
  // is nothing better to do than crashing.
  if (!ok) {
    // Don't know the exact size, the SHA256 spec hints at ~64 (block size)
    // + 32 (digest) bytes.
    base::TerminateBecauseOutOfMemory(64 + kDigestSize);
  }
  // Unless SHA256 is... not 256 bits?
  DCHECK(digest_result.size() == kDigestSize);
  return std::make_unique<SecureDigest>(digest_result);
}

// static
scoped_refptr<ParkableStringImpl> ParkableStringImpl::MakeNonParkable(
    scoped_refptr<StringImpl>&& impl) {
  return base::AdoptRef(new ParkableStringImpl(std::move(impl), nullptr));
}

// static
scoped_refptr<ParkableStringImpl> ParkableStringImpl::MakeParkable(
    scoped_refptr<StringImpl>&& impl,
    std::unique_ptr<SecureDigest> digest) {
  DCHECK(!!digest);
  return base::AdoptRef(
      new ParkableStringImpl(std::move(impl), std::move(digest)));
}

ParkableStringImpl::ParkableStringImpl(scoped_refptr<StringImpl>&& impl,
                                       std::unique_ptr<SecureDigest> digest)
    : string_(std::move(impl)),
      metadata_(digest ? std::make_unique<ParkableMetadata>(string_,
                                                            std::move(digest))
                       : nullptr)
#if DCHECK_IS_ON()
      ,
      owning_thread_(CurrentThread())
#endif
{
  DCHECK(!string_.IsNull());
}

ParkableStringImpl::~ParkableStringImpl() {
  AssertOnValidThread();
  if (!may_be_parked())
    return;

  DCHECK_EQ(0, lock_depth_for_testing());
  AsanUnpoisonString(string_);
  // Cannot destroy while parking is in progress, as the object is kept alive by
  // the background task.
  DCHECK(!metadata_->background_task_in_progress_);

  auto& manager = ParkableStringManager::Instance();
  manager.Remove(this);

  if (has_on_disk_data())
    manager.data_allocator().Discard(std::move(metadata_->on_disk_metadata_));
}

void ParkableStringImpl::Lock() {
  if (!may_be_parked())
    return;

  MutexLocker locker(metadata_->mutex_);
  metadata_->lock_depth_ += 1;
  // Make young as this is a strong (but not certain) indication that the string
  // will be accessed soon.
  MakeYoung();
}

void ParkableStringImpl::Unlock() {
  if (!may_be_parked())
    return;

  MutexLocker locker(metadata_->mutex_);
  DCHECK_GT(metadata_->lock_depth_, 0);
  metadata_->lock_depth_ -= 1;

#if defined(ADDRESS_SANITIZER) && DCHECK_IS_ON()
  // There are no external references to the data, nobody should touch the data.
  //
  // Note: Only poison the memory if this is on the owning thread, as this is
  // otherwise racy. Indeed |Unlock()| may be called on any thread, and
  // the owning thread may concurrently call |ToString()|. It is then allowed
  // to use the string until the end of the current owning thread task.
  // Requires DCHECK_IS_ON() for the |owning_thread_| check.
  //
  // Checking the owning thread first as |CurrentStatus()| can only be called
  // from the owning thread.
  if (owning_thread_ == CurrentThread() &&
      CurrentStatus() == Status::kUnreferencedExternally) {
    AsanPoisonString(string_);
  }
#endif  // defined(ADDRESS_SANITIZER) && DCHECK_IS_ON()
}

const String& ParkableStringImpl::ToString() {
  AssertOnValidThread();
  if (!may_be_parked())
    return string_;

  MutexLocker locker(metadata_->mutex_);
  MakeYoung();
  AsanUnpoisonString(string_);
  Unpark();
  return string_;
}

unsigned ParkableStringImpl::CharactersSizeInBytes() const {
  AssertOnValidThread();
  if (!may_be_parked())
    return string_.CharactersSizeInBytes();

  return metadata_->length_ * (is_8bit() ? sizeof(LChar) : sizeof(UChar));
}

size_t ParkableStringImpl::MemoryFootprintForDump() const {
  AssertOnValidThread();
  size_t size = sizeof(ParkableStringImpl);

  if (!may_be_parked())
    return size + string_.CharactersSizeInBytes();

  size += sizeof(ParkableMetadata);

  if (!is_parked())
    size += string_.CharactersSizeInBytes();

  if (metadata_->compressed_)
    size += metadata_->compressed_->size();

  return size;
}

ParkableStringImpl::AgeOrParkResult ParkableStringImpl::MaybeAgeOrParkString() {
  MutexLocker locker(metadata_->mutex_);
  AssertOnValidThread();
  DCHECK(may_be_parked());
  DCHECK(!is_on_disk());

  // No concurrent background tasks.
  if (metadata_->background_task_in_progress_)
    return AgeOrParkResult::kSuccessOrTransientFailure;

  // TODO(lizeb): Simplify logic below.
  if (is_parked()) {
    if (metadata_->age_ == Age::kVeryOld) {
      bool ok = ParkInternal(ParkingMode::kToDisk);
      if (!ok)
        return AgeOrParkResult::kNonTransientFailure;
    } else {
      metadata_->age_ = MakeOlder(metadata_->age_);
    }
    return AgeOrParkResult::kSuccessOrTransientFailure;
  }

  Status status = CurrentStatus();
  Age age = metadata_->age_;
  if (age == Age::kYoung) {
    if (status == Status::kUnreferencedExternally)
      metadata_->age_ = MakeOlder(age);
  } else if (age == Age::kOld && CanParkNow()) {
    bool ok = ParkInternal(ParkingMode::kCompress);
    DCHECK(ok);
    return AgeOrParkResult::kSuccessOrTransientFailure;
  }

  // External references to a string can be long-lived, cannot provide a
  // progress guarantee for this string.
  return status == Status::kTooManyReferences
             ? AgeOrParkResult::kNonTransientFailure
             : AgeOrParkResult::kSuccessOrTransientFailure;
}

bool ParkableStringImpl::Park(ParkingMode mode) {
  MutexLocker locker(metadata_->mutex_);
  AssertOnValidThread();
  DCHECK(may_be_parked());

  if (metadata_->state_ == State::kParked)
    return true;

  // Making the string old to cancel parking if it is accessed/locked before
  // parking is complete.
  metadata_->age_ = Age::kOld;
  if (!CanParkNow())
    return false;

  ParkInternal(mode);
  return true;
}

// Returns false if parking fails and will fail in the future (non-transient
// failure).
bool ParkableStringImpl::ParkInternal(ParkingMode mode) {
  DCHECK(metadata_->state_ == State::kUnparked ||
         metadata_->state_ == State::kParked);
  DCHECK(metadata_->age_ != Age::kYoung);
  DCHECK(CanParkNow());

  // No concurrent background tasks.
  if (metadata_->background_task_in_progress_)
    return true;

  switch (mode) {
    case ParkingMode::kSynchronousOnly:
      if (has_compressed_data())
        DiscardUncompressedData();
      break;
    case ParkingMode::kCompress:
      if (has_compressed_data())
        DiscardUncompressedData();
      else
        PostBackgroundCompressionTask();
      break;
    case ParkingMode::kToDisk:
      auto& manager = ParkableStringManager::Instance();
      if (has_on_disk_data()) {
        DiscardCompressedData();
      } else {
        // If the disk allocator doesn't accept writes, then the failure is not
        // transient, notify the caller. This is important so that
        // ParkableStringManager doesn't endlessly schedule aging tasks when
        // writing to disk is not possible.
        if (!manager.data_allocator().may_write())
          return false;
        PostBackgroundWritingTask();
      }
      break;
  }
  return true;
}

void ParkableStringImpl::DiscardUncompressedData() {
  // Must unpoison the memory before releasing it.
  AsanUnpoisonString(string_);
  string_ = String();

  metadata_->state_ = State::kParked;
  ParkableStringManager::Instance().OnParked(this);
}

void ParkableStringImpl::DiscardCompressedData() {
  metadata_->compressed_ = nullptr;
  metadata_->state_ = State::kOnDisk;
  ParkableStringManager::Instance().OnWrittenToDisk(this);
}

bool ParkableStringImpl::is_parked() const {
  return metadata_->state_ == State::kParked;
}

bool ParkableStringImpl::is_on_disk() const {
  return metadata_->state_ == State::kOnDisk;
}

ParkableStringImpl::Status ParkableStringImpl::CurrentStatus() const {
  AssertOnValidThread();
  DCHECK(may_be_parked());
  // Can park iff:
  // - |this| is not locked.
  // - There are no external reference to |string_|. Since |this| holds a
  //   reference to |string_|, it must the only one.
  if (metadata_->lock_depth_ != 0)
    return Status::kLocked;
  // Can be null if it is compressed or on disk.
  if (string_.IsNull())
    return Status::kUnreferencedExternally;

  if (!string_.Impl()->HasOneRef())
    return Status::kTooManyReferences;

  return Status::kUnreferencedExternally;
}

bool ParkableStringImpl::CanParkNow() const {
  return CurrentStatus() == Status::kUnreferencedExternally &&
         metadata_->age_ != Age::kYoung;
}

void ParkableStringImpl::Unpark() {
  AssertOnValidThread();
  DCHECK(may_be_parked());

  if (metadata_->state_ == State::kUnparked)
    return;

  TRACE_EVENT1("blink", "ParkableStringImpl::Unpark", "size",
               CharactersSizeInBytes());
  DCHECK(metadata_->compressed_ || metadata_->on_disk_metadata_);
  string_ = UnparkInternal();
  metadata_->state_ = State::kUnparked;
  ParkableStringManager::Instance().OnUnparked(this);
}

String ParkableStringImpl::UnparkInternal() {
  AssertOnValidThread();
  DCHECK(is_parked() || is_on_disk());
  // Note: No need for |mutex_| to be held, this doesn't touch any member
  // variable protected by it.

  base::ElapsedTimer timer;

  if (is_on_disk()) {
    base::ElapsedTimer disk_read_timer;
    DCHECK(has_on_disk_data());
    metadata_->compressed_ = std::make_unique<Vector<uint8_t>>();
    metadata_->compressed_->Grow(metadata_->on_disk_metadata_->size());
    auto& manager = ParkableStringManager::Instance();
    manager.data_allocator().Read(*metadata_->on_disk_metadata_,
                                  metadata_->compressed_->data());
    RecordStatistics(metadata_->on_disk_metadata_->size(),
                     disk_read_timer.Elapsed(), ParkingAction::kRead);
    manager.OnReadFromDisk(this);
  }

  base::StringPiece compressed_string_piece(
      reinterpret_cast<const char*>(metadata_->compressed_->data()),
      metadata_->compressed_->size() * sizeof(uint8_t));
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

  // If the buffer size is incorrect, then we have a corrupted data issue,
  // and in such case there is nothing else to do than crash.
  CHECK_EQ(compression::GetUncompressedSize(compressed_string_piece),
           uncompressed_string_piece.size());
  // If decompression fails, this is either because:
  // 1. Compressed data is corrupted
  // 2. Cannot allocate memory in zlib
  //
  // (1) is data corruption, and (2) is OOM. In all cases, we cannot
  // recover the string we need, nothing else to do than to abort.
  //
  // Stability sheriffs: If you see this, this is likely an OOM.
  CHECK(compression::GzipUncompress(compressed_string_piece,
                                    uncompressed_string_piece));

  base::TimeDelta elapsed = timer.Elapsed();
  ParkableStringManager::Instance().RecordUnparkingTime(elapsed);
  RecordStatistics(CharactersSizeInBytes(), elapsed, ParkingAction::kUnparked);

  return uncompressed;
}

void ParkableStringImpl::PostBackgroundCompressionTask() {
  DCHECK(!metadata_->background_task_in_progress_);
  // |string_|'s data should not be touched except in the compression task.
  AsanPoisonString(string_);
  metadata_->background_task_in_progress_ = true;
  // |params| keeps |this| alive until |OnParkingCompleteOnMainThread()|.
  auto params = std::make_unique<BackgroundTaskParams>(
      this, string_.Bytes(), string_.CharactersSizeInBytes(),
      Thread::Current()->GetTaskRunner());
  worker_pool::PostTask(
      FROM_HERE, CrossThreadBindOnce(&ParkableStringImpl::CompressInBackground,
                                     std::move(params)));
}

// static
void ParkableStringImpl::CompressInBackground(
    std::unique_ptr<BackgroundTaskParams> params) {
  TRACE_EVENT1("blink", "ParkableStringImpl::CompressInBackground", "size",
               params->size);

  base::ElapsedTimer timer;
#if defined(ADDRESS_SANITIZER)
  // Lock the string to prevent a concurrent |Unlock()| on the main thread from
  // poisoning the string in the meantime.
  //
  // Don't make the string young at the same time, otherwise parking would
  // always be cancelled on the main thread with address sanitizer, since the
  // |OnParkingCompleteOnMainThread()| callback would be executed on a young
  // string.
  params->string->LockWithoutMakingYoung();
#endif  // defined(ADDRESS_SANITIZER)
  // Compression touches the string.
  AsanUnpoisonString(params->string->string_);
  bool ok;
  base::StringPiece data(reinterpret_cast<const char*>(params->data),
                         params->size);
  std::unique_ptr<Vector<uint8_t>> compressed = nullptr;

  // This runs in background, making CPU starvation likely, and not an issue.
  // Hence, report thread time instead of wall clock time.
  base::ElapsedThreadTimer thread_timer;
  {
    // Temporary vector. As we don't want to waste memory, the temporary buffer
    // has the same size as the initial data. Compression will fail if this is
    // not large enough.
    //
    // This is not using:
    // - malloc() or any STL container: this is discouraged in blink, and there
    //   is a suspected memory regression caused by using it (crbug.com/920194).
    // - WTF::Vector<> as allocation failures result in an OOM crash, whereas
    //   we can fail gracefully. See crbug.com/905777 for an example of OOM
    //   triggered from there.
    NullableCharBuffer buffer(params->size);
    ok = buffer.data();
    size_t compressed_size;
    if (ok) {
      // Use partition alloc for zlib's temporary data. This is crucial to avoid
      // leaking memory on Android, see the details in crbug.com/931553.
      auto fast_malloc = [](size_t size) {
        return WTF::Partitions::FastMalloc(size, "ZlibTemporaryData");
      };
      ok = compression::GzipCompress(data, buffer.data(), buffer.size(),
                                     &compressed_size, fast_malloc,
                                     WTF::Partitions::FastFree);
    }

#if defined(ADDRESS_SANITIZER)
    params->string->Unlock();
#endif  // defined(ADDRESS_SANITIZER)

    if (ok) {
      compressed = std::make_unique<Vector<uint8_t>>();
      // Not using realloc() as we want the compressed data to be a regular
      // WTF::Vector.
      compressed->Append(reinterpret_cast<const uint8_t*>(buffer.data()),
                         compressed_size);
    }
  }
  base::TimeDelta thread_elapsed = thread_timer.Elapsed();

  auto* task_runner = params->callback_task_runner.get();
  size_t size = params->size;
  PostCrossThreadTask(
      *task_runner, FROM_HERE,
      CrossThreadBindOnce(
          [](std::unique_ptr<BackgroundTaskParams> params,
             std::unique_ptr<Vector<uint8_t>> compressed,
             base::TimeDelta parking_thread_time) {
            auto* string = params->string.get();
            string->OnParkingCompleteOnMainThread(
                std::move(params), std::move(compressed), parking_thread_time);
          },
          std::move(params), std::move(compressed), thread_elapsed));
  RecordStatistics(size, timer.Elapsed(), ParkingAction::kParked);
}

void ParkableStringImpl::OnParkingCompleteOnMainThread(
    std::unique_ptr<BackgroundTaskParams> params,
    std::unique_ptr<Vector<uint8_t>> compressed,
    base::TimeDelta parking_thread_time) {
  DCHECK(metadata_->background_task_in_progress_);
  MutexLocker locker(metadata_->mutex_);
  DCHECK_EQ(State::kUnparked, metadata_->state_);
  metadata_->background_task_in_progress_ = false;

  // Always keep the compressed data. Compression is expensive, so even if the
  // uncompressed representation cannot be discarded now, avoid compressing
  // multiple times. This will allow synchronous parking next time.
  DCHECK(!metadata_->compressed_);
  if (compressed)
    metadata_->compressed_ = std::move(compressed);

  // Between |Park()| and now, things may have happened:
  // 1. |ToString()| or
  // 2. |Lock()| may have been called.
  //
  // Both of these will make the string young again, and if so we don't
  // discard the compressed representation yet.
  if (CanParkNow() && metadata_->compressed_) {
    DiscardUncompressedData();
  } else {
    metadata_->state_ = State::kUnparked;
  }
  // Record the time no matter whether the string was parked or not, as the
  // parking cost was paid.
  ParkableStringManager::Instance().RecordParkingThreadTime(
      parking_thread_time);
}

void ParkableStringImpl::PostBackgroundWritingTask() {
  DCHECK(!metadata_->background_task_in_progress_);
  DCHECK_EQ(State::kParked, metadata_->state_);
  auto& manager = ParkableStringManager::Instance();
  auto& data_allocator = manager.data_allocator();
  if (!has_on_disk_data() && data_allocator.may_write()) {
    metadata_->background_task_in_progress_ = true;
    auto params = std::make_unique<BackgroundTaskParams>(
        this, metadata_->compressed_->data(), metadata_->compressed_->size(),
        Thread::Current()->GetTaskRunner());
    worker_pool::PostTask(
        FROM_HERE,
        CrossThreadBindOnce(&ParkableStringImpl::WriteToDiskInBackground,
                            std::move(params)));
  }
}

// static
void ParkableStringImpl::WriteToDiskInBackground(
    std::unique_ptr<BackgroundTaskParams> params) {
  auto& allocator = ParkableStringManager::Instance().data_allocator();
  base::ElapsedTimer timer;
  auto metadata = allocator.Write(params->data, params->size);
  RecordStatistics(params->size, timer.Elapsed(), ParkingAction::kWritten);

  auto* task_runner = params->callback_task_runner.get();
  PostCrossThreadTask(
      *task_runner, FROM_HERE,
      CrossThreadBindOnce(
          [](std::unique_ptr<BackgroundTaskParams> params,
             std::unique_ptr<DiskDataAllocator::Metadata> metadata) {
            auto* string = params->string.get();
            string->OnWritingCompleteOnMainThread(std::move(params),
                                                  std::move(metadata));
          },
          std::move(params), std::move(metadata)));
}

void ParkableStringImpl::OnWritingCompleteOnMainThread(
    std::unique_ptr<BackgroundTaskParams> params,
    std::unique_ptr<DiskDataAllocator::Metadata> on_disk_metadata) {
  DCHECK(metadata_->background_task_in_progress_);
  DCHECK(!metadata_->on_disk_metadata_);

  metadata_->background_task_in_progress_ = false;

  // Writing failed.
  if (!on_disk_metadata)
    return;

  metadata_->on_disk_metadata_ = std::move(on_disk_metadata);
  // State can be:
  // - kParked: unparking didn't happen in the meantime.
  // - Unparked: unparking happened in the meantime.
  DCHECK(metadata_->state_ == State::kUnparked ||
         metadata_->state_ == State::kParked);
  if (metadata_->state_ == State::kParked) {
    DiscardCompressedData();
    metadata_->state_ = State::kOnDisk;
  }
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
    impl_ = ParkableStringImpl::MakeNonParkable(std::move(impl));
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

void ParkableString::OnMemoryDump(WebProcessMemoryDump* pmd,
                                  const String& name) const {
  if (!impl_)
    return;

  auto* dump = pmd->CreateMemoryAllocatorDump(name);
  dump->AddScalar("size", "bytes", impl_->MemoryFootprintForDump());

  const char* parent_allocation =
      may_be_parked() ? ParkableStringManager::kAllocatorDumpName
                      : WTF::Partitions::kAllocatedObjectPoolName;
  pmd->AddSuballocation(dump->Guid(), parent_allocation);
}

bool ParkableString::Is8Bit() const {
  return impl_->is_8bit();
}

const String& ParkableString::ToString() const {
  return impl_ ? impl_->ToString() : g_empty_string;
}

wtf_size_t ParkableString::CharactersSizeInBytes() const {
  return impl_ ? impl_->CharactersSizeInBytes() : 0;
}

}  // namespace blink
