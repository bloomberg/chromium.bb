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
 * PP_CompletionCallback_Func defines the signature that you implement to
 * receive callbacks on asynchronous completion.
 */
typedef void (*PP_CompletionCallback_Func)(void* user_data, int32_t result);
/**
 * @}
 */

/**
 * @addtogroup Structs
 * @{
 */

/**
 * Any method that takes a PP_CompletionCallback has the option of completing
 * asynchronously if the operation would block.  Such a method should return
 * PP_OK_COMPLETIONPENDING to indicate that the method will complete
 * asynchronously and will always be invoked from the main thread of PPAPI
 * execution.  If the completion callback is NULL, then the operation will
 * block if necessary to complete its work.  PP_BlockUntilComplete() provides a
 * convenient way to specify blocking behavior. Refer to PP_BlockUntilComplete
 * for more information.
 *
 * The result parameter passes an int32_t that, if negative or equal to 0,
 * indicate if the call will completely asynchronously (the callback will be
 * called with a status code). A value greater than zero indicates additional
 * information such as bytes read.
 */
struct PP_CompletionCallback {
  PP_CompletionCallback_Func func;
  void* user_data;
};
/**
 * @}
 */

/**
 * @addtogroup Functions
 * @{
 */

/**
 * PP_MakeCompletionCallback() is used to create a PP_CompletionCallback.
 *
 * @param[in] func A PP_CompletionCallback_Func that will be called.
 * @param[in] user_data A pointer to user data passed to your callback
 * function. This is optional and is typically used to help track state
 * when you may have multiple callbacks pending.
 * @return A PP_CompletionCallback structure.
 */
PP_INLINE struct PP_CompletionCallback PP_MakeCompletionCallback(
    PP_CompletionCallback_Func func,
    void* user_data) {
  struct PP_CompletionCallback cc;
  cc.func = func;
  cc.user_data = user_data;
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
 * PP_RunCompletionCallback() is used to run a callback.
 *
 * @param[in] cc A pointer to a PP_CompletionCallback that will be run.
 * @param[in] res The result parameter that, if negative or equal to 0,
 * indicate if the call will completely asynchronously (the callback will be
 * called with a status code). A value greater than zero indicates additional
 * information such as bytes read.
 */
PP_INLINE void PP_RunCompletionCallback(struct PP_CompletionCallback* cc,
                                        int32_t res) {
  cc->func(cc->user_data, res);
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
 * until the function completes.  Blocking completion callbacks are only usable
 * from background threads.
 *
 * @return A PP_CompletionCallback structure.
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
