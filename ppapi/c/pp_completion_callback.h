/* Copyright (c) 2011 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#ifndef PPAPI_C_PP_COMPLETION_CALLBACK_H_
#define PPAPI_C_PP_COMPLETION_CALLBACK_H_

/**
 * @file
 * This file defines the API to create and run a callback.
 */

#include <stdlib.h>

#include "ppapi/c/pp_macros.h"
#include "ppapi/c/pp_stdint.h"

/**
 * @addtogroup Typedefs
 * @{
 */

/**
 * PP_CompletionCallback_Func defines the function signature that you implement
 * to receive callbacks on asynchronous completion of an operation.
 *
 * |user_data| is a pointer to user-specified data associated with this
 * function at callback creation. See PP_MakeCompletionCallback() for details.
 *
 * |result| is the result of the operation. Non-positive values correspond to
 * the error codes from pp_errors.h (excluding PP_OK_COMPLETIONPENDING).
 * Positive values indicate additional information such as bytes read.
 */
typedef void (*PP_CompletionCallback_Func)(void* user_data, int32_t result);
/**
 * @}
 */

/**
 *
 * @addtogroup Enums
 * @{
 */

/**
 * This enumeration contains flags used to control how non-NULL callbacks are
 * scheduled by asynchronous methods.
 */
typedef enum {
  /**
   * This flag allows any non-NULL callback to be always invoked asynchronously,
   * on success or error, even if the operation could complete synchronously
   * without blocking.
   *
   * The method taking such callback will always return PP_OK_COMPLETIONPENDING.
   * The callback will be invoked on the main thread of PPAPI execution.
   *
   * TODO(polina): make this the default once all the clients use flags.
   */
  PP_COMPLETIONCALLBACK_FLAG_NONE = 0 << 0,
  /**
   * This flag allows any method taking such callback to complete synchronously
   * and not call the callback if the operation would not block. This is useful
   * when performance is an issue, and the operation bandwidth should not be
   * limited to the processing speed of the message loop.
   *
   * On synchronous method completion, the completion result will be returned
   * by the method itself. Otherwise, the method will return
   * PP_OK_COMPLETIONPENDING, and the callback will be invoked asynchronously on
   * the main thread of PPAPI execution.
   */
  PP_COMPLETIONCALLBACK_FLAG_OPTIONAL = 1 << 0
} PP_CompletionCallback_Flag;
PP_COMPILE_ASSERT_SIZE_IN_BYTES(PP_CompletionCallback_Flag, 4);


/**
 * @addtogroup Structs
 * @{
 */

/**
 * Any method that takes a PP_CompletionCallback can complete asynchronously.
 * Refer to PP_CompletionCallback_Flag for more information.
 *
 * If PP_CompletionCallback_Func is NULL, the operation might block if necessary
 * to complete the work. Refer to PP_BlockUntilComplete for more information.
 *
 * See PP_MakeCompletionCallback() for the description of each field.
 */
struct PP_CompletionCallback {
  PP_CompletionCallback_Func func;
  void* user_data;
  int32_t flags;
};
/**
 * @}
 */

/**
 * @addtogroup Functions
 * @{
 */
/**
 * PP_MakeCompletionCallback() is used to create a PP_CompletionCallback
 * without flags. If you want to alter the default callback behavior, set the
 * flags to a bit field combination of PP_CompletionCallback_Flag's.
 *
 * Example:
 *   struct PP_CompletionCallback cc = PP_MakeCompletionCallback(Foo, NULL);
 *   cc.flags = cc.flags | PP_COMPLETIONCALLBACK_FLAG_OPTIONAL;
 *
 * @param[in] func A PP_CompletionCallback_Func to be called on completion.
 * @param[in] user_data A pointer to user data passed to be passed to the
 * callback function. This is optional and is typically used to help track state
 * in case of multiple pending callbacks.
 *
 * @return A PP_CompletionCallback structure.
 */
PP_INLINE struct PP_CompletionCallback PP_MakeCompletionCallback(
    PP_CompletionCallback_Func func,
    void* user_data) {
  struct PP_CompletionCallback cc;
  cc.func = func;
  cc.user_data = user_data;
  /* TODO(polina): switch the default to PP_COMPLETIONCALLBACK_FLAG_NONE. */
  cc.flags = PP_COMPLETIONCALLBACK_FLAG_OPTIONAL;
  return cc;
}

/**
 * PP_MakeOptionalCompletionCallback() is used to create a PP_CompletionCallback
 * with PP_COMPLETIONCALLBACK_FLAG_OPTIONAL set.
 *
 * @param[in] func A PP_CompletionCallback_Func to be called on completion.
 * @param[in] user_data A pointer to user data passed to be passed to the
 * callback function. This is optional and is typically used to help track state
 * in case of multiple pending callbacks.
 *
 * @return A PP_CompletionCallback structure.
 */
PP_INLINE struct PP_CompletionCallback PP_MakeOptionalCompletionCallback(
    PP_CompletionCallback_Func func,
    void* user_data) {
  struct PP_CompletionCallback cc = PP_MakeCompletionCallback(func, user_data);
  cc.flags = cc.flags | PP_COMPLETIONCALLBACK_FLAG_OPTIONAL;
  return cc;
}
/**
 * @}
 */

/**
 * @addtogroup Functions
 * @{
 */

/**
 * PP_RunCompletionCallback() is used to run a callback. It invokes
 * the callback function passing it user data specified on creation and
 * completion |result|.
 *
 * @param[in] cc A pointer to a PP_CompletionCallback that will be run.
 * @param[in] result The result of the operation. Non-positive values correspond
 * to the error codes from pp_errors.h (excluding PP_OK_COMPLETIONPENDING).
 * Positive values indicate additional information such as bytes read.
 */
PP_INLINE void PP_RunCompletionCallback(struct PP_CompletionCallback* cc,
                                        int32_t result) {
  cc->func(cc->user_data, result);
}

/**
 * @}
 */

/**
 * @addtogroup Functions
 * @{
 */

 /**
 * PP_BlockUntilComplete() is used in place of an actual completion callback
 * to request blocking behavior. If specified, the calling thread will block
 * until the function completes. Blocking completion callbacks are only allowed
 * from background threads.
 *
 * @return A PP_CompletionCallback structure corresponding to a NULL callback.
 */
PP_INLINE struct PP_CompletionCallback PP_BlockUntilComplete() {
  return PP_MakeCompletionCallback(NULL, NULL);
}

/**
 * Runs a callback and clears the reference to it.
 *
 * This is used when the null-ness of a completion callback is used as a signal
 * for whether a completion callback has been registered. In this case, after
 * the execution of the callback, it should be cleared.
 *
 * However, this introduces a conflict if the completion callback wants to
 * schedule more work that involves the same completion callback again (for
 * example, when reading data from an URLLoader, one would typically queue up
 * another read callback). As a result, this function clears the pointer
 * *before* the given callback is executed.
 */
PP_INLINE void PP_RunAndClearCompletionCallback(
    struct PP_CompletionCallback* cc,
    int32_t res) {
  struct PP_CompletionCallback temp = *cc;
  *cc = PP_BlockUntilComplete();
  PP_RunCompletionCallback(&temp, res);
}
/**
 * @}
 */

#endif  /* PPAPI_C_PP_COMPLETION_CALLBACK_H_ */
