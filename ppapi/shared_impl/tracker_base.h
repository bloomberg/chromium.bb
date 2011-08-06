// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_SHARED_IMPL_TRACKER_BASE_H_
#define PPAPI_SHARED_IMPL_TRACKER_BASE_H_

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/proxy/interface_id.h"

namespace ppapi {

class FunctionGroupBase;
class ResourceObjectBase;
class Var;

// Tracks resource and function APIs, providing a mapping between ID and
// object.
// TODO(brettw) Eventually this should be one object with global tracking and
// called "Tracker", and this would be used in both the plugin side of the
// proxy as well as the implementation in the renderer. Currently, all this
// does is forward to the process-type-specific tracker to get the information.
// TODO(fischman/vrk): When brettw fixes the TODO above, fix the ugliness in
// VideoDecoderImpl accordingly.
class TrackerBase {
 public:
  // Must be called before any other function that uses the TrackerBase.
  // This sets the getter that returns the global implmenetation of
  // TrackerBase. It will be different for in the renderer and in the plugin
  // process.
  static void Init(TrackerBase*(*getter)());

  // Retrieves the global tracker. This will be NULL if you haven't called
  // Init() first (it should be unnecessary to NULL-check this).
  static TrackerBase* Get();

  // Returns the resource object corresponding to the given ID, or NULL if
  // there isn't one.
  virtual ResourceObjectBase* GetResourceAPI(PP_Resource res) = 0;

  // Returns the function object corresponding to the given ID, or NULL if
  // there isn't one.
  virtual FunctionGroupBase* GetFunctionAPI(PP_Instance inst,
                                            pp::proxy::InterfaceID id) = 0;

  // Returns the instance corresponding to the given resource, or 0 if the
  // resource is invalid.
  virtual PP_Instance GetInstanceForResource(PP_Resource resource) = 0;

  // PP_Vars -------------------------------------------------------------------

  // Adds a new var to the tracker, returning the new Var ID.
  virtual int32 AddVar(Var* var) = 0;

  // Retrieves a var from the tracker, returning an empty scoped ptr on failure.
  virtual scoped_refptr<Var> GetVar(int32 var_id) const = 0;

  virtual bool AddRefVar(int32 var_id) = 0;
  virtual bool UnrefVar(int32 var_id) = 0;
};

}  // namespace ppapi

#endif  // PPAPI_SHARED_IMPL_TRACKER_BASE_H_
