// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef O3D_GPU_PLUGIN_NP_UTILS_BASE_NP_OBJECT_H_
#define O3D_GPU_PLUGIN_NP_UTILS_BASE_NP_OBJECT_H_

#include "third_party/npapi/bindings/npapi.h"
#include "third_party/npapi/bindings/npruntime.h"

namespace o3d {
namespace gpu_plugin {

// This class is a base class for NPObjects implemented in C++. It forwards
// The NPObject calls through to virtual functions implemented by the subclass.
class BaseNPObject : public NPObject {
 public:
  // Returns the NPClass for the concrete BaseNPObject subclass T.
  template <typename NPObjectType>
  static const NPClass* GetNPClass();

  NPP npp() const {
    return npp_;
  }

  // Override these in subclass.
  virtual void Invalidate();

  virtual bool HasMethod(NPIdentifier name);

  virtual bool Invoke(NPIdentifier name,
                      const NPVariant* args,
                      uint32_t num_args,
                      NPVariant* result);

  virtual bool InvokeDefault(const NPVariant* args,
                             uint32_t num_args,
                             NPVariant* result);

  virtual bool HasProperty(NPIdentifier name);

  virtual bool GetProperty(NPIdentifier name, NPVariant* result);

  virtual bool SetProperty(NPIdentifier name, const NPVariant* value);

  virtual bool RemoveProperty(NPIdentifier name);

  virtual bool Enumerate(NPIdentifier** names,
                         uint32_t* count);

  virtual bool Construct(const NPVariant* args,
                         uint32_t num_args,
                         NPVariant* result);

 protected:

  explicit BaseNPObject(NPP npp);
  virtual ~BaseNPObject();

 private:
  // This template version of the NPClass allocate function creates a subclass
  // of BaseNPObject.
  template <typename NPObjectType>
  static NPObject* AllocateImpl(NPP npp, NPClass*) {
    return new NPObjectType(npp);
  }

  // These implementations of the NPClass functions forward to the virtual
  // functions in BaseNPObject.
  static void DeallocateImpl(NPObject* object);

  static void InvalidateImpl(NPObject* object);

  static bool HasMethodImpl(NPObject* object, NPIdentifier name);

  static bool InvokeImpl(NPObject* object,
                         NPIdentifier name,
                         const NPVariant* args,
                         uint32_t num_args,
                         NPVariant* result);

  static bool InvokeDefaultImpl(NPObject* object,
                                const NPVariant* args,
                                uint32_t num_args,
                                NPVariant* result);

  static bool HasPropertyImpl(NPObject* object, NPIdentifier name);

  static bool GetPropertyImpl(NPObject* object,
                              NPIdentifier name,
                              NPVariant* result);

  static bool SetPropertyImpl(NPObject* object,
                              NPIdentifier name,
                              const NPVariant* value);

  static bool RemovePropertyImpl(NPObject* object, NPIdentifier name);

  static bool EnumerateImpl(NPObject* object,
                            NPIdentifier** names,
                            uint32_t* count);

  static bool ConstructImpl(NPObject* object,
                            const NPVariant* args,
                            uint32_t num_args,
                            NPVariant* result);

  NPP npp_;

  DISALLOW_COPY_AND_ASSIGN(BaseNPObject);
};

template <typename NPObjectType>
const NPClass* BaseNPObject::GetNPClass() {
  static NPClass np_class = {
    NP_CLASS_STRUCT_VERSION,
    AllocateImpl<NPObjectType>,
    DeallocateImpl,
    InvalidateImpl,
    HasMethodImpl,
    InvokeImpl,
    InvokeDefaultImpl,
    HasPropertyImpl,
    GetPropertyImpl,
    SetPropertyImpl,
    RemovePropertyImpl,
    EnumerateImpl,
    ConstructImpl,
  };
  return &np_class;
};
}  // namespace gpu_plugin
}  // namespace o3d

#endif  // O3D_GPU_PLUGIN_NP_UTILS_BASE_NP_OBJECT_H_
