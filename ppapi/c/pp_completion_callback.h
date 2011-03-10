/* Copyright (c) 2010 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#ifndef PPAPI_C_PP_COMPLETION_CALLBACK_H_
#define PPAPI_C_PP_COMPLETION_CALLBACK_H_

/**
 * @file
 * Defines the API ...
 */

#include <stdlib.h>

#include "ppapi/c/pp_macros.h"
#include "ppapi/c/pp_stdint.h"

/**
 * @addtogroup Typedefs
 * @{
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
 * PP_ERROR_WOULDBLOCK to indicate that the method will complete
 * asynchronously.  In this case it will signal completion by invoking the
 * supplied completion callback, which will always be invoked from the main
 * PPAPI thread of execution.  If the method returns any other value,
 * including PP_OK, the completion callback will not be invoked.  If the
 * completion callback is NULL, then the method will block if necessary to
 * complete its work.  PP_BlockUntilComplete() provides a convenient way to
 * specify blocking behavior.
 *
 * The result parameter passes an int32_t that if negative indicates an error
 * code.  Otherwise the result value indicates success.  If it is a positive
 * value then it may carry additional information.
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
 * Use this in place of an actual completion callback to request blocking
 * behavior.  If specified, the calling thread will block until a method
 *  completes.  This is only usable from background threads.
 */
PP_INLINE struct PP_CompletionCallback PP_BlockUntilComplete() {
  return PP_MakeCompletionCallback(NULL, NULL);
}
/**
 * @}
 */

#endif  /* PPAPI_C_PP_COMPLETION_CALLBACK_H_ */

