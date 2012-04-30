// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/thunk/enter.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/stringprintf.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/shared_impl/ppapi_globals.h"
#include "ppapi/thunk/ppb_instance_api.h"
#include "ppapi/thunk/resource_creation_api.h"

namespace ppapi {
namespace thunk {

namespace subtle {

bool CallbackIsRequired(const PP_CompletionCallback& callback) {
  return callback.func != NULL &&
         (callback.flags & PP_COMPLETIONCALLBACK_FLAG_OPTIONAL) == 0;
}

EnterBase::EnterBase()
    : callback_(PP_BlockUntilComplete()),
      retval_(PP_OK) {
  // TODO(brettw) validate threads.
}

EnterBase::EnterBase(const PP_CompletionCallback& callback)
    : callback_(CallbackIsRequired(callback) ? callback
                                             : PP_BlockUntilComplete()),
      retval_(PP_OK) {
  // TODO(brettw) validate threads.
}

EnterBase::~EnterBase() {
  if (callback_.func) {
    // All async completions should have cleared the callback in SetResult().
    DCHECK(retval_ != PP_OK_COMPLETIONPENDING && retval_ != PP_OK);
    MessageLoop::current()->PostTask(FROM_HERE, RunWhileLocked(base::Bind(
        callback_.func, callback_.user_data, retval_)));
  }
}

int32_t EnterBase::SetResult(int32_t result) {
  if (!callback_.func || result == PP_OK_COMPLETIONPENDING) {
    // Easy case, we don't need to issue the callback (either none is
    // required, or the implementation will asynchronously issue it
    // for us), just store the result.
    callback_ = PP_BlockUntilComplete();
    retval_ = result;
    return retval_;
  }

  // This is a required callback, asynchronously issue it.
  // TODO(brettw) make this work on different threads, etc.
  MessageLoop::current()->PostTask(FROM_HERE, RunWhileLocked(base::Bind(
      callback_.func, callback_.user_data, result)));

  // Now that the callback will be issued in the future, we should return
  // "pending" to the caller, and not issue the callback again.
  callback_ = PP_BlockUntilComplete();
  retval_ = PP_OK_COMPLETIONPENDING;
  return retval_;
}

Resource* EnterBase::GetResource(PP_Resource resource) const {
  return PpapiGlobals::Get()->GetResourceTracker()->GetResource(resource);
}

void EnterBase::SetStateForResourceError(PP_Resource pp_resource,
                                         Resource* resource_base,
                                         void* object,
                                         bool report_error) {
  if (object)
    return;  // Everything worked.

  if (callback_.func) {
    // Required callback, issue the async completion.
    MessageLoop::current()->PostTask(FROM_HERE, RunWhileLocked(base::Bind(
        callback_.func, callback_.user_data,
        static_cast<int32_t>(PP_ERROR_BADRESOURCE))));
    callback_ = PP_BlockUntilComplete();
    retval_ = PP_OK_COMPLETIONPENDING;
  } else {
    retval_ = PP_ERROR_BADRESOURCE;
  }

  // We choose to silently ignore the error when the pp_resource is null
  // because this is a pretty common case and we don't want to have lots
  // of errors in the log. This should be an obvious case to debug.
  if (report_error && pp_resource) {
    std::string message;
    if (resource_base) {
      message = base::StringPrintf(
          "0x%X is not the correct type for this function.",
          pp_resource);
    } else {
      message = base::StringPrintf(
          "0x%X is not a valid resource ID.",
          pp_resource);
    }
    PpapiGlobals::Get()->BroadcastLogWithSource(0, PP_LOGLEVEL_ERROR,
                                                std::string(), message);
  }
}

void EnterBase::SetStateForFunctionError(PP_Instance pp_instance,
                                         void* object,
                                         bool report_error) {
  if (object)
    return;  // Everything worked.

  if (callback_.func) {
    // Required callback, issue the async completion.
    MessageLoop::current()->PostTask(FROM_HERE, RunWhileLocked(base::Bind(
        callback_.func, callback_.user_data,
        static_cast<int32_t>(PP_ERROR_BADARGUMENT))));
    callback_ = PP_BlockUntilComplete();
    retval_ = PP_OK_COMPLETIONPENDING;
  } else {
    retval_ = PP_ERROR_BADARGUMENT;
  }

  // We choose to silently ignore the error when the pp_instance is null as
  // for PP_Resources above.
  if (report_error && pp_instance) {
    std::string message;
    message = base::StringPrintf(
        "0x%X is not a valid instance ID.",
        pp_instance);
    PpapiGlobals::Get()->BroadcastLogWithSource(0, PP_LOGLEVEL_ERROR,
                                                std::string(), message);
  }
}

}  // namespace subtle

EnterInstance::EnterInstance(PP_Instance instance)
    : EnterBase(),
      functions_(PpapiGlobals::Get()->GetInstanceAPI(instance)) {
  SetStateForFunctionError(instance, functions_, true);
}

EnterInstance::EnterInstance(PP_Instance instance,
                             const PP_CompletionCallback& callback)
    : EnterBase(callback),
      functions_(PpapiGlobals::Get()->GetInstanceAPI(instance)) {
  SetStateForFunctionError(instance, functions_, true);
}

EnterInstance::~EnterInstance() {
}

EnterInstanceNoLock::EnterInstanceNoLock(PP_Instance instance)
    : EnterBase(),
      functions_(PpapiGlobals::Get()->GetInstanceAPI(instance)) {
  SetStateForFunctionError(instance, functions_, true);
}

EnterInstanceNoLock::~EnterInstanceNoLock() {
}

EnterResourceCreation::EnterResourceCreation(PP_Instance instance)
    : EnterBase(),
      functions_(PpapiGlobals::Get()->GetResourceCreationAPI(instance)) {
  SetStateForFunctionError(instance, functions_, true);
}

EnterResourceCreation::~EnterResourceCreation() {
}

EnterResourceCreationNoLock::EnterResourceCreationNoLock(PP_Instance instance)
    : EnterBase(),
      functions_(PpapiGlobals::Get()->GetResourceCreationAPI(instance)) {
  SetStateForFunctionError(instance, functions_, true);
}

EnterResourceCreationNoLock::~EnterResourceCreationNoLock() {
}

}  // namespace thunk
}  // namespace ppapi
