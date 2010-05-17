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

using WebKit::WebBindings;

namespace pepper {

namespace {

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
  if (exception && exception->type != PP_VarType_Void)
    return false;

  NPObject* object = GetNPObject(var);
  if (!object) {
    // TODO(darin): raise exception
    return false;
  }

  NPIdentifier identifier = PPVarToNPIdentifier(name);
  if (!identifier) {
    // TODO(darin): raise exception
    return false;
  }

  return WebBindings::hasProperty(NULL, object, identifier);
}

PP_Var GetProperty(PP_Var var,
                   PP_Var name,
                   PP_Var* exception) {
  if (exception && exception->type != PP_VarType_Void)
    return PP_MakeVoid();

  NPObject* object = GetNPObject(var);
  if (!object) {
    // TODO(darin): raise exception
    return PP_MakeVoid();
  }

  NPIdentifier identifier = PPVarToNPIdentifier(name);
  if (!identifier) {
    // TODO(darin): raise exception
    return PP_MakeVoid();
  }

  NPVariant result;
  if (!WebBindings::getProperty(NULL, object, identifier, &result)) {
    // TODO(darin): raise exception
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
  if (exception && exception->type != PP_VarType_Void)
    return;

  // TODO(darin)
}

void SetProperty(PP_Var var,
                 PP_Var name,
                 PP_Var value,
                 PP_Var* exception) {
  if (exception && exception->type != PP_VarType_Void)
    return;

  NPObject* object = GetNPObject(var);
  if (!object) {
    // TODO(darin): raise exception
    return;
  }

  NPIdentifier identifier = PPVarToNPIdentifier(name);
  if (!identifier) {
    // TODO(darin): raise exception
    return;
  }

  NPVariant variant = PPVarToNPVariantNoCopy(value);
  WebBindings::setProperty(NULL, object, identifier, &variant);
}

void RemoveProperty(PP_Var var,
                    PP_Var name,
                    PP_Var* exception) {
  if (exception && exception->type != PP_VarType_Void)
    return;

  NPObject* object = GetNPObject(var);
  if (!object) {
    // TODO(darin): raise exception
    return;
  }

  NPIdentifier identifier = PPVarToNPIdentifier(name);
  if (!identifier) {
    // TODO(darin): raise exception
    return;
  }

  WebBindings::removeProperty(NULL, object, identifier);
}

PP_Var Call(PP_Var var,
            PP_Var method_name,
            int32_t argc,
            PP_Var* argv,
            PP_Var* exception) {
  if (exception && exception->type != PP_VarType_Void)
    return PP_MakeVoid();

  NPObject* object = GetNPObject(var);
  if (!object) {
    // TODO(darin): raise exception
    return PP_MakeVoid();
  }

  NPIdentifier identifier;
  if (method_name.type == PP_VarType_Void) {
    identifier = NULL;
  } else {
    identifier = PPVarToNPIdentifier(method_name);
    if (!identifier) {
      // TODO(darin): raise exception
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
    // TODO(darin): raise exception
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
  if (exception && exception->type != PP_VarType_Void)
    return PP_MakeVoid();

  NPObject* object = GetNPObject(var);
  if (!object) {
    // TODO(darin): raise exception
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
    // TODO(darin): raise exception
    return PP_MakeVoid();
  }

  PP_Var ret = NPVariantToPPVar(&result);
  WebBindings::releaseVariantValue(&result);
  return ret;
}

PP_Var CreateObject(const PPP_Class* object_class,
                    void* object_data) {
  return PP_MakeVoid();  // TODO(darin)
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

}  // namespace pepper
