// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/plugins/ppapi/npapi_glue.h"

#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/string_util.h"
#include "webkit/plugins/ppapi/plugin_module.h"
#include "webkit/plugins/ppapi/plugin_object.h"
#include "webkit/plugins/ppapi/ppapi_plugin_instance.h"
#include "webkit/plugins/ppapi/var.h"
#include "third_party/npapi/bindings/npapi.h"
#include "third_party/npapi/bindings/npruntime.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebBindings.h"

using WebKit::WebBindings;

namespace webkit {
namespace ppapi {

namespace {

const char kInvalidPluginValue[] = "Error: Plugin returned invalid value.";

}  // namespace

// Utilities -------------------------------------------------------------------

bool PPVarToNPVariant(PP_Var var, NPVariant* result) {
  switch (var.type) {
    case PP_VARTYPE_UNDEFINED:
      VOID_TO_NPVARIANT(*result);
      break;
    case PP_VARTYPE_NULL:
      NULL_TO_NPVARIANT(*result);
      break;
    case PP_VARTYPE_BOOL:
      BOOLEAN_TO_NPVARIANT(var.value.as_bool, *result);
      break;
    case PP_VARTYPE_INT32:
      INT32_TO_NPVARIANT(var.value.as_int, *result);
      break;
    case PP_VARTYPE_DOUBLE:
      DOUBLE_TO_NPVARIANT(var.value.as_double, *result);
      break;
    case PP_VARTYPE_STRING: {
      scoped_refptr<StringVar> string(StringVar::FromPPVar(var));
      if (!string) {
        VOID_TO_NPVARIANT(*result);
        return false;
      }
      const std::string& value = string->value();
      char* c_string = static_cast<char*>(malloc(value.size()));
      memcpy(c_string, value.data(), value.size());
      STRINGN_TO_NPVARIANT(c_string, value.size(), *result);
      break;
    }
    case PP_VARTYPE_OBJECT: {
      scoped_refptr<ObjectVar> object(ObjectVar::FromPPVar(var));
      if (!object) {
        VOID_TO_NPVARIANT(*result);
        return false;
      }
      OBJECT_TO_NPVARIANT(WebBindings::retainObject(object->np_object()),
                          *result);
      break;
    }
    case PP_VARTYPE_ARRAY:
    case PP_VARTYPE_DICTIONARY:
      VOID_TO_NPVARIANT(*result);
      break;
  }
  return true;
}

// PPResultAndExceptionToNPResult ----------------------------------------------

PPResultAndExceptionToNPResult::PPResultAndExceptionToNPResult(
    NPObject* object_var,
    NPVariant* np_result)
    : object_var_(object_var),
      np_result_(np_result),
      exception_(PP_MakeUndefined()),
      success_(false),
      checked_exception_(false) {
}

PPResultAndExceptionToNPResult::~PPResultAndExceptionToNPResult() {
  // The user should have called SetResult or CheckExceptionForNoResult
  // before letting this class go out of scope, or the exception will have
  // been lost.
  DCHECK(checked_exception_);

  ObjectVar::PluginReleasePPVar(exception_);
}

// Call this with the return value of the PPAPI function. It will convert
// the result to the NPVariant output parameter and pass any exception on to
// the JS engine. It will update the success flag and return it.
bool PPResultAndExceptionToNPResult::SetResult(PP_Var result) {
  DCHECK(!checked_exception_);  // Don't call more than once.
  DCHECK(np_result_);  // Should be expecting a result.

  checked_exception_ = true;

  if (has_exception()) {
    ThrowException();
    success_ = false;
  } else if (!PPVarToNPVariant(result, np_result_)) {
    WebBindings::setException(object_var_, kInvalidPluginValue);
    success_ = false;
  } else {
    success_ = true;
  }

  // No matter what happened, we need to release the reference to the
  // value passed in. On success, a reference to this value will be in
  // the np_result_.
  Var::PluginReleasePPVar(result);
  return success_;
}

// Call this after calling a PPAPI function that could have set the
// exception. It will pass the exception on to the JS engine and update
// the success flag.
//
// The success flag will be returned.
bool PPResultAndExceptionToNPResult::CheckExceptionForNoResult() {
  DCHECK(!checked_exception_);  // Don't call more than once.
  DCHECK(!np_result_);  // Can't have a result when doing this.

  checked_exception_ = true;

  if (has_exception()) {
    ThrowException();
    success_ = false;
    return false;
  }
  success_ = true;
  return true;
}

// Call this to ignore any exception. This prevents the DCHECK from failing
// in the destructor.
void PPResultAndExceptionToNPResult::IgnoreException() {
  checked_exception_ = true;
}

// Throws the current exception to JS. The exception must be set.
void PPResultAndExceptionToNPResult::ThrowException() {
  scoped_refptr<StringVar> string(StringVar::FromPPVar(exception_));
  if (string) {
    WebBindings::setException(object_var_, string->value().c_str());
  }
}

// PPVarArrayFromNPVariantArray ------------------------------------------------

PPVarArrayFromNPVariantArray::PPVarArrayFromNPVariantArray(
    PluginInstance* instance,
    size_t size,
    const NPVariant* variants)
    : size_(size) {
  if (size_ > 0) {
    array_.reset(new PP_Var[size_]);
    for (size_t i = 0; i < size_; i++)
      array_[i] = Var::NPVariantToPPVar(instance, &variants[i]);
  }
}

PPVarArrayFromNPVariantArray::~PPVarArrayFromNPVariantArray() {
  for (size_t i = 0; i < size_; i++)
    Var::PluginReleasePPVar(array_[i]);
}

// PPVarFromNPObject -----------------------------------------------------------

PPVarFromNPObject::PPVarFromNPObject(PluginInstance* instance, NPObject* object)
    : var_(ObjectVar::NPObjectToPPVar(instance, object)) {
}

PPVarFromNPObject::~PPVarFromNPObject() {
  Var::PluginReleasePPVar(var_);
}

// NPObjectAccessorWithIdentifier ----------------------------------------------

NPObjectAccessorWithIdentifier::NPObjectAccessorWithIdentifier(
    NPObject* object,
    NPIdentifier identifier,
    bool allow_integer_identifier)
    : object_(PluginObject::FromNPObject(object)),
      identifier_(PP_MakeUndefined()) {
  if (object_) {
    identifier_ = Var::NPIdentifierToPPVar(
        object_->instance()->module()->pp_module(), identifier);
    if (identifier_.type == PP_VARTYPE_INT32 && !allow_integer_identifier)
      identifier_.type = PP_VARTYPE_UNDEFINED;  // Mark it invalid.
  }
}

NPObjectAccessorWithIdentifier::~NPObjectAccessorWithIdentifier() {
  Var::PluginReleasePPVar(identifier_);
}

}  // namespace ppapi
}  // namespace webkit
