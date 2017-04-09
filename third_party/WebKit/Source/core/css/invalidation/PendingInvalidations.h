// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PendingInvalidations_h
#define PendingInvalidations_h

#include "core/css/invalidation/InvalidationSet.h"

namespace blink {

class CORE_EXPORT PendingInvalidations final {
  WTF_MAKE_NONCOPYABLE(PendingInvalidations);

 public:
  PendingInvalidations() {}

  InvalidationSetVector& Descendants() { return descendants_; }
  const InvalidationSetVector& Descendants() const { return descendants_; }
  InvalidationSetVector& Siblings() { return siblings_; }
  const InvalidationSetVector& Siblings() const { return siblings_; }

 private:
  InvalidationSetVector descendants_;
  InvalidationSetVector siblings_;
};

}  // namespace blink

#endif  // PendingInvalidations_h
