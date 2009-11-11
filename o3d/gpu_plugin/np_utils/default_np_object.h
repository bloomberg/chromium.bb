// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef O3D_GPU_PLUGIN_NP_UTILS_DEFAULT_NP_OBJECT_H_
#define O3D_GPU_PLUGIN_NP_UTILS_DEFAULT_NP_OBJECT_H_

#include "base/basictypes.h"
#include "o3d/gpu_plugin/np_utils/np_headers.h"

namespace gpu_plugin {

class BaseNPDispatcher;

// This class implements each of the functions in the NPClass interface. They
// all return error by default. Note that these are not virtual functions and
// this is not an interface. This class can be used as a mixin so that an
// NPObject class does not need to implement every NPClass function but rather
// inherits a default from DefaultNPObject.
template <typename RootClass>
class DefaultNPObject : public RootClass {
 public:
  void Invalidate() {}

  bool HasMethod(NPIdentifier name) {
    return false;
  }

  bool Invoke(NPIdentifier name,
              const NPVariant* args,
              uint32_t num_args,
              NPVariant* result) {
    return false;
  }

  bool InvokeDefault(const NPVariant* args,
                     uint32_t num_args,
                     NPVariant* result) {
    return false;
  }

  bool HasProperty(NPIdentifier name) {
    return false;
  }

  bool GetProperty(NPIdentifier name, NPVariant* result) {
    return false;
  }

  bool SetProperty(NPIdentifier name, const NPVariant* value) {
    return false;
  }

  bool RemoveProperty(NPIdentifier name) {
    return false;
  }

  bool Enumerate(NPIdentifier** names,
                 uint32_t* count) {
    *names = NULL;
    *count = 0;
    return true;
  }

  bool Construct(const NPVariant* args,
                 uint32_t num_args,
                 NPVariant* result) {
    return false;
  }

  static BaseNPDispatcher* GetDispatcherChain() {
    return NULL;
  }

 protected:
  DefaultNPObject() {}
  virtual ~DefaultNPObject() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(DefaultNPObject);
};
}  // namespace gpu_plugin

#endif  // O3D_GPU_PLUGIN_NP_UTILS_DEFAULT_NP_OBJECT_H_
