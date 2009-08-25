// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef O3D_GPU_PLUGIN_NP_UTILS_NP_VARIANT_UTILS_H_
#define O3D_GPU_PLUGIN_NP_UTILS_NP_VARIANT_UTILS_H_

#include <string>

#include "o3d/gpu_plugin/np_utils/npn_funcs.h"
#include "third_party/npapi/bindings/npapi.h"
#include "third_party/npapi/bindings/npruntime.h"

namespace o3d {
namespace gpu_plugin {

// Convert NPVariant to C++ type. Returns whether the conversion was successful.
bool NPVariantToValue(bool* value, const NPVariant& variant);
bool NPVariantToValue(int* value, const NPVariant& variant);
bool NPVariantToValue(float* value, const NPVariant& variant);
bool NPVariantToValue(double* value, const NPVariant& variant);
bool NPVariantToValue(std::string* value, const NPVariant& variant);
bool NPVariantToValue(NPObject** value, const NPVariant& variant);

// Convert C++ type to NPVariant.
void ValueToNPVariant(bool value, NPVariant* variant);
void ValueToNPVariant(int value, NPVariant* variant);
void ValueToNPVariant(float value, NPVariant* variant);
void ValueToNPVariant(double value, NPVariant* variant);
void ValueToNPVariant(const std::string& value, NPVariant* variant);
void ValueToNPVariant(NPObject* value, NPVariant* variant);

// NPVariant that autmatically releases in destructor.
class SmartNPVariant {
 public:
  SmartNPVariant() {
    VOID_TO_NPVARIANT(variant_);
  }

  ~SmartNPVariant() {
    Release();
  }

  template <typename T>
  bool GetValue(T* v) const {
    return NPVariantToValue(v, variant_);
  }

  template <typename T>
  void SetValue(const T& v) {
    ValueToNPVariant(v, &variant_);
  }

  void Release() {
    gpu_plugin::NPN_ReleaseVariantValue(&variant_);
    VOID_TO_NPVARIANT(variant_);
  }

  NPVariant& GetVariant() {
    return variant_;
  }

  const NPVariant& GetVariant() const {
    return variant_;
  }

 private:
  NPVariant variant_;
  DISALLOW_COPY_AND_ASSIGN(SmartNPVariant);
};

// These allow a method to be invoked with automatic conversion of C++
// types to variants for arguments and return values.

inline bool NPInvokeVoid(NPP npp, NPObject* object, NPIdentifier name) {
  SmartNPVariant result;
  return gpu_plugin::NPN_Invoke(npp, object, name, NULL, 0,
                                &result.GetVariant());
}

template<typename R>
bool NPInvoke(NPP npp, NPObject* object, NPIdentifier name,
              R* r) {
  SmartNPVariant result;
  if (NPN_Invoke(npp, object, name, NULL, 0,
                 &result.GetVariant())) {
    return result.GetValue(r);
  }
  return false;
}

template<typename P0>
bool NPInvokeVoid(NPP npp, NPObject* object, NPIdentifier name,
                  P0 p0) {
  SmartNPVariant args[1];
  args[0].SetValue(p0);
  SmartNPVariant result;
  return gpu_plugin::NPN_Invoke(npp, object, name, &args[0].GetVariant(), 1,
                                &result.GetVariant());
}

template<typename R, typename P0>
bool NPInvoke(NPP npp, NPObject* object, NPIdentifier name,
              P0 p0, R* r) {
  SmartNPVariant args[1];
  args[0].SetValue(p0);
  SmartNPVariant result;
  if (gpu_plugin::NPN_Invoke(npp, object, name, &args[0].GetVariant(), 1,
                             &result.GetVariant())) {
    return result.GetValue(r);
  }
  return false;
}

template<typename P0, typename P1>
bool NPInvokeVoid(NPP npp, NPObject* object, NPIdentifier name,
                  P0 p0, P1 p1) {
  SmartNPVariant args[2];
  args[0].SetValue(p0);
  args[1].SetValue(p1);
  SmartNPVariant result;
  return gpu_plugin::NPN_Invoke(npp, object, name, &args[0].GetVariant(), 2,
                                &result.GetVariant());
}

template<typename R, typename P0, typename P1>
bool NPInvoke(NPP npp, NPObject* object, NPIdentifier name,
              P0 p0, P1 p1, R* r) {
  SmartNPVariant args[2];
  args[0].SetValue(p0);
  args[1].SetValue(p1);
  SmartNPVariant result;
  if (gpu_plugin::NPN_Invoke(npp, object, name, &args[0].GetVariant(), 2,
                             &result.GetVariant())) {
    return result.GetValue(r);
  }
  return false;
}

}  // namespace gpu_plugin
}  // namespace o3d

#endif  // O3D_GPU_PLUGIN_NP_UTILS_NP_VARIANT_UTILS_H_
