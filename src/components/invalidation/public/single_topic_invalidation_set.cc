// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/invalidation/public/single_topic_invalidation_set.h"

#include "base/values.h"
#include "components/invalidation/public/invalidation_util.h"

namespace invalidation {

SingleTopicInvalidationSet::SingleTopicInvalidationSet() = default;

SingleTopicInvalidationSet::SingleTopicInvalidationSet(
    const SingleTopicInvalidationSet& other) = default;

SingleTopicInvalidationSet& SingleTopicInvalidationSet::operator=(
    const SingleTopicInvalidationSet& other) = default;

SingleTopicInvalidationSet::~SingleTopicInvalidationSet() = default;

void SingleTopicInvalidationSet::Insert(const Invalidation& invalidation) {
  invalidations_.insert(invalidation);
}

void SingleTopicInvalidationSet::InsertAll(
    const SingleTopicInvalidationSet& other) {
  invalidations_.insert(other.begin(), other.end());
}

void SingleTopicInvalidationSet::Clear() {
  invalidations_.clear();
}

void SingleTopicInvalidationSet::Erase(const_iterator it) {
  invalidations_.erase(*it);
}

bool SingleTopicInvalidationSet::StartsWithUnknownVersion() const {
  return !invalidations_.empty() &&
         invalidations_.begin()->is_unknown_version();
}

size_t SingleTopicInvalidationSet::GetSize() const {
  return invalidations_.size();
}

bool SingleTopicInvalidationSet::IsEmpty() const {
  return invalidations_.empty();
}

namespace {

struct InvalidationComparator {
  bool operator()(const Invalidation& inv1, const Invalidation& inv2) {
    return inv1.Equals(inv2);
  }
};

}  // namespace

bool SingleTopicInvalidationSet::operator==(
    const SingleTopicInvalidationSet& other) const {
  return std::equal(invalidations_.begin(), invalidations_.end(),
                    other.invalidations_.begin(), InvalidationComparator());
}

SingleTopicInvalidationSet::const_iterator SingleTopicInvalidationSet::begin()
    const {
  return invalidations_.begin();
}

SingleTopicInvalidationSet::const_iterator SingleTopicInvalidationSet::end()
    const {
  return invalidations_.end();
}

SingleTopicInvalidationSet::const_reverse_iterator
SingleTopicInvalidationSet::rbegin() const {
  return invalidations_.rbegin();
}

SingleTopicInvalidationSet::const_reverse_iterator
SingleTopicInvalidationSet::rend() const {
  return invalidations_.rend();
}

const Invalidation& SingleTopicInvalidationSet::back() const {
  return *invalidations_.rbegin();
}

std::unique_ptr<base::ListValue> SingleTopicInvalidationSet::ToValue() const {
  std::unique_ptr<base::ListValue> value(new base::ListValue);
  for (const Invalidation& invalidation : invalidations_) {
    value->Append(invalidation.ToValue());
  }
  return value;
}

}  // namespace invalidation
