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

ParkableStringImpl::ParkableStringImpl() = default;

ParkableStringImpl::ParkableStringImpl(scoped_refptr<StringImpl>&& impl)
    : string_(std::move(impl)), is_parked_(false) {}

ParkableStringImpl::~ParkableStringImpl() {
  if (ParkableStringManager::ShouldPark(string_.Impl()))
    ParkableStringManager::Instance().Remove(string_.Impl());
}

bool ParkableStringImpl::Is8Bit() const {
  return string_.Is8Bit();
}

bool ParkableStringImpl::IsNull() const {
  return string_.IsNull();
}

const String& ParkableStringImpl::ToString() {
  Unpark();
  return string_;
}

unsigned ParkableStringImpl::CharactersSizeInBytes() const {
  return string_.CharactersSizeInBytes();
}

bool ParkableStringImpl::Park() {
  // Cannot park strings with several references.
  if (string_.Impl()->HasOneRef()) {
    RecordParkingAction(ParkingAction::kParkedInBackground);
    is_parked_ = true;
  }
  return is_parked_;
}

void ParkableStringImpl::Unpark() {
  if (!is_parked_)
    return;

  bool backgrounded =
      ParkableStringManager::Instance().IsRendererBackgrounded();
  RecordParkingAction(backgrounded ? ParkingAction::kUnparkedInBackground
                                   : ParkingAction::kUnparkedInForeground);
  is_parked_ = false;
}

ParkableString::ParkableString(scoped_refptr<StringImpl>&& impl) {
  if (ParkableStringManager::ShouldPark(impl.get())) {
    impl_ = ParkableStringManager::Instance().Add(std::move(impl));
  } else {
    impl_ = base::MakeRefCounted<ParkableStringImpl>(std::move(impl));
  }
}

ParkableString::~ParkableString() = default;

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
