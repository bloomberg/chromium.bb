// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/scopes/scope_lock.h"

#include <ostream>

namespace content {

ScopeLock::ScopeLock() = default;
ScopeLock::~ScopeLock() = default;
ScopeLock::ScopeLock(ScopeLock&& other) noexcept {
  DCHECK(!this->is_locked_)
      << "Cannot move a lock onto an active lock: " << *this;
  this->is_locked_ = other.is_locked_;
  this->range_ = std::move(other.range_);
  this->level_ = other.level_;
  this->closure_runner_ = std::move(other.closure_runner_);
  other.is_locked_ = false;
}
ScopeLock::ScopeLock(ScopeLockRange range, int level, base::OnceClosure closure)
    : is_locked_(!closure.is_null()),
      range_(std::move(range)),
      level_(level),
      closure_runner_(std::move(closure)) {}

ScopeLock& ScopeLock::operator=(ScopeLock&& other) noexcept {
  DCHECK(!this->is_locked_)
      << "Cannot move a lock onto an active lock: " << *this;
  this->is_locked_ = other.is_locked_;
  this->range_ = std::move(other.range_);
  this->level_ = other.level_;
  this->closure_runner_ = std::move(other.closure_runner_);
  other.is_locked_ = false;
  return *this;
}

void ScopeLock::Release() {
  if (is_locked_) {
    is_locked_ = false;
    closure_runner_.RunAndReset();
  }
}

std::ostream& operator<<(std::ostream& out, const ScopeLock& lock) {
  return out << "<ScopeLock>{is_locked_: " << lock.is_locked_
             << ", level_: " << lock.level_ << ", range_: " << lock.range_
             << "}";
}

bool operator<(const ScopeLock& x, const ScopeLock& y) {
  if (x.level_ != y.level_)
    return x.level_ < y.level_;
  if (x.range_.begin != y.range_.begin)
    return x.range_.begin < y.range_.begin;
  return x.range_.end < y.range_.end;
}
bool operator==(const ScopeLock& x, const ScopeLock& y) {
  return x.level_ == y.level_ && x.range_.begin == y.range_.begin &&
         x.range_.end == y.range_.end;
}
bool operator!=(const ScopeLock& x, const ScopeLock& y) {
  return !(x == y);
}

}  // namespace content
