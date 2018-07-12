// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/wtf/text/movable_string.h"

#include <string.h>

#include "base/metrics/histogram_macros.h"

namespace WTF {

namespace {

template <typename T>
bool IsLargeEnough(const T* impl) {
  // Don't attempt to park strings smaller than this size.
  static constexpr unsigned int kSizeThreshold = 10000;
  return impl && impl->length() > kSizeThreshold;
}

void RecordParkingAction(MovableStringImpl::ParkingAction action) {
  UMA_HISTOGRAM_ENUMERATION("Memory.MovableStringParkingAction", action);
}

}  // namespace

MovableStringImpl::MovableStringImpl() = default;

MovableStringImpl::MovableStringImpl(scoped_refptr<StringImpl>&& impl)
    : is_parked_(false),
      string_(std::move(impl))
#if DCHECK_IS_ON()
      ,
      parked_string_()
#endif
{
  is_null_ = string_.IsNull();
  is_8bit_ = is_null_ ? false : string_.Is8Bit();
  length_ = is_null_ ? 0 : string_.length();
}

MovableStringImpl::~MovableStringImpl() {
  MovableStringTable::Instance().Remove(this, string_.Impl());
}

bool MovableStringImpl::Is8Bit() const {
  return is_8bit_;
}

bool MovableStringImpl::IsNull() const {
  return is_null_;
}

const String& MovableStringImpl::ToString() {
  Unpark();
  return string_;
}

unsigned MovableStringImpl::CharactersSizeInBytes() const {
  return length() * (Is8Bit() ? sizeof(LChar) : sizeof(UChar));
}

bool MovableStringImpl::Park() {
  // Cannot park strings with several references.
  if (!is_parked_ && string_.Impl()->HasOneRef()) {
    RecordParkingAction(ParkingAction::kParkedInBackground);
#if DCHECK_IS_ON()
    // Since the copy is done before freeing the original data, the two strings
    // are guaranteed to not have the same address.
    parked_string_ = string_.IsolatedCopy();
    // Poison the previous allocation.
    // |string_| is kept as is to make sure the memory isn't reused.
    const void* data = Is8Bit()
                           ? static_cast<const void*>(string_.Characters8())
                           : static_cast<const void*>(string_.Characters16());
    memset(const_cast<void*>(data), 0xcc, string_.CharactersSizeInBytes());
#endif
    is_parked_ = true;
  }
  return is_parked_;
}

void MovableStringImpl::Unpark() {
  if (!is_parked_)
    return;

  bool backgrounded = MovableStringTable::Instance().IsRendererBackgrounded();
  RecordParkingAction(backgrounded ? ParkingAction::kUnparkedInBackground
                                   : ParkingAction::kUnparkedInForeground);
  is_parked_ = false;
#if DCHECK_IS_ON()
  // string_'s data has the same address as parked_string_.
  // This enforces MovableStringImpl.ToString().Characthers{8,16}() returning
  // a different value after Park() / Unpark().
  string_ = parked_string_;
  parked_string_ = String();
#endif
  MovableStringTable::Instance().OnUnpark(this, string_.Impl());
}

MovableString::MovableString(scoped_refptr<StringImpl>&& impl) {
  impl_ = MovableStringTable::Instance().Add(std::move(impl));
  if (!impl_)
    impl_ = base::MakeRefCounted<MovableStringImpl>(std::move(impl));
}

MovableString::~MovableString() = default;

bool MovableString::Is8Bit() const {
  return impl_->Is8Bit();
}

bool MovableString::IsNull() const {
  return impl_->IsNull();
}
const String& MovableString::ToString() const {
  return impl_->ToString();
}

unsigned MovableString::CharactersSizeInBytes() const {
  return impl_->CharactersSizeInBytes();
}

MovableStringTable::MovableStringTable() = default;
MovableStringTable::~MovableStringTable() = default;

scoped_refptr<MovableStringImpl> MovableStringTable::Add(
    scoped_refptr<StringImpl>&& string) {
  StringImpl* impl = string.get();
  if (!IsLargeEnough(impl))
    return nullptr;

  auto it = unparked_strings_.find(impl);
  if (it != unparked_strings_.end()) {
    return it->second;
  }
  auto new_movable_string =
      base::MakeRefCounted<MovableStringImpl>(std::move(string));
  unparked_strings_.emplace(impl, new_movable_string.get());
  return new_movable_string;
}

void MovableStringTable::OnUnpark(MovableStringImpl* movable_impl,
                                  StringImpl* impl) {
  if (!IsLargeEnough(movable_impl))
    return;

  auto it = parked_strings_.find(movable_impl);
  DCHECK(it != parked_strings_.end());
  parked_strings_.erase(it);
  unparked_strings_.emplace(impl, movable_impl);
}

void MovableStringTable::Remove(MovableStringImpl* movable_impl,
                                StringImpl* string) {
  if (!IsLargeEnough(movable_impl))
    return;

  if (movable_impl->is_parked()) {
    auto it = parked_strings_.find(movable_impl);
    DCHECK(it != parked_strings_.end());
    parked_strings_.erase(it);
  } else {
    auto it = unparked_strings_.find(string);
    DCHECK(it != unparked_strings_.end());
    unparked_strings_.erase(it);
  }
}

void MovableStringTable::SetRendererBackgrounded(bool backgrounded) {
  backgrounded_ = backgrounded;
}

bool MovableStringTable::IsRendererBackgrounded() const {
  return backgrounded_;
}

void MovableStringTable::MaybeParkAll() {
  if (!IsRendererBackgrounded())
    return;

  size_t total_size = 0, count = 0;
  for (auto it = unparked_strings_.begin(); it != unparked_strings_.end();) {
    MovableStringImpl* str = it->second;
    bool parked = str->Park();
    if (parked) {
      parked_strings_.insert(str);
      it = unparked_strings_.erase(it);
    } else {
      ++it;
      total_size += str->CharactersSizeInBytes();
      count += 1;
    }
  }
  for (const auto* str : parked_strings_) {
    DCHECK(str);
    total_size += str->CharactersSizeInBytes();
    count += 1;
  }

  size_t total_size_kb = total_size / 1000;
  UMA_HISTOGRAM_COUNTS_100000("Memory.MovableStringsTotalSizeKb",
                              total_size_kb);
  UMA_HISTOGRAM_COUNTS_1000("Memory.MovableStringsCount", count);
}

size_t MovableStringTable::Size() const {
  return parked_strings_.size() + unparked_strings_.size();
}

}  // namespace WTF
