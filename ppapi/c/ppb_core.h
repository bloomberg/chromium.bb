/* Copyright (c) 2010 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#ifndef PPAPI_C_PPB_CORE_H_
#define PPAPI_C_PPB_CORE_H_

#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/pp_stdint.h"
#include "ppapi/c/pp_time.h"
#include "ppapi/c/pp_var.h"

struct PP_CompletionCallback;

#define PPB_CORE_INTERFACE "PPB_Core;0.3"

/**
 * @file
 * This file defines the PPB_Core interface defined by the browser and
 * and containing pointers to functions related to memory management,
 * time, and threads.
 */

/**
 * @addtogroup Interfaces
 * @{
 */

/**
 * The PPB_Core interface contains pointers to functions related to memory
 * management, time, and threads on the browser.
 *
 */
struct PPB_Core {
  /**
   * Same as AddRefVar for Resources.
   * AddRefResource is a pointer to a function that adds a reference to
   * a resource.
   *
   * @param[in] config A PP_Resource containing the resource.
   */
  void (*AddRefResource)(PP_Resource resource);

  /**
   * ReleaseResource is a pointer to a function that removes a reference
   * from a resource.
   *
   * @param[in] config A PP_Resource containing the resource.
   */
/*Same as ReleaseVar for Resources. */
  void (*ReleaseResource)(PP_Resource resource);

  /**
   * MemAlloc is a pointer to a function that allocate memory.
   *
   * @param[in] num_bytes A size_t number of bytes to allocate.
   * @return A pointer to the memory if successful, NULL If the
   * allocation fails.
   */
  void* (*MemAlloc)(size_t num_bytes);

  /**
   * MemFree is a pointer to a function that deallocates memory.
   *
   * @param[in] ptr A pointer to the memory to deallocate. It is safe to
   * pass NULL to this function.
   */
  void (*MemFree)(void* ptr);

  /**
   * GetTime is a pointer to a function that returns the "wall clock
   * time" according to the browser.
   *
   * @return A PP_Time containing the "wall clock time" according to the
   * browser.
   */
  PP_Time (*GetTime)();

  /**
   * GetTimeTicks is a pointer to a function that returns the "tick time"
   * according to the browser. This clock is used by the browser when passing
   * some event times to the plugin (e.g., via the
   * PP_InputEvent::time_stamp_seconds field). It is not correlated to any
   * actual wall clock time (like GetTime()). Because of this, it will not run
   * change if the user changes their computer clock.
   *
   * @return A PP_TimeTicks containing the "tick time" according to the
   * browser.
   */

// TODO(brettw) http://code.google.com/p/chromium/issues/detail?id=57448
// This currently does change with wall clock time, but will be fixed in
// a future release.
  PP_TimeTicks (*GetTimeTicks)();

  /**
   * CallOnMainThread is a pointer to a function that schedules work to be
   * executed on the main module thread after the specified delay. The delay
   * may be 0 to specify a call back as soon as possible.
   *
   * The |result| parameter will just be passed as the second argument to the
   * callback. Many applications won't need this, but it allows a plugin to
   * emulate calls of some callbacks which do use this value.
   *
   * NOTE: If the browser is shutting down or if the plugin has no instances,
   * then the callback function may not be called.
   *
   * @param[in] delay_in_milliseconds An int32_t delay in milliseconds.
   * @param[in] callback A PP_CompletionCallback callback function that the
   * browser will call after the specified delay.
   * @param[in] result An int32_t that the browser will pass to the given
   * PP_CompletionCallback.
   */
  void (*CallOnMainThread)(int32_t delay_in_milliseconds,
                           struct PP_CompletionCallback callback,
                           int32_t result);

  /**
   * IsMainThread is a pointer to a function that returns true if the
   * current thread is the main pepper thread.
   *
   * This function is useful for implementing sanity checks, and deciding if
   * dispatching using CallOnMainThread() is required.
   *
   * @return A PP_BOOL containing PP_TRUE if the current thread is the main
   * pepper thread, otherwise PP_FALSE.
   */
  PP_Bool (*IsMainThread)();
};
/**
 * @}
 */


#endif  /* PPAPI_C_DEV_PPB_CORE_DEV_H_ */

