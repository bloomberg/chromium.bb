// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "o3d/gpu_plugin/np_utils/np_utils.h"
#include "third_party/npapi/bindings/npapi.h"
#include "third_party/npapi/bindings/npruntime.h"

namespace o3d {
namespace gpu_plugin {

bool NPVariantToValue(bool* value, const NPVariant& variant) {
  if (NPVARIANT_IS_BOOLEAN(variant)) {
    *value = NPVARIANT_TO_BOOLEAN(variant);
    return true;
  }

  return false;
}

bool NPVariantToValue(int* value, const NPVariant& variant) {
  if (NPVARIANT_IS_INT32(variant)) {
    *value = NPVARIANT_TO_INT32(variant);
    return true;
  }

  return false;
}

bool NPVariantToValue(float* value, const NPVariant& variant) {
  if (NPVARIANT_IS_DOUBLE(variant)) {
    *value = static_cast<float>(NPVARIANT_TO_DOUBLE(variant));
    return true;
  } else if (NPVARIANT_IS_INT32(variant)) {
    *value = static_cast<float>(NPVARIANT_TO_INT32(variant));
    return true;
  }

  return false;
}

bool NPVariantToValue(double* value, const NPVariant& variant) {
  if (NPVARIANT_IS_DOUBLE(variant)) {
    *value = NPVARIANT_TO_DOUBLE(variant);
    return true;
  } else if (NPVARIANT_IS_INT32(variant)) {
    *value = NPVARIANT_TO_INT32(variant);
    return true;
  }

  return false;
}

bool NPVariantToValue(std::string* value, const NPVariant& variant) {
  if (NPVARIANT_IS_STRING(variant)) {
    const NPString& str = NPVARIANT_TO_STRING(variant);
    *value = std::string(str.UTF8Characters, str.UTF8Length);
    return true;
  }

  return false;
}

void ValueToNPVariant(bool value, NPVariant* variant) {
  BOOLEAN_TO_NPVARIANT(value, *variant);
}

void ValueToNPVariant(int value, NPVariant* variant) {
  INT32_TO_NPVARIANT(value, *variant);
}

void ValueToNPVariant(float value, NPVariant* variant) {
  DOUBLE_TO_NPVARIANT(value, *variant);
}

void ValueToNPVariant(double value, NPVariant* variant) {
  DOUBLE_TO_NPVARIANT(value, *variant);
}

void ValueToNPVariant(const std::string& value, NPVariant* variant) {
  NPUTF8* p = static_cast<NPUTF8*>(NPBrowser::get()->MemAlloc(value.length()));
  memcpy(p, value.c_str(), value.length());
  STRINGN_TO_NPVARIANT(p, value.length(), *variant);
}

SmartNPVariant::SmartNPVariant() {
  VOID_TO_NPVARIANT(*this);
}

SmartNPVariant::SmartNPVariant(const SmartNPVariant& rhs) {
  Copy(rhs);
}

SmartNPVariant::~SmartNPVariant() {
  Release();
}

SmartNPVariant& SmartNPVariant::operator=(const SmartNPVariant& rhs) {
  Release();
  Copy(rhs);
  return *this;
}

void SmartNPVariant::Release() {
  NPBrowser::get()->ReleaseVariantValue(this);
  VOID_TO_NPVARIANT(*this);
}

void SmartNPVariant::Copy(const NPVariant& rhs) {
  if (NPVARIANT_IS_OBJECT(rhs)) {
    NPObject* object = NPVARIANT_TO_OBJECT(rhs);
    OBJECT_TO_NPVARIANT(object, *this);
    NPBrowser::get()->RetainObject(object);
  } else if (NPVARIANT_IS_STRING(rhs)) {
    NPUTF8* copy = static_cast<NPUTF8*>(NPBrowser::get()->MemAlloc(
        rhs.value.stringValue.UTF8Length));
    memcpy(copy,
           rhs.value.stringValue.UTF8Characters,
           rhs.value.stringValue.UTF8Length);
    STRINGN_TO_NPVARIANT(copy, rhs.value.stringValue.UTF8Length, *this);
  } else {
    memcpy(this, &rhs, sizeof(rhs));
  }
}

bool NPHasMethod(NPP npp,
                 const NPObjectPointer<NPObject>& object,
                 const NPUTF8* name) {
  return NPBrowser::get()->HasMethod(
      npp,
      object.Get(),
      NPBrowser::get()->GetStringIdentifier(name));
}

bool NPHasProperty(NPP npp,
                   const NPObjectPointer<NPObject>& object,
                   const NPUTF8* name) {
  return NPBrowser::get()->HasProperty(
      npp,
      object.Get(),
      NPBrowser::get()->GetStringIdentifier(name));
}

}  // namespace gpu_plugin
}  // namespace o3d
