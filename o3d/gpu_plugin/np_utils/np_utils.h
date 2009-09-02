// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef O3D_GPU_PLUGIN_NP_UTILS_NP_UTILS_H_
#define O3D_GPU_PLUGIN_NP_UTILS_NP_UTILS_H_

#include <string>

#include "o3d/gpu_plugin/np_utils/np_browser.h"
#include "o3d/gpu_plugin/np_utils/np_object_pointer.h"
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

template <typename T>
bool NPVariantToValue(NPObjectPointer<T>* value,
                      const NPVariant& variant) {
  if (NPVARIANT_IS_NULL(variant)) {
    *value = NPObjectPointer<T>();
    return true;
  } else if (NPVARIANT_IS_OBJECT(variant)) {
    NPObject* object = NPVARIANT_TO_OBJECT(variant);
    if (object->_class == BaseNPObject::GetNPClass<T>()) {
      *value = NPObjectPointer<T>(static_cast<T*>(
          NPVARIANT_TO_OBJECT(variant)));
      return true;
    }
  }

  return false;
}

// Specialization for NPObject does not check for mismatched NPClass.
template <>
inline bool NPVariantToValue(NPObjectPointer<NPObject>* value,
                             const NPVariant& variant) {
  if (NPVARIANT_IS_NULL(variant)) {
    *value = NPObjectPointer<NPObject>();
    return true;
  } else if (NPVARIANT_IS_OBJECT(variant)) {
    *value = NPObjectPointer<NPObject>(NPVARIANT_TO_OBJECT(variant));
    return true;
  }

  return false;
}

// Convert C++ type to NPVariant.
void ValueToNPVariant(bool value, NPVariant* variant);
void ValueToNPVariant(int value, NPVariant* variant);
void ValueToNPVariant(float value, NPVariant* variant);
void ValueToNPVariant(double value, NPVariant* variant);
void ValueToNPVariant(const std::string& value, NPVariant* variant);

template <typename T>
void ValueToNPVariant(const NPObjectPointer<T>& value,
                      NPVariant* variant) {
  if (value.Get()) {
    NPBrowser::get()->RetainObject(value.Get());
    OBJECT_TO_NPVARIANT(value.Get(), *variant);
  } else {
    NULL_TO_NPVARIANT(*variant);
  }
}

// NPVariant that automatically manages lifetime of string and object variants.
class SmartNPVariant : public NPVariant {
 public:
  SmartNPVariant();
  SmartNPVariant(const SmartNPVariant& rhs);

  template <typename T>
  explicit SmartNPVariant(const T& v) {
    ValueToNPVariant(v, this);
  }

  ~SmartNPVariant();

  SmartNPVariant& operator=(const SmartNPVariant& rhs);

  template <typename T>
  bool GetValue(T* v) const {
    return NPVariantToValue(v, *this);
  }

  template <typename T>
  void SetValue(const T& v) {
    Release();
    ValueToNPVariant(v, this);
  }

  void Release();

 private:
  void Copy(const NPVariant& rhs);
};

// These allow a method to be invoked with automatic conversion of C++
// types to variants for arguments and return values.

bool NPHasMethod(NPP npp,
                 const NPObjectPointer<NPObject>& object,
                 const NPUTF8* name);

inline bool NPInvokeVoid(NPP npp,
                         const NPObjectPointer<NPObject>& object,
                         const NPUTF8* name) {
  SmartNPVariant result;
  return NPBrowser::get()->Invoke(
      npp,
      object.Get(),
      NPBrowser::get()->GetStringIdentifier(name),
      NULL, 0,
      &result);
}

template<typename R>
bool NPInvoke(NPP npp,
              const NPObjectPointer<NPObject>& object,
              const NPUTF8* name,
              R* r) {
  SmartNPVariant result;
  if (NPBrowser::get()->Invoke(
      npp,
      object.Get(),
      NPBrowser::get()->GetStringIdentifier(name),
      NULL, 0,
      &result)) {
    return result.GetValue(r);
  }
  return false;
}

template<typename P0>
bool NPInvokeVoid(NPP npp,
                  const NPObjectPointer<NPObject>& object,
                  const NPUTF8* name,
                  P0 p0) {
  SmartNPVariant args[1];
  args[0].SetValue(p0);
  SmartNPVariant result;
  return NPBrowser::get()->Invoke(
      npp,
      object.Get(),
      NPBrowser::get()->GetStringIdentifier(name),
      &args[0], 1,
      &result);
}

template<typename R, typename P0>
bool NPInvoke(NPP npp,
              const NPObjectPointer<NPObject>& object,
              const NPUTF8* name,
              P0 p0,
              R* r) {
  SmartNPVariant args[1];
  args[0].SetValue(p0);
  SmartNPVariant result;
  if (NPBrowser::get()->Invoke(
      npp,
      object.Get(),
      NPBrowser::get()->GetStringIdentifier(name),
      &args[0], 1,
      &result)) {
    return result.GetValue(r);
  }
  return false;
}

template<typename P0, typename P1>
bool NPInvokeVoid(NPP npp,
                  const NPObjectPointer<NPObject>& object,
                  const NPUTF8* name,
                  P0 p0, P1 p1) {
  SmartNPVariant args[2];
  args[0].SetValue(p0);
  args[1].SetValue(p1);
  SmartNPVariant result;
  return NPBrowser::get()->Invoke(
      npp,
      object.Get(),
      NPBrowser::get()->GetStringIdentifier(name),
      &args[0], 2,
      &result);
}

template<typename R, typename P0, typename P1>
bool NPInvoke(NPP npp,
              const NPObjectPointer<NPObject>& object,
              const NPUTF8* name,
              P0 p0, P1 p1,
              R* r) {
  SmartNPVariant args[2];
  args[0].SetValue(p0);
  args[1].SetValue(p1);
  SmartNPVariant result;
  if (NPBrowser::get()->Invoke(
      npp,
      object.Get(),
      NPBrowser::get()->GetStringIdentifier(name),
      &args[0], 2,
      &result)) {
    return result.GetValue(r);
  }
  return false;
}

bool NPHasProperty(NPP npp,
                   const NPObjectPointer<NPObject>& object,
                   const NPUTF8* name);

template <typename T>
bool NPGetProperty(NPP npp,
                   const NPObjectPointer<NPObject>& object,
                   const NPUTF8* name,
                   T* value) {
  SmartNPVariant result;
  if (NPBrowser::get()->GetProperty(npp,
                                    object.Get(),
                                    NPBrowser::get()->GetStringIdentifier(name),
                                    &result)) {
    return result.GetValue(value);
  }
  return false;
}

template <typename NPObjectType>
NPObjectPointer<NPObjectType> NPCreateObject(NPP npp) {
  const NPClass* np_class = BaseNPObject::GetNPClass<NPObjectType>();
  NPObjectType* object = static_cast<NPObjectType*>(
      NPBrowser::get()->CreateObject(npp, np_class));
  return NPObjectPointer<NPObjectType>::FromReturned(object);
}

}  // namespace gpu_plugin
}  // namespace o3d

#endif  // O3D_GPU_PLUGIN_NP_UTILS_NP_UTILS_H_
