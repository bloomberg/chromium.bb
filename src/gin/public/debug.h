// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GIN_PUBLIC_DEBUG_H_
#define GIN_PUBLIC_DEBUG_H_

#include <stddef.h>

#include "build/build_config.h"
#include "gin/gin_export.h"
#include "v8/include/v8.h"

namespace gin {

class GIN_EXPORT Debug {
 public:
  /* Installs a callback that is invoked each time jit code is added, moved,
   * or removed.
   *
   * This only affects IsolateHolder instances created after
   * SetJitCodeEventHandler was invoked.
   */
  static void SetJitCodeEventHandler(v8::JitCodeEventHandler event_handler);

#if defined(OS_WIN)
  /* Sets a callback that is invoked for exceptions that arise in V8-generated
   * code (jitted code or embedded builtins).
   */
  static void SetUnhandledExceptionCallback(
      v8::UnhandledExceptionCallback callback);

  typedef void (__cdecl *DebugBreakCallback)();

  /* Gets the callback that should be invoked when JavaScript execution is
   * paused at a debug breakpoint.
   */
  static DebugBreakCallback GetDebugBreakCallback();

  /* Sets a callback that is invoked when JavaScript execution is paused at a
   * debug breakpoint.
   */
  static void SetDebugBreakCallback(DebugBreakCallback callback);

  typedef void (__cdecl *DebugResumeCallback)();

  /* Gets the callback that should be invoked when JavaScript execution is
   * resumed after a debug breakpoint.
   */
  static DebugResumeCallback GetDebugResumeCallback();

  /* Sets a callback that is invoked when JavaScript execution is resumed after
   * a debug breakpoint.
   */
  static void SetDebugResumeCallback(DebugResumeCallback callback);

#endif
};

}  // namespace gin

#endif  // GIN_PUBLIC_DEBUG_H_
