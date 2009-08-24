// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "o3d/gpu_plugin/np_utils/np_variant_utils.h"
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

bool NPVariantToValue(NPObject** value, const NPVariant& variant) {
  if (NPVARIANT_IS_NULL(variant)) {
    *value = NULL;
    return true;
  } else if (NPVARIANT_IS_OBJECT(variant)) {
    *value = NPVARIANT_TO_OBJECT(variant);
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
  NPUTF8* p = static_cast<NPUTF8*>(NPN_MemAlloc(value.length()));
  memcpy(p, value.c_str(), value.length());
  STRINGN_TO_NPVARIANT(p, value.length(), *variant);
}

void ValueToNPVariant(NPObject* value, NPVariant* variant) {
  if (value) {
    NPN_RetainObject(value);
    OBJECT_TO_NPVARIANT(value, *variant);
  } else {
    NULL_TO_NPVARIANT(*variant);
  }
}

}  // namespace gpu_plugin
}  // namespace o3d
