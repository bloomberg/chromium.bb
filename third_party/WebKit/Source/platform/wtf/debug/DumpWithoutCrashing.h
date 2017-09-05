// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WTF_DumpWithoutCrashing_h
#define WTF_DumpWithoutCrashing_h

#include "base/debug/dump_without_crashing.h"

namespace WTF {
namespace debug {

// See base::debug::DumpWithoutCrashing for documentation.
inline void DumpWithoutCrashing() {
  base::debug::DumpWithoutCrashing();
}

}  // namespace debug
}  // namespace WTF

#endif  // WTF_DumpWithoutCrashing_h
