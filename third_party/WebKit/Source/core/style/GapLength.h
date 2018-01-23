// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GapLength_h
#define GapLength_h

#include "platform/Length.h"
#include "platform/wtf/Allocator.h"

namespace blink {

class GapLength {
  DISALLOW_NEW();

 public:
  GapLength() : is_normal_(true) {}
  GapLength(const Length& length) : is_normal_(false), length_(length) {
    DCHECK(length.IsFixed() || length.IsPercent());
  }

  bool IsNormal() const { return is_normal_; }
  const Length& GetLength() const {
    DCHECK(!IsNormal());
    return length_;
  }

  bool operator==(const GapLength& o) const {
    return is_normal_ == o.is_normal_ && length_ == o.length_;
  }

  bool operator!=(const GapLength& o) const {
    return is_normal_ != o.is_normal_ || length_ != o.length_;
  }

 private:
  bool is_normal_;
  Length length_;
};

}  // namespace blink

#endif  // GapLength_h
