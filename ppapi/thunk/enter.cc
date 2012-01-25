// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/thunk/enter.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "ppapi/c/pp_errors.h"
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
    MessageLoop::current()->PostTask(FROM_HERE, base::Bind(
        callback_.func, callback_.user_data, retval_));
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
  MessageLoop::current()->PostTask(FROM_HERE, base::Bind(
      callback_.func, callback_.user_data, result));

  // Now that the callback will be issued in the future, we should return
  // "pending" to the caller, and not issue the callback again.
  callback_ = PP_BlockUntilComplete();
  retval_ = PP_OK_COMPLETIONPENDING;
  return retval_;
}

FunctionGroupBase* EnterBase::GetFunctions(PP_Instance instance,
                                           ApiID id) const {
  return PpapiGlobals::Get()->GetFunctionAPI(instance, id);
}

Resource* EnterBase::GetResource(PP_Resource resource) const {
  return PpapiGlobals::Get()->GetResourceTracker()->GetResource(resource);
}

void EnterBase::SetStateForResourceError(PP_Resource /* pp_resource */,
                                         Resource* /* resource_base */,
                                         void* object,
                                         bool /* report_error */) {
  if (object)
    return;  // Everything worked.

  retval_ = PP_ERROR_BADRESOURCE;
  // TODO(brettw) log the error.
}

}  // namespace subtle

EnterResourceCreation::EnterResourceCreation(PP_Instance instance)
    : EnterFunctionNoLock<ResourceCreationAPI>(instance, true) {
}

EnterResourceCreation::~EnterResourceCreation() {
}

EnterInstance::EnterInstance(PP_Instance instance)
    : EnterFunctionNoLock<PPB_Instance_FunctionAPI>(instance, true) {
}

EnterInstance::~EnterInstance() {
}

}  // namespace thunk
}  // namespace ppapi
