// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WTF_StackTrace_h
#define WTF_StackTrace_h

#include "base/debug/stack_trace.h"

namespace WTF {
namespace debug {

using StackTrace = base::debug::StackTrace;

}  // namespace debug
}  // namespace WTF

#endif  // WTF_StackTrace_h
