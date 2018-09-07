// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/bindings/parkable_string.h"

#include "base/metrics/histogram_macros.h"
#include "third_party/blink/renderer/platform/bindings/parkable_string_manager.h"
#include "third_party/blink/renderer/platform/wtf/thread_specific.h"

namespace blink {

namespace {

void RecordParkingAction(ParkableStringImpl::ParkingAction action) {
  UMA_HISTOGRAM_ENUMERATION("Memory.MovableStringParkingAction", action);
}

}  // namespace

ParkableStringImpl::ParkableStringImpl()
    : ParkableStringImpl({nullptr}, ParkableState::kNotParkable) {}

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
}

ParkableStringImpl::~ParkableStringImpl() {
  // Null strings (default-constructed) can be destroyed on a different thread.
  if (!string_.IsNull())
    AssertOnValidThread();

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
  bool is_parkable = ParkableStringManager::ShouldPark(impl.get());
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

bool ParkableString::IsNull() const {
  return impl_->IsNull();
}
const String& ParkableString::ToString() const {
  return impl_->ToString();
}

unsigned ParkableString::CharactersSizeInBytes() const {
  return impl_->CharactersSizeInBytes();
}

}  // namespace blink
