// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/bindings/ScriptForbiddenScope.h"

#include "platform/wtf/Assertions.h"
#include "platform/wtf/ThreadSpecific.h"

namespace blink {

unsigned ScriptForbiddenScope::g_main_thread_counter_ = 0;

unsigned& ScriptForbiddenScope::GetMutableCounter() {
  if (IsMainThread())
    return g_main_thread_counter_;

  DEFINE_THREAD_SAFE_STATIC_LOCAL(WTF::ThreadSpecific<unsigned>,
                                  script_forbidden_counter_, ());
  return *script_forbidden_counter_;
}

}  // namespace blink
