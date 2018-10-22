// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/bindings/parkable_string.h"

#include "base/metrics/histogram_macros.h"
#include "third_party/blink/renderer/platform/wtf/thread_specific.h"

namespace blink {

namespace {

bool IsLargeEnough(const StringImpl* impl) {
  // Don't attempt to park strings smaller than this size.
  static constexpr unsigned int kSizeThreshold = 10000;
  return impl && impl->length() > kSizeThreshold;
}

void RecordParkingAction(ParkableStringImpl::ParkingAction action) {
  UMA_HISTOGRAM_ENUMERATION("Memory.MovableStringParkingAction", action);
}

}  // namespace

ParkableStringImpl::ParkableStringImpl() = default;

ParkableStringImpl::ParkableStringImpl(scoped_refptr<StringImpl>&& impl)
    : string_(std::move(impl)), is_parked_(false) {}

ParkableStringImpl::~ParkableStringImpl() {
  ParkableStringTable::Instance().Remove(string_.Impl());
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

  bool backgrounded = ParkableStringTable::Instance().IsRendererBackgrounded();
  RecordParkingAction(backgrounded ? ParkingAction::kUnparkedInBackground
                                   : ParkingAction::kUnparkedInForeground);
  is_parked_ = false;
}

ParkableString::ParkableString(scoped_refptr<StringImpl>&& impl) {
  // Don't park small strings.
  if (IsLargeEnough(impl.get())) {
    impl_ = ParkableStringTable::Instance().Add(std::move(impl));
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

// static
ParkableStringTable& ParkableStringTable::Instance() {
  static auto* table = new WTF::ThreadSpecific<ParkableStringTable>();
  return **table;
}

ParkableStringTable::ParkableStringTable() = default;
ParkableStringTable::~ParkableStringTable() = default;

scoped_refptr<ParkableStringImpl> ParkableStringTable::Add(
    scoped_refptr<StringImpl>&& string) {
  StringImpl* raw_ptr = string.get();
  auto it = table_.find(raw_ptr);
  if (it != table_.end()) {
    return it->second;
  }
  auto new_parkable_string =
      base::MakeRefCounted<ParkableStringImpl>(std::move(string));
  table_.emplace(raw_ptr, new_parkable_string.get());
  return new_parkable_string;
}

void ParkableStringTable::Remove(StringImpl* string) {
  if (!IsLargeEnough(string))
    return;

  auto it = table_.find(string);
  DCHECK(it != table_.end());
  table_.erase(it);
}

void ParkableStringTable::SetRendererBackgrounded(bool backgrounded) {
  backgrounded_ = backgrounded;
}

bool ParkableStringTable::IsRendererBackgrounded() const {
  return backgrounded_;
}

void ParkableStringTable::MaybeParkAll() {
  if (!IsRendererBackgrounded())
    return;

  size_t total_size = 0, count = 0;
  for (auto& kv : table_) {
    ParkableStringImpl* str = kv.second;
    str->Park();
    total_size += str->CharactersSizeInBytes();
    count += 1;
  }

  size_t total_size_kb = total_size / 1000;
  UMA_HISTOGRAM_COUNTS_100000("Memory.MovableStringsTotalSizeKb",
                              total_size_kb);
  UMA_HISTOGRAM_COUNTS_1000("Memory.MovableStringsCount", table_.size());
}

}  // namespace blink
