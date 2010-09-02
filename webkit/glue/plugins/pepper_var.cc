// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/glue/plugins/pepper_var.h"

#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "third_party/ppapi/c/pp_var.h"
#include "third_party/ppapi/c/ppb_var.h"
#include "third_party/ppapi/c/ppp_class.h"
#include "third_party/WebKit/WebKit/chromium/public/WebBindings.h"
#include "webkit/glue/plugins/pepper_string.h"
#include "v8/include/v8.h"

using WebKit::WebBindings;

namespace pepper {

namespace {

void Release(PP_Var var);
PP_Var VarFromUtf8(const char* data, uint32_t len);

const char kInvalidObjectException[] = "Error: Invalid object";
const char kInvalidPropertyException[] = "Error: Invalid property";
const char kUnableToGetPropertyException[] = "Error: Unable to get property";
const char kUnableToSetPropertyException[] = "Error: Unable to set property";
const char kUnableToRemovePropertyException[] =
    "Error: Unable to remove property";
const char kUnableToGetAllPropertiesException[] =
    "Error: Unable to get all properties";
const char kUnableToCallMethodException[] = "Error: Unable to call method";
const char kUnableToConstructException[] = "Error: Unable to construct";

// ---------------------------------------------------------------------------
// Utilities

String* GetStringUnchecked(PP_Var var) {
  return reinterpret_cast<String*>(var.value.as_id);
}

NPObject* GetNPObjectUnchecked(PP_Var var) {
  return reinterpret_cast<NPObject*>(var.value.as_id);
}

// Returns a NPVariant that corresponds to the given PP_Var.  The contents of
// the PP_Var will be copied unless the PP_Var corresponds to an object.
NPVariant PPVarToNPVariant(PP_Var var) {
  NPVariant ret;
  switch (var.type) {
    case PP_VARTYPE_VOID:
      VOID_TO_NPVARIANT(ret);
      break;
    case PP_VARTYPE_NULL:
      NULL_TO_NPVARIANT(ret);
      break;
    case PP_VARTYPE_BOOL:
      BOOLEAN_TO_NPVARIANT(var.value.as_bool, ret);
      break;
    case PP_VARTYPE_INT32:
      INT32_TO_NPVARIANT(var.value.as_int, ret);
      break;
    case PP_VARTYPE_DOUBLE:
      DOUBLE_TO_NPVARIANT(var.value.as_double, ret);
      break;
    case PP_VARTYPE_STRING: {
      const std::string& value = GetStringUnchecked(var)->value();
      STRINGN_TO_NPVARIANT(base::strdup(value.c_str()), value.size(), ret);
      break;
    }
    case PP_VARTYPE_OBJECT: {
      NPObject* object = GetNPObjectUnchecked(var);
      OBJECT_TO_NPVARIANT(WebBindings::retainObject(object), ret);
      break;
    }
  }
  return ret;
}

// Returns a NPVariant that corresponds to the given PP_Var.  The contents of
// the PP_Var will NOT be copied, so you need to ensure that the PP_Var remains
// valid while the resultant NPVariant is in use.
NPVariant PPVarToNPVariantNoCopy(PP_Var var) {
  NPVariant ret;
  switch (var.type) {
    case PP_VARTYPE_VOID:
      VOID_TO_NPVARIANT(ret);
      break;
    case PP_VARTYPE_NULL:
      NULL_TO_NPVARIANT(ret);
      break;
    case PP_VARTYPE_BOOL:
      BOOLEAN_TO_NPVARIANT(var.value.as_bool, ret);
      break;
    case PP_VARTYPE_INT32:
      INT32_TO_NPVARIANT(var.value.as_int, ret);
      break;
    case PP_VARTYPE_DOUBLE:
      DOUBLE_TO_NPVARIANT(var.value.as_double, ret);
      break;
    case PP_VARTYPE_STRING: {
      const std::string& value = GetStringUnchecked(var)->value();
      STRINGN_TO_NPVARIANT(value.c_str(), value.size(), ret);
      break;
    }
    case PP_VARTYPE_OBJECT: {
      OBJECT_TO_NPVARIANT(GetNPObjectUnchecked(var), ret);
      break;
    }
  }
  return ret;
}

// Returns a NPIdentifier that corresponds to the given PP_Var.  The contents
// of the PP_Var will be copied.  Returns NULL if the given PP_Var is not a a
// string or integer type.
NPIdentifier PPVarToNPIdentifier(PP_Var var) {
  switch (var.type) {
    case PP_VARTYPE_STRING:
      return WebBindings::getStringIdentifier(
          GetStringUnchecked(var)->value().c_str());
    case PP_VARTYPE_INT32:
      return WebBindings::getIntIdentifier(var.value.as_int);
    default:
      return NULL;
  }
}

PP_Var NPIdentifierToPPVar(NPIdentifier id) {
  const NPUTF8* string_value = NULL;
  int32_t int_value = 0;
  bool is_string = false;
  WebBindings::extractIdentifierData(id, string_value, int_value, is_string);
  if (is_string)
    return VarFromUtf8(string_value, strlen(string_value));

  return PP_MakeInt32(int_value);
}

PP_Var NPIdentifierToPPVarString(NPIdentifier id) {
  PP_Var var = NPIdentifierToPPVar(id);
  if (var.type == PP_VARTYPE_STRING)
    return var;
  DCHECK(var.type == PP_VARTYPE_INT32);
  const std::string& str = base::IntToString(var.value.as_int);
  return VarFromUtf8(str.data(), str.size());
}

void ThrowException(NPObject* object, PP_Var exception) {
  String* str = GetString(exception);
  if (str)
    WebBindings::setException(object, str->value().c_str());
}

// ---------------------------------------------------------------------------
// NPObject implementation in terms of PPP_Class

struct WrapperObject : NPObject {
  const PPP_Class* ppp_class;
  void* ppp_class_data;
};

static WrapperObject* ToWrapper(NPObject* object) {
  return static_cast<WrapperObject*>(object);
}

NPObject* WrapperClass_Allocate(NPP npp, NPClass* unused) {
  return new WrapperObject;
}

void WrapperClass_Deallocate(NPObject* object) {
  WrapperObject* wrapper = ToWrapper(object);
  wrapper->ppp_class->Deallocate(wrapper->ppp_class_data);
  delete object;
}

void WrapperClass_Invalidate(NPObject* object) {
  // TODO(darin): Do I need to do something here?
}

bool WrapperClass_HasMethod(NPObject* object, NPIdentifier method_name) {
  WrapperObject* wrapper = ToWrapper(object);

  PP_Var method_name_var = NPIdentifierToPPVarString(method_name);
  PP_Var exception = PP_MakeVoid();
  bool rv = wrapper->ppp_class->HasMethod(wrapper->ppp_class_data,
                                          method_name_var,
                                          &exception);
  Release(method_name_var);

  if (exception.type != PP_VARTYPE_VOID) {
    ThrowException(object, exception);
    Release(exception);
    return false;
  }
  return rv;
}

bool WrapperClass_Invoke(NPObject* object, NPIdentifier method_name,
                         const NPVariant* argv, uint32_t argc,
                         NPVariant* result) {
  WrapperObject* wrapper = ToWrapper(object);

  scoped_array<PP_Var> args;
  if (argc) {
    args.reset(new PP_Var[argc]);
    for (uint32_t i = 0; i < argc; ++i)
      args[i] = NPVariantToPPVar(&argv[i]);
  }
  PP_Var method_name_var = NPIdentifierToPPVarString(method_name);
  PP_Var exception = PP_MakeVoid();
  PP_Var result_var = wrapper->ppp_class->Call(wrapper->ppp_class_data,
                                               method_name_var, argc,
                                               args.get(), &exception);
  Release(method_name_var);
  for (uint32_t i = 0; i < argc; ++i)
    Release(args[i]);

  bool rv;
  if (exception.type == PP_VARTYPE_VOID) {
    rv = true;
    *result = PPVarToNPVariant(result_var);
  } else {
    rv = false;
    ThrowException(object, exception);
    Release(exception);
  }
  Release(result_var);
  return rv;
}

bool WrapperClass_InvokeDefault(NPObject* object, const NPVariant* argv,
                                uint32_t argc, NPVariant* result) {
  WrapperObject* wrapper = ToWrapper(object);

  scoped_array<PP_Var> args;
  if (argc) {
    args.reset(new PP_Var[argc]);
    for (uint32_t i = 0; i < argc; ++i)
      args[i] = NPVariantToPPVar(&argv[i]);
  }
  PP_Var exception = PP_MakeVoid();
  PP_Var result_var = wrapper->ppp_class->Call(wrapper->ppp_class_data,
                                               PP_MakeVoid(), argc, args.get(),
                                               &exception);
  for (uint32_t i = 0; i < argc; ++i)
    Release(args[i]);

  bool rv;
  if (exception.type == PP_VARTYPE_VOID) {
    rv = true;
    *result = PPVarToNPVariant(result_var);
  } else {
    rv = false;
    ThrowException(object, exception);
    Release(exception);
  }
  Release(result_var);
  return rv;
}

bool WrapperClass_HasProperty(NPObject* object, NPIdentifier property_name) {
  WrapperObject* wrapper = ToWrapper(object);

  PP_Var property_name_var = NPIdentifierToPPVar(property_name);
  PP_Var exception = PP_MakeVoid();
  bool rv = wrapper->ppp_class->HasProperty(wrapper->ppp_class_data,
                                            property_name_var,
                                            &exception);
  Release(property_name_var);

  if (exception.type != PP_VARTYPE_VOID) {
    ThrowException(object, exception);
    Release(exception);
    return false;
  }
  return rv;
}

bool WrapperClass_GetProperty(NPObject* object, NPIdentifier property_name,
                              NPVariant* result) {
  WrapperObject* wrapper = ToWrapper(object);

  PP_Var property_name_var = NPIdentifierToPPVar(property_name);
  PP_Var exception = PP_MakeVoid();
  PP_Var result_var = wrapper->ppp_class->GetProperty(wrapper->ppp_class_data,
                                                      property_name_var,
                                                      &exception);
  Release(property_name_var);

  bool rv;
  if (exception.type == PP_VARTYPE_VOID) {
    rv = true;
    *result = PPVarToNPVariant(result_var);
  } else {
    rv = false;
    ThrowException(object, exception);
    Release(exception);
  }
  Release(result_var);
  return rv;
}

bool WrapperClass_SetProperty(NPObject* object, NPIdentifier property_name,
                              const NPVariant* value) {
  WrapperObject* wrapper = ToWrapper(object);

  PP_Var property_name_var = NPIdentifierToPPVar(property_name);
  PP_Var value_var = NPVariantToPPVar(value);
  PP_Var exception = PP_MakeVoid();
  wrapper->ppp_class->SetProperty(wrapper->ppp_class_data, property_name_var,
                                  value_var, &exception);
  Release(value_var);
  Release(property_name_var);

  if (exception.type != PP_VARTYPE_VOID) {
    ThrowException(object, exception);
    Release(exception);
    return false;
  }
  return true;
}

bool WrapperClass_RemoveProperty(NPObject* object, NPIdentifier property_name) {
  WrapperObject* wrapper = ToWrapper(object);

  PP_Var property_name_var = NPIdentifierToPPVar(property_name);
  PP_Var exception = PP_MakeVoid();
  wrapper->ppp_class->RemoveProperty(wrapper->ppp_class_data, property_name_var,
                                     &exception);
  Release(property_name_var);

  if (exception.type != PP_VARTYPE_VOID) {
    ThrowException(object, exception);
    Release(exception);
    return false;
  }
  return true;
}

bool WrapperClass_Enumerate(NPObject* object, NPIdentifier** values,
                            uint32_t* count) {
  WrapperObject* wrapper = ToWrapper(object);

  uint32_t property_count = 0;
  PP_Var* properties = NULL;
  PP_Var exception = PP_MakeVoid();
  wrapper->ppp_class->GetAllPropertyNames(wrapper->ppp_class_data,
                                          &property_count,
                                          &properties,
                                          &exception);

  bool rv;
  if (exception.type == PP_VARTYPE_VOID) {
    rv = true;
    if (property_count == 0) {
      *values = NULL;
      *count = 0;
    } else {
      *values = static_cast<NPIdentifier*>(
          malloc(sizeof(NPIdentifier) * property_count));
      *count = property_count;
      for (uint32_t i = 0; i < property_count; ++i)
        (*values)[i] = PPVarToNPIdentifier(properties[i]);
    }
  } else {
    rv = false;
    ThrowException(object, exception);
    Release(exception);
  }

  for (uint32_t i = 0; i < property_count; ++i)
    Release(properties[i]);
  free(properties);
  return rv;
}

bool WrapperClass_Construct(NPObject* object, const NPVariant* argv,
                            uint32_t argc, NPVariant* result) {
  WrapperObject* wrapper = ToWrapper(object);

  scoped_array<PP_Var> args;
  if (argc) {
    args.reset(new PP_Var[argc]);
    for (uint32_t i = 0; i < argc; ++i)
      args[i] = NPVariantToPPVar(&argv[i]);
  }

  PP_Var exception = PP_MakeVoid();
  PP_Var result_var = wrapper->ppp_class->Construct(wrapper->ppp_class_data,
                                                    argc, args.get(),
                                                    &exception);
  for (uint32_t i = 0; i < argc; ++i)
    Release(args[i]);

  bool rv;
  if (exception.type == PP_VARTYPE_VOID) {
    rv = true;
    *result = PPVarToNPVariant(result_var);
  } else {
    rv = false;
    ThrowException(object, exception);
    Release(exception);
  }
  Release(result_var);
  return rv;
}

const NPClass wrapper_class = {
  NP_CLASS_STRUCT_VERSION,
  WrapperClass_Allocate,
  WrapperClass_Deallocate,
  WrapperClass_Invalidate,
  WrapperClass_HasMethod,
  WrapperClass_Invoke,
  WrapperClass_InvokeDefault,
  WrapperClass_HasProperty,
  WrapperClass_GetProperty,
  WrapperClass_SetProperty,
  WrapperClass_RemoveProperty,
  WrapperClass_Enumerate,
  WrapperClass_Construct
};

// ---------------------------------------------------------------------------
// PPB_Var methods

void AddRef(PP_Var var) {
  if (var.type == PP_VARTYPE_STRING) {
    GetStringUnchecked(var)->AddRef();
  } else if (var.type == PP_VARTYPE_OBJECT) {
    // TODO(darin): Add thread safety check
    WebBindings::retainObject(GetNPObjectUnchecked(var));
  }
}

void Release(PP_Var var) {
  if (var.type == PP_VARTYPE_STRING) {
    GetStringUnchecked(var)->Release();
  } else if (var.type == PP_VARTYPE_OBJECT) {
    // TODO(darin): Add thread safety check
    WebBindings::releaseObject(GetNPObjectUnchecked(var));
  }
}

PP_Var VarFromUtf8(const char* data, uint32_t len) {
  scoped_refptr<String> str = new String(data, len);

  if (!str || !IsStringUTF8(str->value())) {
    return PP_MakeNull();
  }

  PP_Var ret;
  ret.type = PP_VARTYPE_STRING;

  // The caller takes ownership now.
  ret.value.as_id = reinterpret_cast<intptr_t>(str.release());

  return ret;
}

const char* VarToUtf8(PP_Var var, uint32_t* len) {
  if (var.type != PP_VARTYPE_STRING) {
    *len = 0;
    return NULL;
  }
  const std::string& str = GetStringUnchecked(var)->value();
  *len = static_cast<uint32_t>(str.size());
  if (str.empty())
    return "";  // Don't return NULL on success.
  return str.data();
}

bool HasProperty(PP_Var var,
                 PP_Var name,
                 PP_Var* exception) {
  TryCatch try_catch(exception);
  if (try_catch.HasException())
    return false;

  NPObject* object = GetNPObject(var);
  if (!object) {
    try_catch.SetException(kInvalidObjectException);
    return false;
  }

  NPIdentifier identifier = PPVarToNPIdentifier(name);
  if (!identifier) {
    try_catch.SetException(kInvalidPropertyException);
    return false;
  }

  return WebBindings::hasProperty(NULL, object, identifier);
}

bool HasMethod(PP_Var var,
               PP_Var name,
               PP_Var* exception) {
  TryCatch try_catch(exception);
  if (try_catch.HasException())
    return false;

  NPObject* object = GetNPObject(var);
  if (!object) {
    try_catch.SetException(kInvalidObjectException);
    return false;
  }

  NPIdentifier identifier = PPVarToNPIdentifier(name);
  if (!identifier) {
    try_catch.SetException(kInvalidPropertyException);
    return false;
  }

  return WebBindings::hasMethod(NULL, object, identifier);
}

PP_Var GetProperty(PP_Var var,
                   PP_Var name,
                   PP_Var* exception) {
  TryCatch try_catch(exception);
  if (try_catch.HasException())
    return PP_MakeVoid();

  NPObject* object = GetNPObject(var);
  if (!object) {
    try_catch.SetException(kInvalidObjectException);
    return PP_MakeVoid();
  }

  NPIdentifier identifier = PPVarToNPIdentifier(name);
  if (!identifier) {
    try_catch.SetException(kInvalidPropertyException);
    return PP_MakeVoid();
  }

  NPVariant result;
  if (!WebBindings::getProperty(NULL, object, identifier, &result)) {
    // An exception may have been raised.
    if (!try_catch.HasException())
      try_catch.SetException(kUnableToGetPropertyException);
    return PP_MakeVoid();
  }

  PP_Var ret = NPVariantToPPVar(&result);
  WebBindings::releaseVariantValue(&result);
  return ret;
}

void GetAllPropertyNames(PP_Var var,
                         uint32_t* property_count,
                         PP_Var** properties,
                         PP_Var* exception) {
  *properties = NULL;
  *property_count = 0;

  TryCatch try_catch(exception);
  if (try_catch.HasException())
    return;

  NPObject* object = GetNPObject(var);
  if (!object) {
    try_catch.SetException(kInvalidObjectException);
    return;
  }

  NPIdentifier* identifiers = NULL;
  uint32_t count = 0;
  if (!WebBindings::enumerate(NULL, object, &identifiers, &count)) {
    if (!try_catch.HasException())
      try_catch.SetException(kUnableToGetAllPropertiesException);
    return;
  }

  if (count == 0)
    return;

  *property_count = count;
  *properties = static_cast<PP_Var*>(malloc(sizeof(PP_Var) * count));
  for (uint32_t i = 0; i < count; ++i)
    (*properties)[i] = NPIdentifierToPPVar(identifiers[i]);
  free(identifiers);
}

void SetProperty(PP_Var var,
                 PP_Var name,
                 PP_Var value,
                 PP_Var* exception) {
  TryCatch try_catch(exception);
  if (try_catch.HasException())
    return;

  NPObject* object = GetNPObject(var);
  if (!object) {
    try_catch.SetException(kInvalidObjectException);
    return;
  }

  NPIdentifier identifier = PPVarToNPIdentifier(name);
  if (!identifier) {
    try_catch.SetException(kInvalidPropertyException);
    return;
  }

  NPVariant variant = PPVarToNPVariantNoCopy(value);
  if (!WebBindings::setProperty(NULL, object, identifier, &variant)) {
    if (!try_catch.HasException())
      try_catch.SetException(kUnableToSetPropertyException);
  }
}

void RemoveProperty(PP_Var var,
                    PP_Var name,
                    PP_Var* exception) {
  TryCatch try_catch(exception);
  if (try_catch.HasException())
    return;

  NPObject* object = GetNPObject(var);
  if (!object) {
    try_catch.SetException(kInvalidObjectException);
    return;
  }

  NPIdentifier identifier = PPVarToNPIdentifier(name);
  if (!identifier) {
    try_catch.SetException(kInvalidPropertyException);
    return;
  }

  if (!WebBindings::removeProperty(NULL, object, identifier)) {
    if (!try_catch.HasException())
      try_catch.SetException(kUnableToRemovePropertyException);
  }
}

PP_Var Call(PP_Var var,
            PP_Var method_name,
            uint32_t argc,
            PP_Var* argv,
            PP_Var* exception) {
  TryCatch try_catch(exception);
  if (try_catch.HasException())
    return PP_MakeVoid();

  NPObject* object = GetNPObject(var);
  if (!object) {
    try_catch.SetException(kInvalidObjectException);
    return PP_MakeVoid();
  }

  NPIdentifier identifier;
  if (method_name.type == PP_VARTYPE_VOID) {
    identifier = NULL;
  } else if (method_name.type == PP_VARTYPE_STRING) {
    // Specifically allow only string functions to be called.
    identifier = PPVarToNPIdentifier(method_name);
    if (!identifier) {
      try_catch.SetException(kInvalidPropertyException);
      return PP_MakeVoid();
    }
  } else {
    try_catch.SetException(kInvalidPropertyException);
    return PP_MakeVoid();
  }

  scoped_array<NPVariant> args;
  if (argc) {
    args.reset(new NPVariant[argc]);
    for (uint32_t i = 0; i < argc; ++i)
      args[i] = PPVarToNPVariantNoCopy(argv[i]);
  }

  bool ok;

  NPVariant result;
  if (identifier) {
    ok = WebBindings::invoke(NULL, object, identifier, args.get(), argc,
                             &result);
  } else {
    ok = WebBindings::invokeDefault(NULL, object, args.get(), argc, &result);
  }

  if (!ok) {
    // An exception may have been raised.
    if (!try_catch.HasException())
      try_catch.SetException(kUnableToCallMethodException);
    return PP_MakeVoid();
  }

  PP_Var ret = NPVariantToPPVar(&result);
  WebBindings::releaseVariantValue(&result);
  return ret;
}

PP_Var Construct(PP_Var var,
                 uint32_t argc,
                 PP_Var* argv,
                 PP_Var* exception) {
  TryCatch try_catch(exception);
  if (try_catch.HasException())
    return PP_MakeVoid();

  NPObject* object = GetNPObject(var);
  if (!object) {
    try_catch.SetException(kInvalidObjectException);
    return PP_MakeVoid();
  }

  scoped_array<NPVariant> args;
  if (argc) {
    args.reset(new NPVariant[argc]);
    for (uint32_t i = 0; i < argc; ++i)
      args[i] = PPVarToNPVariantNoCopy(argv[i]);
  }

  NPVariant result;
  if (!WebBindings::construct(NULL, object, args.get(), argc, &result)) {
    // An exception may have been raised.
    if (!try_catch.HasException())
      try_catch.SetException(kUnableToConstructException);
    return PP_MakeVoid();
  }

  PP_Var ret = NPVariantToPPVar(&result);
  WebBindings::releaseVariantValue(&result);
  return ret;
}

bool IsInstanceOf(PP_Var var, const PPP_Class* ppp_class,
                  void** ppp_class_data) {
  NPObject* object = GetNPObject(var);
  if (!object)
    return false;

  if (object->_class != &wrapper_class)
    return false;

  WrapperObject* wrapper = ToWrapper(object);
  if (wrapper->ppp_class != ppp_class)
    return false;

  if (ppp_class_data)
    *ppp_class_data = wrapper->ppp_class_data;
  return true;
}

PP_Var CreateObject(const PPP_Class* ppp_class, void* ppp_class_data) {
  NPObject* object =
      WebBindings::createObject(NULL, const_cast<NPClass*>(&wrapper_class));
  static_cast<WrapperObject*>(object)->ppp_class = ppp_class;
  static_cast<WrapperObject*>(object)->ppp_class_data = ppp_class_data;
  PP_Var ret = NPObjectToPPVar(object);
  WebBindings::releaseObject(object);  // Release reference from createObject
  return ret;
}

const PPB_Var var_interface = {
  &AddRef,
  &Release,
  &VarFromUtf8,
  &VarToUtf8,
  &HasProperty,
  &HasMethod,
  &GetProperty,
  &GetAllPropertyNames,
  &SetProperty,
  &RemoveProperty,
  &Call,
  &Construct,
  &IsInstanceOf,
  &CreateObject
};

}  // namespace

const PPB_Var* GetVarInterface() {
  return &var_interface;
}

PP_Var NPObjectToPPVar(NPObject* object) {
  PP_Var ret;
  ret.type = PP_VARTYPE_OBJECT;
  ret.value.as_id = reinterpret_cast<intptr_t>(object);
  WebBindings::retainObject(object);
  return ret;
}

PP_Var NPVariantToPPVar(const NPVariant* variant) {
  switch (variant->type) {
    case NPVariantType_Void:
      return PP_MakeVoid();
    case NPVariantType_Null:
      return PP_MakeNull();
    case NPVariantType_Bool:
      return PP_MakeBool(NPVARIANT_TO_BOOLEAN(*variant));
    case NPVariantType_Int32:
      return PP_MakeInt32(NPVARIANT_TO_INT32(*variant));
    case NPVariantType_Double:
      return PP_MakeDouble(NPVARIANT_TO_DOUBLE(*variant));
    case NPVariantType_String:
      return VarFromUtf8(NPVARIANT_TO_STRING(*variant).UTF8Characters,
                         NPVARIANT_TO_STRING(*variant).UTF8Length);
    case NPVariantType_Object:
      return NPObjectToPPVar(NPVARIANT_TO_OBJECT(*variant));
  }
  NOTREACHED();
  return PP_MakeVoid();
}


NPObject* GetNPObject(PP_Var var) {
  if (var.type != PP_VARTYPE_OBJECT)
    return NULL;
  return GetNPObjectUnchecked(var);
}

PP_Var StringToPPVar(const std::string& str) {
  DCHECK(IsStringUTF8(str));
  return VarFromUtf8(str.data(), str.size());
}

String* GetString(PP_Var var) {
  if (var.type != PP_VARTYPE_STRING)
    return NULL;
  return GetStringUnchecked(var);
}

TryCatch::TryCatch(PP_Var* exception) : exception_(exception) {
  WebBindings::pushExceptionHandler(&TryCatch::Catch, this);
}

TryCatch::~TryCatch() {
  WebBindings::popExceptionHandler();
}

bool TryCatch::HasException() const {
  return exception_ && exception_->type != PP_VARTYPE_VOID;
}

void TryCatch::SetException(const char* message) {
  DCHECK(!HasException());
  if (exception_)
    *exception_ = VarFromUtf8(message, strlen(message));
}

// static
void TryCatch::Catch(void* self, const char* message) {
  static_cast<TryCatch*>(self)->SetException(message);
}


}  // namespace pepper
