// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_CPP_COMPLETION_CALLBACK_H_
#define PPAPI_CPP_COMPLETION_CALLBACK_H_

#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/cpp/logging.h"
#include "ppapi/cpp/module.h"

/// @file
/// This file defines the API to create and run a callback.
namespace pp {

/// This API enables you to implement and receive callbacks when
/// Pepper operations complete asynchronously.
class CompletionCallback {
 public:
  /// The default constructor will create a blocking
  /// <code>CompletionCallback</code> that can be passed to a method to
  /// indicate that the calling thread should be blocked until the asynchronous
  /// operation corresponding to the method completes.
  ///
  /// <strong>Note:</strong> Blocking completion callbacks are only allowed from
  /// from background threads.
  CompletionCallback() {
    cc_ = PP_BlockUntilComplete();
  }

  /// A constructor for creating a <code>CompletionCallback</code>.
  ///
  /// @param[in] func The function to be called on completion.
  /// @param[in] user_data The user data to be passed to the callback function.
  /// This is optional and is typically used to help track state in case of
  /// multiple pending callbacks.
  CompletionCallback(PP_CompletionCallback_Func func, void* user_data) {
    cc_ = PP_MakeCompletionCallback(func, user_data);
  }

  /// A constructor for creating a <code>CompletionCallback</code> with
  /// specified flags.
  ///
  /// @param[in] func The function to be called on completion.
  /// @param[in] user_data The user data to be passed to the callback function.
  /// This is optional and is typically used to help track state in case of
  /// multiple pending callbacks.
  /// @param[in] flags Bit field combination of
  /// <code>PP_CompletionCallback_Flag</code> flags used to control how
  /// non-NULL callbacks are scheduled by asynchronous methods.
  CompletionCallback(PP_CompletionCallback_Func func, void* user_data,
                     int32_t flags) {
    cc_ = PP_MakeCompletionCallback(func, user_data);
    cc_.flags = flags;
  }

  /// The set_flags() function is used to set the flags used to control
  /// how non-NULL callbacks are scheduled by asynchronous methods.
  ///
  /// @param[in] flags Bit field combination of
  /// <code>PP_CompletionCallback_Flag</code> flags used to control how
  /// non-NULL callbacks are scheduled by asynchronous methods.
  void set_flags(int32_t flags) { cc_.flags = flags; }

  /// Run() is used to run the <code>CompletionCallback</code>.
  /// Normally, the system runs a <code>CompletionCallback</code> after an
  /// asynchronous operation completes, but programs may wish to run the
  /// <code>CompletionCallback</code> manually in order to reuse the same code
  /// paths.
  ///
  /// @param[in] result The result of the operation to be passed to the
  /// callback function. Non-positive values correspond to the error codes
  /// from <code>pp_errors.h</code> (excluding
  /// <code>PP_OK_COMPLETIONPENDING</code>). Positive values indicate
  /// additional information such as bytes read.
  void Run(int32_t result) {
    PP_DCHECK(cc_.func);
    PP_RunCompletionCallback(&cc_, result);
  }

  /// IsOptional() is used to determine the setting of the
  /// <code>PP_COMPLETIONCALLBACK_FLAG_OPTIONAL</code> flag. This flag allows
  /// any method taking such callback to complete synchronously
  /// and not call the callback if the operation would not block. This is useful
  /// when performance is an issue, and the operation bandwidth should not be
  /// limited to the processing speed of the message loop.
  ///
  /// On synchronous method completion, the completion result will be returned
  /// by the method itself. Otherwise, the method will return
  /// PP_OK_COMPLETIONPENDING, and the callback will be invoked asynchronously
  /// on the main thread of Pepper execution.
  ///
  /// @return true if this callback is optional, otherwise false.
  bool IsOptional() const {
    return (cc_.func == NULL ||
            (cc_.flags & PP_COMPLETIONCALLBACK_FLAG_OPTIONAL) != 0);
  }

  /// The pp_completion_callback() function returns the underlying
  /// <code>PP_CompletionCallback</code>
  ///
  /// @return A <code>PP_CompletionCallback</code>.
  const PP_CompletionCallback& pp_completion_callback() const { return cc_; }

  /// The flags() function returns flags used to control how non-NULL callbacks
  /// are scheduled by asynchronous methods.
  ///
  /// @return An int32_t containing a bit field combination of
  /// <code>PP_CompletionCallback_Flag</code> flags.
  int32_t flags() const { return cc_.flags; }

  /// MayForce() is used when implementing functions taking callbacks.
  /// If the callback is required and <code>result</code> indicates that it has
  /// not been scheduled, it will be forced on the main thread.
  ///
  /// <strong>Example:</strong>
  ///
  /// @code
  ///
  /// int32_t OpenURL(pp::URLLoader* loader,
  ///                 pp::URLRequestInfo* url_request_info,
  ///                 const CompletionCallback& cc) {
  ///   if (loader == NULL || url_request_info == NULL)
  ///     return cc.MayForce(PP_ERROR_BADRESOURCE);
  ///   return loader->Open(*loader, *url_request_info, cc);
  /// }
  ///
  /// @endcode
  ///
  /// @param[in] result PP_OK_COMPLETIONPENDING or the result of the completed
  /// operation to be passed to the callback function. PP_OK_COMPLETIONPENDING
  /// indicates that the callback has already been scheduled. Other
  /// non-positive values correspond to error codes from
  /// <code>pp_errors.h</code>. Positive values indicate additional information
  /// such as bytes read.
  ///
  /// @return <code>PP_OK_COMPLETIONPENDING</code> if the callback has been
  /// forced, result parameter otherwise.
  int32_t MayForce(int32_t result) const {
    if (result == PP_OK_COMPLETIONPENDING || IsOptional())
      return result;
    Module::Get()->core()->CallOnMainThread(0, *this, result);
    return PP_OK_COMPLETIONPENDING;
  }

 protected:
  PP_CompletionCallback cc_;
};

/// BlockUntilComplete() is used in place of an actual completion callback
/// to request blocking behavior. If specified, the calling thread will block
/// until the function completes. Blocking completion callbacks are only
/// allowed from background threads.
///
/// @return A <code>CompletionCallback</code> corresponding to a NULL callback.
CompletionCallback BlockUntilComplete();

}  // namespace pp

#endif  // PPAPI_CPP_COMPLETION_CALLBACK_H_
