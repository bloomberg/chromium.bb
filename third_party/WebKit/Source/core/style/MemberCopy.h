// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MemberCopy_h
#define MemberCopy_h

#include "platform/heap/Persistent.h"
#include "platform/wtf/RefPtr.h"

namespace blink {

template <typename T>
RefPtr<T> MemberCopy(const RefPtr<T>& v) {
  return v;
}

template <typename T>
Persistent<T> MemberCopy(const Persistent<T>& v) {
  return v;
}

}  // namespace blink

#endif  // MemberCopy_h
