// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ScriptForbiddenScope_h
#define ScriptForbiddenScope_h

#include "base/macros.h"
#include "platform/PlatformExport.h"
#include "platform/wtf/Allocator.h"
#include "platform/wtf/AutoReset.h"
#include "platform/wtf/StackUtil.h"
#include "platform/wtf/WTF.h"

namespace blink {

// Scoped disabling of script execution.
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
    AllowUserAgentScript() : saved_counter_(&GetMutableCounter(), 0) {}
    ~AllowUserAgentScript() { DCHECK(!IsScriptForbidden()); }

   private:
    AutoReset<unsigned> saved_counter_;
  };

  static bool IsScriptForbidden() {
    if (LIKELY(!WTF::MayNotBeMainThread()))
      return g_main_thread_counter_ > 0;
    return GetMutableCounter() > 0;
  }

  // DO NOT USE THESE FUNCTIONS FROM OUTSIDE OF THIS CLASS.
  static void Enter() {
    if (LIKELY(!WTF::MayNotBeMainThread())) {
      ++g_main_thread_counter_;
    } else {
      ++GetMutableCounter();
    }
  }
  static void Exit() {
    DCHECK(IsScriptForbidden());
    if (LIKELY(!WTF::MayNotBeMainThread())) {
      --g_main_thread_counter_;
    } else {
      --GetMutableCounter();
    }
  }

 private:
  static unsigned& GetMutableCounter();
  static unsigned g_main_thread_counter_;
};

}  // namespace blink

#endif  // ScriptForbiddenScope_h
