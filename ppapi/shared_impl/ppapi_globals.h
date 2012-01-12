// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_SHARED_IMPL_PPAPI_GLOBALS_H_
#define PPAPI_SHARED_IMPL_PPAPI_GLOBALS_H_

#include "base/basictypes.h"
#include "base/threading/thread_local.h"  // For testing purposes only.
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_module.h"
#include "ppapi/shared_impl/api_id.h"
#include "ppapi/shared_impl/ppapi_shared_export.h"

namespace base {
class Lock;
}

namespace ppapi {

class CallbackTracker;
class FunctionGroupBase;
class ResourceTracker;
class VarTracker;

// Abstract base class
class PPAPI_SHARED_EXPORT PpapiGlobals {
 public:
  PpapiGlobals();

  // This constructor is to be used only for making a PpapiGlobal for testing
  // purposes. This avoids setting the global static ppapi_globals_. For unit
  // tests that use this feature, the "test" PpapiGlobals should be constructed
  // using this method. See SetPpapiGlobalsOnThreadForTest for more information.
  struct ForTest {};
  PpapiGlobals(ForTest);

  virtual ~PpapiGlobals();

  // Getter for the global singleton.
  inline static PpapiGlobals* Get() {
    if (ppapi_globals_)
      return ppapi_globals_;
    // In unit tests, the following might be valid (see
    // SetPpapiGlobalsOnThreadForTest). Normally, this will just return NULL.
    return GetThreadLocalPointer();
  }

  // This allows us to set a given PpapiGlobals object as the PpapiGlobals for
  // a given thread. After setting the PpapiGlobals for a thread, Get() will
  // return that PpapiGlobals when Get() is called on that thread. Other threads
  // are unaffected. This allows us to have tests which use >1 PpapiGlobals in
  // the same process, e.g. for having 1 thread emulate the "host" and 1 thread
  // emulate the "plugin".
  //
  // PpapiGlobals object must have been constructed using the "ForTest"
  // parameter.
  static void SetPpapiGlobalsOnThreadForTest(PpapiGlobals* ptr);

  // Retrieves the corresponding tracker.
  virtual ResourceTracker* GetResourceTracker() = 0;
  virtual VarTracker* GetVarTracker() = 0;
  virtual CallbackTracker* GetCallbackTrackerForInstance(
      PP_Instance instance) = 0;
  virtual base::Lock* GetProxyLock() = 0;

  // Returns the function object corresponding to the given ID, or NULL if
  // there isn't one.
  virtual FunctionGroupBase* GetFunctionAPI(PP_Instance inst, ApiID id) = 0;

  // Returns the PP_Module associated with the given PP_Instance, or 0 on
  // failure.
  virtual PP_Module GetModuleForInstance(PP_Instance instance) = 0;

  virtual bool IsHostGlobals() const;
  virtual bool IsPluginGlobals() const;

 private:
  // Return the thread-local pointer which is used only for unit testing. It
  // should always be NULL when running in production. It allows separate
  // threads to have distinct "globals".
  static PpapiGlobals* GetThreadLocalPointer();

  static PpapiGlobals* ppapi_globals_;

  DISALLOW_COPY_AND_ASSIGN(PpapiGlobals);
};

}  // namespace ppapi

#endif  // PPAPI_SHARED_IMPL_PPAPI_GLOBALS_H_
