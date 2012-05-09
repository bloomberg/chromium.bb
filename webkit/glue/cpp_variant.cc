// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains definitions for CppVariant.

#include <limits>
#include "third_party/WebKit/Source/WebKit/chromium/public/WebBindings.h"
#include "webkit/glue/cpp_variant.h"
#include "base/logging.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"

using WebKit::WebBindings;

namespace webkit_glue {

CppVariant::CppVariant() {
  type = NPVariantType_Null;
}

// Note that Set() performs a deep copy, which is necessary to safely
// call FreeData() on the value in the destructor.
CppVariant::CppVariant(const CppVariant& original) {
  type = NPVariantType_Null;
  Set(original);
}

// See comment for copy constructor, above.
CppVariant& CppVariant::operator=(const CppVariant& original) {
  if (&original != this)
    Set(original);
  return *this;
}

CppVariant::~CppVariant() {
  FreeData();
}

void CppVariant::FreeData() {
  WebBindings::releaseVariantValue(this);
}

bool CppVariant::isEqual(const CppVariant& other) const {
  if (type != other.type)
    return false;

  switch (type) {
    case NPVariantType_Bool: {
      return (value.boolValue == other.value.boolValue);
    }
    case NPVariantType_Int32: {
      return (value.intValue == other.value.intValue);
    }
    case NPVariantType_Double: {
      return (value.doubleValue == other.value.doubleValue);
    }
    case NPVariantType_String: {
      const NPString *this_value = &value.stringValue;
      const NPString *other_value = &other.value.stringValue;
      uint32_t len = this_value->UTF8Length;
      return (len == other_value->UTF8Length &&
              !strncmp(this_value->UTF8Characters, other_value->UTF8Characters,
                       len));
    }
    case NPVariantType_Null:
    case NPVariantType_Void: {
      return true;
    }
    case NPVariantType_Object: {
      NPObject *this_value = value.objectValue;
      NPObject *other_value = other.value.objectValue;
      return (this_value->_class == other_value->_class &&
              this_value->referenceCount == other_value->referenceCount);
    }
  }
  return false;
}

void CppVariant::CopyToNPVariant(NPVariant* result) const {
  result->type = type;
  switch (type) {
    case NPVariantType_Bool:
      result->value.boolValue = value.boolValue;
      break;
    case NPVariantType_Int32:
      result->value.intValue = value.intValue;
      break;
    case NPVariantType_Double:
      result->value.doubleValue = value.doubleValue;
      break;
    case NPVariantType_String:
      WebBindings::initializeVariantWithStringCopy(result, &value.stringValue);
      break;
    case NPVariantType_Null:
    case NPVariantType_Void:
      // Nothing to set.
      break;
    case NPVariantType_Object:
      result->type = NPVariantType_Object;
      result->value.objectValue = WebBindings::retainObject(value.objectValue);
      break;
  }
}

void CppVariant::Set(const NPVariant& new_value) {
  FreeData();
  switch (new_value.type) {
    case NPVariantType_Bool:
      Set(new_value.value.boolValue);
      break;
    case NPVariantType_Int32:
      Set(new_value.value.intValue);
      break;
    case NPVariantType_Double:
      Set(new_value.value.doubleValue);
      break;
    case NPVariantType_String:
      Set(new_value.value.stringValue);
      break;
    case NPVariantType_Null:
    case NPVariantType_Void:
      type = new_value.type;
      break;
    case NPVariantType_Object:
      Set(new_value.value.objectValue);
      break;
  }
}

void CppVariant::SetNull() {
  FreeData();
  type = NPVariantType_Null;
}

void CppVariant::Set(bool new_value) {
  FreeData();
  type = NPVariantType_Bool;
  value.boolValue = new_value;
}

void CppVariant::Set(int32_t new_value) {
  FreeData();
  type = NPVariantType_Int32;
  value.intValue = new_value;
}

void CppVariant::Set(double new_value) {
  FreeData();
  type = NPVariantType_Double;
  value.doubleValue = new_value;
}

// The new_value must be a null-terminated string.
void CppVariant::Set(const char* new_value) {
  FreeData();
  type = NPVariantType_String;
  NPString new_string = {new_value,
                         static_cast<uint32_t>(strlen(new_value))};
  WebBindings::initializeVariantWithStringCopy(this, &new_string);
}

void CppVariant::Set(const std::string& new_value) {
  FreeData();
  type = NPVariantType_String;
  NPString new_string = {new_value.data(),
                         static_cast<uint32_t>(new_value.size())};
  WebBindings::initializeVariantWithStringCopy(this, &new_string);
}
void CppVariant::Set(const NPString& new_value) {
  FreeData();
  type = NPVariantType_String;
  WebBindings::initializeVariantWithStringCopy(this, &new_value);
}

void CppVariant::Set(NPObject* new_value) {
  FreeData();
  type = NPVariantType_Object;
  value.objectValue = WebBindings::retainObject(new_value);
}

std::string CppVariant::ToString() const {
  DCHECK(isString());
  return std::string(value.stringValue.UTF8Characters,
                     value.stringValue.UTF8Length);
}

int32_t CppVariant::ToInt32() const {
  if (isInt32()) {
    return value.intValue;
  } else if (isDouble()) {
    return static_cast<int32_t>(value.doubleValue);
  } else {
    NOTREACHED();
    return 0;
  }
}

double CppVariant::ToDouble() const {
  if (isInt32()) {
    return static_cast<double>(value.intValue);
  } else if (isDouble()) {
    return value.doubleValue;
  } else {
    NOTREACHED();
    return 0.0;
  }
}

bool CppVariant::ToBoolean() const {
  DCHECK(isBool());
  return value.boolValue;
}

std::vector<CppVariant> CppVariant::ToVector() const {
  DCHECK(isObject());
  std::vector<CppVariant> vector;
  NPObject* np_value = value.objectValue;
  NPIdentifier length_id = WebBindings::getStringIdentifier("length");

  if (WebBindings::hasProperty(NULL, np_value, length_id)) {
    CppVariant length_value;
    if (WebBindings::getProperty(NULL, np_value, length_id, &length_value)) {
      int length = 0;
      // The length is a double in some cases.
      if (NPVARIANT_IS_DOUBLE(length_value))
        length = static_cast<int>(NPVARIANT_TO_DOUBLE(length_value));
      else if (NPVARIANT_IS_INT32(length_value))
        length = NPVARIANT_TO_INT32(length_value);
      else
        NOTREACHED();

      // For sanity, only allow 60000 items.
      length = std::min(60000, length);
      for (int i = 0; i < length; ++i) {
        // Get each of the items.
        NPIdentifier index = WebBindings::getIntIdentifier(i);
        if (WebBindings::hasProperty(NULL, np_value, index)) {
          CppVariant index_value;
          if (WebBindings::getProperty(NULL, np_value, index, &index_value))
            vector.push_back(index_value);
        }
      }
    }
  } else {
    NOTREACHED();
  }
  return vector;
}

bool CppVariant::Invoke(const std::string& method, const CppVariant* args,
                        uint32 arg_count, CppVariant& result) const {
  DCHECK(isObject());
  NPIdentifier method_name = WebBindings::getStringIdentifier(method.c_str());
  NPObject* np_object = value.objectValue;
  if (WebBindings::hasMethod(NULL, np_object, method_name)) {
    NPVariant r;
    bool status = WebBindings::invoke(NULL, np_object, method_name, args, arg_count, &r);
    result.Set(r);
    return status;
  } else {
    return false;
  }
}

}  // namespace webkit_glue
