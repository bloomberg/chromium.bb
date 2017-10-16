// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ScriptForbiddenScope_h
#define ScriptForbiddenScope_h

#include "base/macros.h"
#include "platform/PlatformExport.h"
#include "platform/wtf/Allocator.h"
#include "platform/wtf/AutoReset.h"
#include "platform/wtf/Optional.h"

namespace blink {

// Scoped disabling of script execution on the main thread,
// and only to be used by the main thread.
class PLATFORM_EXPORT ScriptForbiddenScope final {
  STACK_ALLOCATED();
  DISALLOW_COPY_AND_ASSIGN(ScriptForbiddenScope);

 public:
  ScriptForbiddenScope() { Enter(); }
  ~ScriptForbiddenScope() { Exit(); }

  class PLATFORM_EXPORT AllowUserAgentScript final {
    STACK_ALLOCATED();
    DISALLOW_COPY_AND_ASSIGN(AllowUserAgentScript);

   public:
    AllowUserAgentScript() {
      if (IsMainThread())
        change_.emplace(&script_forbidden_count_, 0);
    }
    ~AllowUserAgentScript() {
      DCHECK(!IsMainThread() || !script_forbidden_count_);
    }

   private:
    Optional<AutoReset<unsigned>> change_;
  };

  static void Enter() {
    DCHECK(IsMainThread());
    ++script_forbidden_count_;
  }
  static void Exit() {
    DCHECK(script_forbidden_count_);
    --script_forbidden_count_;
  }
  static bool IsScriptForbidden() {
    return IsMainThread() && script_forbidden_count_;
  }

 private:
  static unsigned script_forbidden_count_;
};

// Scoped disabling of script execution on the main thread,
// if called on the main thread.
//
// No effect when used by from other threads -- simplifies
// call sites that might be used by multiple threads to have
// this scope object perform the is-main-thread check on
// its behalf.
class PLATFORM_EXPORT ScriptForbiddenIfMainThreadScope final {
  STACK_ALLOCATED();
  DISALLOW_COPY_AND_ASSIGN(ScriptForbiddenIfMainThreadScope);

 public:
  ScriptForbiddenIfMainThreadScope() {
    is_main_thread_ = IsMainThread();
    if (is_main_thread_)
      ScriptForbiddenScope::Enter();
  }
  ~ScriptForbiddenIfMainThreadScope() {
    if (is_main_thread_)
      ScriptForbiddenScope::Exit();
  }
  bool is_main_thread_;
};

}  // namespace blink

#endif  // ScriptForbiddenScope_h
