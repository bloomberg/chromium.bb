// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/bindings/parkable_string.h"

#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "third_party/blink/renderer/platform/bindings/parkable_string_manager.h"
#include "third_party/blink/renderer/platform/wtf/address_sanitizer.h"
#include "third_party/blink/renderer/platform/wtf/thread_specific.h"

namespace blink {

namespace {

void RecordParkingAction(ParkableStringImpl::ParkingAction action) {
  UMA_HISTOGRAM_ENUMERATION("Memory.MovableStringParkingAction", action);
}

#if defined(ADDRESS_SANITIZER)
void AsanPoisonString(const String& string) {
  const void* start = string.Is8Bit()
                          ? static_cast<const void*>(string.Characters8())
                          : static_cast<const void*>(string.Characters16());
  ASAN_POISON_MEMORY_REGION(start, string.CharactersSizeInBytes());
}

void AsanUnpoisonString(const String& string) {
  const void* start = string.Is8Bit()
                          ? static_cast<const void*>(string.Characters8())
                          : static_cast<const void*>(string.Characters16());
  ASAN_UNPOISON_MEMORY_REGION(start, string.CharactersSizeInBytes());
}
#endif  // defined(ADDRESS_SANITIZER)

}  // namespace

ParkableStringImpl::ParkableStringImpl(scoped_refptr<StringImpl>&& impl,
                                       ParkableState parkable)
    : mutex_(),
      lock_depth_(0),
      string_(std::move(impl)),
      is_parked_(false),
      is_parkable_(parkable == ParkableState::kParkable)
#if DCHECK_IS_ON()
      ,
      owning_thread_(CurrentThread())
#endif
{
  DCHECK(!string_.IsNull());
}

ParkableStringImpl::~ParkableStringImpl() {
  AssertOnValidThread();
#if defined(ADDRESS_SANITIZER)
  AsanUnpoisonString(string_);
#endif
  if (is_parkable_)
    ParkableStringManager::Instance().Remove(string_.Impl());
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
  if (lock_depth_ == 0 && is_parkable_ && string_.Impl()->HasOneRef() &&
      owning_thread_ == CurrentThread()) {
    AsanPoisonString(string_);
  }
#endif  // defined(ADDRESS_SANITIZER) && DCHECK_IS_ON()
}

bool ParkableStringImpl::Is8Bit() const {
  AssertOnValidThread();
  return string_.Is8Bit();
}

bool ParkableStringImpl::IsNull() const {
  AssertOnValidThread();
  return string_.IsNull();
}

const String& ParkableStringImpl::ToString() {
  AssertOnValidThread();
  MutexLocker locker(mutex_);
#if defined(ADDRESS_SANITIZER)
  AsanUnpoisonString(string_);
#endif

  Unpark();
  return string_;
}

unsigned ParkableStringImpl::CharactersSizeInBytes() const {
  AssertOnValidThread();
  return string_.CharactersSizeInBytes();
}

bool ParkableStringImpl::Park() {
  AssertOnValidThread();
  MutexLocker locker(mutex_);
  DCHECK(is_parkable_);

  if (lock_depth_ != 0)
    return false;

  // Cannot park strings with several references.
  if (string_.Impl()->HasOneRef()) {
#if defined(ADDRESS_SANITIZER)
    AsanPoisonString(string_);
#endif
    RecordParkingAction(ParkingAction::kParkedInBackground);
    is_parked_ = true;
  }
  return is_parked_;
}

void ParkableStringImpl::Unpark() {
  AssertOnValidThread();
  mutex_.AssertAcquired();
  if (!is_parked_)
    return;

  bool backgrounded =
      ParkableStringManager::Instance().IsRendererBackgrounded();
  RecordParkingAction(backgrounded ? ParkingAction::kUnparkedInBackground
                                   : ParkingAction::kUnparkedInForeground);
  is_parked_ = false;
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
  return impl_->Is8Bit();
}

String ParkableString::ToString() const {
  return impl_ ? impl_->ToString() : String();
}

unsigned ParkableString::CharactersSizeInBytes() const {
  return impl_ ? impl_->CharactersSizeInBytes() : 0;
}

}  // namespace blink
