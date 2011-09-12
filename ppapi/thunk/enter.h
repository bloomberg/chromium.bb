// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_THUNK_ENTER_H_
#define PPAPI_THUNK_ENTER_H_

#include "base/basictypes.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/proxy/interface_id.h"
#include "ppapi/shared_impl/function_group_base.h"
#include "ppapi/shared_impl/resource.h"
#include "ppapi/shared_impl/tracker_base.h"
#include "ppapi/shared_impl/resource_tracker.h"

namespace ppapi {
namespace thunk {

// EnterHost* helper objects: These objects wrap a call from the C PPAPI into
// the internal implementation. They make sure the lock is acquired and will
// automatically set up some stuff for you.
//
// You should always check whether the enter succeeded before using the object.
// If this fails, then the instance or resource ID supplied was invalid.
//
// The |report_error| arguments to the constructor should indicate if errors
// should be logged to the console. If the calling function expects that the
// input values are correct (the normal case), this should be set to true. In
// some case like |IsFoo(PP_Resource)| the caller is quersioning whether their
// handle is this type, and we don't want to report an error if it's not.
//
// Standalone functions: EnterHostFunction
//   Automatically gets the implementation for the function API for the
//   supplied PP_Instance.
//
// Resource member functions: EnterHostResource
//   Automatically interprets the given PP_Resource as a resource ID and sets
//   up the resource object for you.

template<typename FunctionsT>
class EnterFunction {
 public:
  EnterFunction(PP_Instance instance, bool report_error)
      : functions_(NULL) {
    FunctionGroupBase* base = TrackerBase::Get()->GetFunctionAPI(
        instance, FunctionsT::interface_id);
    if (base)
      functions_ = base->GetAs<FunctionsT>();
    // TODO(brettw) check error and if report_error is set, do something.
  }
  ~EnterFunction() {}

  bool succeeded() const { return !!functions_; }
  bool failed() const { return !functions_; }

  FunctionsT* functions() { return functions_; }

 private:
  FunctionsT* functions_;

  DISALLOW_COPY_AND_ASSIGN(EnterFunction);
};

// Like EnterResource but assumes the lock is already held.
// TODO(brettw) actually implement locks, this is just a placeholder for now.
template<typename FunctionsT>
class EnterFunctionNoLock : public EnterFunction<FunctionsT> {
 public:
  EnterFunctionNoLock(PP_Instance instance, bool report_error)
      : EnterFunction<FunctionsT>(instance, report_error) {
    // TODO(brettw) assert the lock is held.
  }
};

// Used when a caller has a resource, and wants to do EnterFunction for the
// instance corresponding to that resource.
template<typename FunctionsT>
class EnterFunctionGivenResource : public EnterFunction<FunctionsT> {
 public:
  EnterFunctionGivenResource(PP_Resource resource, bool report_error)
      : EnterFunction<FunctionsT>(GetInstanceForResource(resource),
                                  report_error) {
  }

 private:
  static PP_Instance GetInstanceForResource(PP_Resource resource) {
    Resource* object =
        TrackerBase::Get()->GetResourceTracker()->GetResource(resource);
    return object ? object->pp_instance() : 0;
  }
};

// EnterResource ---------------------------------------------------------------

template<typename ResourceT>
class EnterResource {
 public:
  EnterResource(PP_Resource resource, bool report_error)
      : object_(NULL) {
    resource_ = TrackerBase::Get()->GetResourceTracker()->GetResource(resource);
    if (resource_)
      object_ = resource_->GetAs<ResourceT>();
    // TODO(brettw) check error and if report_error is set, do something.
  }
  ~EnterResource() {}

  bool succeeded() const { return !!object_; }
  bool failed() const { return !object_; }

  ResourceT* object() { return object_; }
  Resource* resource() { return resource_; }

 private:
  Resource* resource_;
  ResourceT* object_;

  DISALLOW_COPY_AND_ASSIGN(EnterResource);
};

// Like EnterResource but assumes the lock is already held.
// TODO(brettw) actually implement locks, this is just a placeholder for now.
template<typename ResourceT>
class EnterResourceNoLock : public EnterResource<ResourceT> {
 public:
  EnterResourceNoLock(PP_Resource resource, bool report_error)
      : EnterResource<ResourceT>(resource, report_error) {
    // TODO(brettw) assert the lock is held.
  }
};

// Simpler wrapper to enter the resource creation API. This is used for every
// class so we have this helper function to save template instantiations and
// typing.
class EnterResourceCreation : public EnterFunctionNoLock<ResourceCreationAPI> {
 public:
  EnterResourceCreation(PP_Instance instance);
  ~EnterResourceCreation();
};

// Simpler wrapper to enter the instance API from proxy code. This is used for
// many interfaces so we have this helper function to save template
// instantiations and typing.
class EnterInstance : public EnterFunctionNoLock<PPB_Instance_FunctionAPI> {
 public:
  EnterInstance(PP_Instance instance);
  ~EnterInstance();
};

}  // namespace thunk
}  // namespace ppapi

#endif  // PPAPI_THUNK_ENTER_H_
