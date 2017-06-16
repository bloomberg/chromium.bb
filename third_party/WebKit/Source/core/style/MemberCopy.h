// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MemberCopy_h
#define MemberCopy_h

#include <memory>
#include "core/style/ContentData.h"
#include "core/style/DataPersistent.h"
#include "core/style/DataRef.h"
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

template <typename T>
std::unique_ptr<T> MemberCopy(const std::unique_ptr<T>& v) {
  return v ? v->Clone() : nullptr;
}

template <typename T>
DataPersistent<T> MemberCopy(const DataPersistent<T>& v) {
  return v;
}

inline Persistent<ContentData> MemberCopy(const Persistent<ContentData>& v) {
  return v ? v->Clone() : nullptr;
}

}  // namespace blink

#endif  // MemberCopy_h
