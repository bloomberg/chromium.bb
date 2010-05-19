// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/glue/plugins/pepper_var.h"

#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "base/string_util.h"
#include "third_party/ppapi/c/pp_var.h"
#include "third_party/ppapi/c/ppb_var.h"
#include "third_party/WebKit/WebKit/chromium/public/WebBindings.h"
#include "webkit/glue/plugins/pepper_string.h"
#include "v8/include/v8.h"

// Uncomment to enable catching JS exceptions
// #define HAVE_WEBBINDINGS_EXCEPTION_HANDLER 1

using WebKit::WebBindings;

namespace pepper {

namespace {

PP_Var VarFromUtf8(const char* data, uint32_t len);

// ---------------------------------------------------------------------------
// Exceptions

class TryCatch {
 public:
  TryCatch(PP_Var* exception) : exception_(exception) {
#ifdef HAVE_WEBBINDINGS_EXCEPTION_HANDLER
    WebBindings::pushExceptionHandler(&TryCatch::Catch, this);
#endif
  }

  ~TryCatch() {
#ifdef HAVE_WEBBINDINGS_EXCEPTION_HANDLER
    WebBindings::popExceptionHandler();
#endif
  }

  bool HasException() const {
    return exception_ && exception_->type != PP_VarType_Void;
  }

  void SetException(const char* message) {
    DCHECK(!HasException());
    if (exception_)
      *exception_ = VarFromUtf8(message, strlen(message));
  }

 private:
  static void Catch(void* self, const NPUTF8* message) {
    static_cast<TryCatch*>(self)->SetException(message);
  }

  // May be null if the consumer isn't interesting in catching exceptions.
  PP_Var* exception_;
};

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

NPObject* GetNPObject(PP_Var var) {
  if (var.type != PP_VarType_Object)
    return NULL;
  return GetNPObjectUnchecked(var);
}

// Returns a PP_Var that corresponds to the given NPVariant.  The contents of
// the NPVariant will be copied unless the NPVariant corresponds to an object.
PP_Var NPVariantToPPVar(NPVariant* variant) {
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

// Returns a NPVariant that corresponds to the given PP_Var.  The contents of
// the PP_Var will be copied unless the PP_Var corresponds to an object.
NPVariant PPVarToNPVariant(PP_Var var) {
  NPVariant ret;
  switch (var.type) {
    case PP_VarType_Void:
      VOID_TO_NPVARIANT(ret);
      break;
    case PP_VarType_Null:
      NULL_TO_NPVARIANT(ret);
      break;
    case PP_VarType_Bool:
      BOOLEAN_TO_NPVARIANT(var.value.as_bool, ret);
      break;
    case PP_VarType_Int32:
      INT32_TO_NPVARIANT(var.value.as_int, ret);
      break;
    case PP_VarType_Double:
      DOUBLE_TO_NPVARIANT(var.value.as_double, ret);
      break;
    case PP_VarType_String: {
      const std::string& value = GetStringUnchecked(var)->value();
      STRINGN_TO_NPVARIANT(base::strdup(value.c_str()), value.size(), ret);
      break;
    }
    case PP_VarType_Object: {
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
    case PP_VarType_Void:
      VOID_TO_NPVARIANT(ret);
      break;
    case PP_VarType_Null:
      NULL_TO_NPVARIANT(ret);
      break;
    case PP_VarType_Bool:
      BOOLEAN_TO_NPVARIANT(var.value.as_bool, ret);
      break;
    case PP_VarType_Int32:
      INT32_TO_NPVARIANT(var.value.as_int, ret);
      break;
    case PP_VarType_Double:
      DOUBLE_TO_NPVARIANT(var.value.as_double, ret);
      break;
    case PP_VarType_String: {
      const std::string& value = GetStringUnchecked(var)->value();
      STRINGN_TO_NPVARIANT(value.c_str(), value.size(), ret);
      break;
    }
    case PP_VarType_Object: {
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
    case PP_VarType_String:
      return WebBindings::getStringIdentifier(
          GetStringUnchecked(var)->value().c_str());
    case PP_VarType_Int32:
      return WebBindings::getIntIdentifier(var.value.as_int);
    default:
      return NULL;
  }
}

// ---------------------------------------------------------------------------
// PPB_Var methods

void AddRef(PP_Var var) {
  if (var.type == PP_VarType_String) {
    GetStringUnchecked(var)->AddRef();
  } else if (var.type == PP_VarType_Object) {
    // TODO(darin): Add thread safety check
    WebBindings::retainObject(GetNPObjectUnchecked(var));
  }
}

void Release(PP_Var var) {
  if (var.type == PP_VarType_String) {
    GetStringUnchecked(var)->Release();
  } else if (var.type == PP_VarType_Object) {
    // TODO(darin): Add thread safety check
    WebBindings::releaseObject(GetNPObjectUnchecked(var));
  }
}

PP_Var VarFromUtf8(const char* data, uint32_t len) {
  String* str = new String(data, len);
  str->AddRef();  // This is for the caller, we return w/ a refcount of 1.
  PP_Var ret;
  ret.type = PP_VarType_String;
  ret.value.as_id = reinterpret_cast<intptr_t>(str);
  return ret;
}

const char* VarToUtf8(PP_Var var, uint32_t* len) {
  if (var.type != PP_VarType_String) {
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
  TryCatch try_catch(exception);
  if (try_catch.HasException())
    return;

  // TODO(darin): Implement this method!
  try_catch.SetException(kUnableToGetAllPropertiesException);
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
            int32_t argc,
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
  if (method_name.type == PP_VarType_Void) {
    identifier = NULL;
  } else {
    identifier = PPVarToNPIdentifier(method_name);
    if (!identifier) {
      try_catch.SetException(kInvalidPropertyException);
      return PP_MakeVoid();
    }
  }

  scoped_array<NPVariant> args;
  if (argc) {
    args.reset(new NPVariant[argc]);
    for (int32_t i = 0; i < argc; ++i)
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
                 int32_t argc,
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
    for (int32_t i = 0; i < argc; ++i)
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

PP_Var CreateObject(const PPP_Class* object_class,
                    void* object_data) {
  return PP_MakeVoid();  // TODO(darin): Implement this method!
}

const PPB_Var var_interface = {
  &AddRef,
  &Release,
  &VarFromUtf8,
  &VarToUtf8,
  &HasProperty,
  &GetProperty,
  &GetAllPropertyNames,
  &SetProperty,
  &RemoveProperty,
  &Call,
  &Construct,
  &CreateObject
};

}  // namespace

const PPB_Var* GetVarInterface() {
  return &var_interface;
}

PP_Var NPObjectToPPVar(NPObject* object) {
  PP_Var ret;
  ret.type = PP_VarType_Object;
  ret.value.as_id = reinterpret_cast<intptr_t>(object);
  WebBindings::retainObject(object);
  return ret;
}

}  // namespace pepper
