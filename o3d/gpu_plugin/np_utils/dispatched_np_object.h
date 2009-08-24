// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef O3D_GPU_PLUGIN_NP_UTILS_DISPATCHED_NP_OBJECT_H_
#define O3D_GPU_PLUGIN_NP_UTILS_DISPATCHED_NP_OBJECT_H_

#include "o3d/gpu_plugin/np_utils/base_np_object.h"
#include "o3d/gpu_plugin/np_utils/np_dispatcher.h"

namespace o3d {
namespace gpu_plugin {

class BaseNPDispatcher;

// DispatchedNPObject is a base class for NPObjects that automatically make
// their regular member functions available as NPObject methods. Usage:
//
// class MyNPObject : public DispatchedNPObject {
//  public:
//   int MyMethod(bool a, float b);
//  protected:
//   NP_UTILS_BEGIN_DISPATCHER_CHAIN(MyNPObject, DispatchedNPObject)
//     NP_UTILS_DISPATCHER(MyMethod, int(bool, float))
//   NP_UTILS_END_DISPATCHER_CHAIN
// };
//
// Multiple member functions may be listed in the dispatcher chain. Inheritance
// is supported. The following types are supported as return types and parameter
// types:
//   * bool
//   * int
//   * float
//   * double
//   * std::string
//   * NPObject*
//
class DispatchedNPObject : public BaseNPObject {
 public:
  explicit DispatchedNPObject(NPP npp);

  // Returns whether a method with the given name is present in the dispatcher
  // chain.
  virtual bool HasMethod(NPIdentifier name);

  // Search through dispatcher chain from most derived to least derived. If
  // a member function with the same name and number of parameters is found,
  // attempt to invoke it and return whether the invocation was successful.
  virtual bool Invoke(NPIdentifier name,
                      const NPVariant* args,
                      uint32_t num_args,
                      NPVariant* result);

  // Return the names of all the methods int he dispatcher chain.
  virtual bool Enumerate(NPIdentifier** names, uint32_t* num_names);

 protected:
  // Get the dispatcher chain for this class and all its superclasses.
  static BaseNPDispatcher* GetStaticDispatcherChain();

  // Get the dispatcher chain for this object.
  virtual BaseNPDispatcher* GetDynamicDispatcherChain();

 private:
  DISALLOW_COPY_AND_ASSIGN(DispatchedNPObject);
};

}  // namespace gpu_plugin
}  // namespace o3d

#endif  // O3D_GPU_PLUGIN_NP_UTILS_DISPATCHED_NP_OBJECT_H_
