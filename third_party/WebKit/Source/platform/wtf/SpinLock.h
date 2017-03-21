// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WTF_SpinLock_h
#define WTF_SpinLock_h

#include "base/allocator/partition_allocator/spin_lock.h"
#include "base/macros.h"

namespace WTF {

// WTF::SpinLock is base::SpinLock. See
// base/allocator/partition_allocator/spin_lock.h for documentation.
using SpinLock = base::subtle::SpinLock;

}  // namespace WTF

using WTF::SpinLock;

#endif  // WTF_SpinLock_h
