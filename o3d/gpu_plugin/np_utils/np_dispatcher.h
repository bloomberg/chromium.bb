// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef O3D_GPU_PLUGIN_NP_UTILS_NP_DISPATCHER_H_
#define O3D_GPU_PLUGIN_NP_UTILS_NP_DISPATCHER_H_

#include <string>

#include "o3d/gpu_plugin/np_utils/np_utils.h"
#include "o3d/gpu_plugin/np_utils/np_headers.h"

// Dispatchers make regular member functions available as NPObject methods.
// Usage:
//
// class MyNPObject : public DefaultNPObject<NPObject> {
//  public:
//   int MyMethod(bool a, float b);
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

// These macros are used to make dispatcher chains.
#define NP_UTILS_NP_UTILS_DISPATCHER_JOIN2(a, b) a ## b
#define NP_UTILS_DISPATCHER_JOIN(a, b) NP_UTILS_NP_UTILS_DISPATCHER_JOIN2(a, b)
#define NP_UTILS_DISPATCHER_UNIQUE                                             \
  NP_UTILS_DISPATCHER_JOIN(dispatcher, __LINE__)

#define NP_UTILS_BEGIN_DISPATCHER_CHAIN(Class, BaseClass)                      \
  static BaseNPDispatcher* GetDispatcherChain() {                              \
    typedef Class ThisClass;                                                   \
    BaseNPDispatcher* top_dispatcher = BaseClass::GetDispatcherChain();        \

#define NP_UTILS_DISPATCHER(name, Signature)                                   \
    static NPDispatcher<ThisClass, Signature>                                  \
        NP_UTILS_DISPATCHER_UNIQUE(                                            \
            top_dispatcher,                                                    \
            #name,                                                             \
            &ThisClass::name);                                                 \
    top_dispatcher = &NP_UTILS_DISPATCHER_UNIQUE;                              \

#define NP_UTILS_END_DISPATCHER_CHAIN                                          \
    return top_dispatcher;                                                     \
  }                                                                            \
  bool HasMethod(NPIdentifier name) {                                          \
    return DispatcherHasMethodHelper(GetDispatcherChain(), this, name);        \
  }                                                                            \
  bool Invoke(NPIdentifier name,                                               \
              const NPVariant* args,                                           \
              uint32_t num_args,                                               \
              NPVariant* result) {                                             \
    return DispatcherInvokeHelper(GetDispatcherChain(),                        \
                                  this,                                        \
                                  name,                                        \
                                  args,                                        \
                                  num_args,                                    \
                                  result);                                     \
  }                                                                            \
  bool Enumerate(NPIdentifier** names, uint32_t* num_names) {                  \
    return DispatcherEnumerateHelper(GetDispatcherChain(),                     \
                                     this,                                     \
                                     names,                                    \
                                     num_names);                               \
  }                                                                            \

namespace gpu_plugin {

class BaseNPDispatcher {
 public:
  BaseNPDispatcher(BaseNPDispatcher* next, const NPUTF8* name);

  virtual ~BaseNPDispatcher();

  BaseNPDispatcher* next() const {
    return next_;
  }

  NPIdentifier name() const {
    return name_;
  }

  virtual int num_args() const = 0;

  virtual bool Invoke(NPObject* object,
                      const NPVariant* args,
                      uint32_t num_args,
                      NPVariant* result) = 0;

 private:
  BaseNPDispatcher* next_;
  NPIdentifier name_;
  DISALLOW_COPY_AND_ASSIGN(BaseNPDispatcher);
};

bool DispatcherHasMethodHelper(BaseNPDispatcher* chain,
                               NPObject* object,
                               NPIdentifier name);

bool DispatcherInvokeHelper(BaseNPDispatcher* chain,
                            NPObject* object,
                            NPIdentifier name,
                            const NPVariant* args,
                            uint32_t num_args,
                            NPVariant* result);

bool DispatcherEnumerateHelper(BaseNPDispatcher* chain,
                               NPObject* object,
                               NPIdentifier** names,
                               uint32_t* num_names);

// This class should never be instantiated. It is always specialized. Attempting
// to instantiate it results in a compilation error. This might mean an
// attempt to instantiate a dispatcher with more parameters than have been
// specialized for. See the specialization code below.
template <typename NPObjectType, typename FunctionType>
struct NPDispatcher {
};

#define TO_NPVARIANT(index)                                                    \
  T##index n##index;                                                           \
  if (!NPVariantToValue(&n##index, args[index]))                               \
    return false;                                                              \

#define NUM_PARAMS 0
#define PARAM_TYPENAMES
#define PARAM_TYPES
#define PARAM_NAMES
#define PARAM_DECLS  // NOLINT

#define PARAM_TO_NVPARIANT_CONVERSIONS                                         \

#include "o3d/gpu_plugin/np_utils/np_dispatcher_specializations.h"  // NOLINT


#define NUM_PARAMS 1
#define PARAM_TYPENAMES , typename T0
#define PARAM_TYPES T0
#define PARAM_NAMES n0
#define PARAM_DECLS T0 n0;  // NOLINT

#define PARAM_TO_NVPARIANT_CONVERSIONS                                         \
  TO_NPVARIANT(0);                                                             \

#include "o3d/gpu_plugin/np_utils/np_dispatcher_specializations.h"  // NOLINT


#define NUM_PARAMS 2
#define PARAM_TYPENAMES , typename T0, typename T1
#define PARAM_TYPES T0, T1
#define PARAM_NAMES n0, n1
#define PARAM_DECLS T0 n0; T1 n1;  // NOLINT

#define PARAM_TO_NVPARIANT_CONVERSIONS                                         \
  TO_NPVARIANT(0);                                                             \
  TO_NPVARIANT(1);                                                             \

#include "o3d/gpu_plugin/np_utils/np_dispatcher_specializations.h"  // NOLINT


#define NUM_PARAMS 3
#define PARAM_TYPENAMES , typename T0, typename T1, typename T2
#define PARAM_TYPES T0, T1, T2
#define PARAM_NAMES n0, n1, n2
#define PARAM_DECLS T0 n0; T1 n1; T2 n2;  // NOLINT

#define PARAM_TO_NVPARIANT_CONVERSIONS                                         \
  TO_NPVARIANT(0);                                                             \
  TO_NPVARIANT(1);                                                             \
  TO_NPVARIANT(2);                                                             \

#include "o3d/gpu_plugin/np_utils/np_dispatcher_specializations.h"  // NOLINT


#define NUM_PARAMS 4
#define PARAM_TYPENAMES , typename T0, typename T1, typename T2, typename T3
#define PARAM_TYPES T0, T1, T2, T3
#define PARAM_NAMES n0, n1, n2, n3
#define PARAM_DECLS T0 n0; T1 n1; T2 n2; T3 n3;  // NOLINT

#define PARAM_TO_NVPARIANT_CONVERSIONS                                         \
  TO_NPVARIANT(0);                                                             \
  TO_NPVARIANT(1);                                                             \
  TO_NPVARIANT(2);                                                             \
  TO_NPVARIANT(3);                                                             \

#include "o3d/gpu_plugin/np_utils/np_dispatcher_specializations.h"  // NOLINT


#define NUM_PARAMS 5
#define PARAM_TYPENAMES , typename T0, typename T1, typename T2, typename T3,  \
    typename T4
#define PARAM_TYPES T0, T1, T2, T3, T4
#define PARAM_NAMES n0, n1, n2, n3, n4
#define PARAM_DECLS T0 n0; T1 n1; T2 n2; T3 n3; T4 n4;  // NOLINT

#define PARAM_TO_NVPARIANT_CONVERSIONS                                         \
  TO_NPVARIANT(0);                                                             \
  TO_NPVARIANT(1);                                                             \
  TO_NPVARIANT(2);                                                             \
  TO_NPVARIANT(3);                                                             \
  TO_NPVARIANT(4);                                                             \

#include "o3d/gpu_plugin/np_utils/np_dispatcher_specializations.h"  // NOLINT


#undef TO_NPVARIANT

}  // namespace gpu_plugin

#endif  // O3D_GPU_PLUGIN_NP_UTILS_NP_DISPATCHER_H_
