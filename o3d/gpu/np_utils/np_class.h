// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_NP_UTILS_NP_CLASS_H_
#define GPU_NP_UTILS_NP_CLASS_H_

#include "gpu/np_utils/np_object_pointer.h"
#include "gpu/np_utils/np_headers.h"

// This file implements NPGetClass<T>. This function returns an NPClass
// that can be used to instantiate an NPObject subclass T. The NPClass
// function pointers will invoke the most derived corresponding member
// functions in T.

namespace gpu_plugin {

namespace np_class_impl {
  // This template version of the NPClass allocate function creates a subclass
  // of BaseNPObject.
  template <typename NPObjectType>
  static NPObject* Allocate(NPP npp, NPClass*) {
    return new NPObjectType(npp);
  }

  // These implementations of the NPClass functions forward to the virtual
  // functions in DefaultNPObject.
  template <typename NPObjectType>
  static void Deallocate(NPObject* object) {
    delete static_cast<NPObjectType*>(object);
  }

  template <typename NPObjectType>
  static void Invalidate(NPObject* object) {
    return static_cast<NPObjectType*>(object)->Invalidate();
  }

  template <typename NPObjectType>
  static bool HasMethod(NPObject* object, NPIdentifier name) {
    return static_cast<NPObjectType*>(object)->HasMethod(name);
  }

  template <typename NPObjectType>
  static bool Invoke(NPObject* object,
                     NPIdentifier name,
                     const NPVariant* args,
                     uint32_t num_args,
                     NPVariant* result) {
    return static_cast<NPObjectType*>(object)->Invoke(
        name, args, num_args, result);
  }

  template <typename NPObjectType>
  static bool InvokeDefault(NPObject* object,
                            const NPVariant* args,
                            uint32_t num_args,
                            NPVariant* result) {
    return static_cast<NPObjectType*>(object)->InvokeDefault(
        args, num_args, result);
  }

  template <typename NPObjectType>
  static bool HasProperty(NPObject* object, NPIdentifier name) {
    return static_cast<NPObjectType*>(object)->HasProperty(name);
  }

  template <typename NPObjectType>
  static bool GetProperty(NPObject* object,
                          NPIdentifier name,
                          NPVariant* result) {
    return static_cast<NPObjectType*>(object)->GetProperty(name, result);
  }

  template <typename NPObjectType>
  static bool SetProperty(NPObject* object,
                              NPIdentifier name,
                              const NPVariant* value) {
    return static_cast<NPObjectType*>(object)->SetProperty(name, value);
  }

  template <typename NPObjectType>
  static bool RemoveProperty(NPObject* object, NPIdentifier name) {
    return static_cast<NPObjectType*>(object)->RemoveProperty(name);
  }

  template <typename NPObjectType>
  static bool Enumerate(NPObject* object,
                            NPIdentifier** names,
                            uint32_t* count) {
    return static_cast<NPObjectType*>(object)->Enumerate(names, count);
  };

  template <typename NPObjectType>
  static bool Construct(NPObject* object,
                            const NPVariant* args,
                            uint32_t num_args,
                            NPVariant* result) {
    return static_cast<NPObjectType*>(object)->Construct(
        args, num_args, result);
  }
}  // namespace np_class_impl;

template <typename NPObjectType>
const NPClass* NPGetClass() {
  static const NPClass np_class = {
    NP_CLASS_STRUCT_VERSION,
    np_class_impl::Allocate<NPObjectType>,
    np_class_impl::Deallocate<NPObjectType>,
    np_class_impl::Invalidate<NPObjectType>,
    np_class_impl::HasMethod<NPObjectType>,
    np_class_impl::Invoke<NPObjectType>,
    np_class_impl::InvokeDefault<NPObjectType>,
    np_class_impl::HasProperty<NPObjectType>,
    np_class_impl::GetProperty<NPObjectType>,
    np_class_impl::SetProperty<NPObjectType>,
    np_class_impl::RemoveProperty<NPObjectType>,
    np_class_impl::Enumerate<NPObjectType>,
    np_class_impl::Construct<NPObjectType>,
  };
  return &np_class;
};

}  // namespace gpu_plugin

#endif  // GPU_NP_UTILS_NP_CLASS_H_
