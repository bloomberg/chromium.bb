/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <string.h>
#include <map>
#include <string>

#include "native_client/tests/fake_browser_ppapi/fake_object.h"

#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/include/portability.h"
#include "native_client/src/shared/ppapi_proxy/plugin_ppb_var_deprecated.h"
#include "native_client/tests/fake_browser_ppapi/utility.h"
#include "ppapi/c/dev/ppb_var_deprecated.h"
#include "ppapi/c/pp_var.h"

using fake_browser_ppapi::DebugPrintf;
using ppapi_proxy::PluginVarDeprecated;
using fake_browser_ppapi::Object;

namespace fake_browser_ppapi {

const PPB_Var_Deprecated* ppb_var_deprecated = NULL;

bool Object::HasProperty(PP_Var name,
                         PP_Var* exception) {
  UNREFERENCED_PARAMETER(exception);
  DebugPrintf("Object::HasProperty: object=%p, name=",
              reinterpret_cast<void*>(this));
  PluginVarDeprecated::Print(name);
  DebugPrintf("\n");
  Object* browser_obj = reinterpret_cast<Object*>(this);
  Object::PropertyMap::iterator i =
      browser_obj->properties()->find(PluginVarDeprecated::DebugString(name));
  return (i != browser_obj->properties()->end());
}

bool Object::HasMethod(PP_Var name,
                       PP_Var* exception) {
  UNREFERENCED_PARAMETER(exception);
  DebugPrintf("Object::HasMethod: object=%p, name=",
              reinterpret_cast<void*>(this));
  PluginVarDeprecated::Print(name);
  DebugPrintf("\n");
  Object* browser_obj = reinterpret_cast<Object*>(this);
  Object::MethodMap::iterator i =
      browser_obj->methods()->find(PluginVarDeprecated::DebugString(name));
  return (i != browser_obj->methods()->end());
}

PP_Var Object::GetProperty(PP_Var name,
                           PP_Var* exception) {
  UNREFERENCED_PARAMETER(exception);
  DebugPrintf("Object::GetProperty: object=%p, name=",
              reinterpret_cast<void*>(this));
  PluginVarDeprecated::Print(name);
  DebugPrintf("\n");
  Object* browser_obj = reinterpret_cast<Object*>(this);
  Object::PropertyMap::iterator i =
      browser_obj->properties()->find(PluginVarDeprecated::DebugString(name));
  if (i != browser_obj->properties()->end()) {
    ppb_var_deprecated->AddRef(*(i->second));
    return *(i->second);
  }
  return PP_MakeUndefined();
}

void Object::GetAllPropertyNames(uint32_t* property_count,
                                 PP_Var** properties,
                                 PP_Var* exception) {
  UNREFERENCED_PARAMETER(exception);
  DebugPrintf("Object::GetAllPropertyNames: object=%p\n",
              reinterpret_cast<void*>(this));
  // Figure out the length of the vector.
  Object* browser_obj = reinterpret_cast<Object*>(this);
  Object::PropertyMap::iterator prop;
  for (prop = browser_obj->properties()->begin();
       prop != browser_obj->properties()->end(); ++prop) {
    ++(*property_count);
  }
  Object::MethodMap::iterator meth;
  for (meth = browser_obj->methods()->begin();
       meth != browser_obj->methods()->end(); ++meth) {
    ++(*property_count);
  }
  // Fill out the vector.
  *properties =
      reinterpret_cast<PP_Var*>(malloc(*property_count * sizeof(**properties)));
  uint32_t i = 0;
  for (prop = browser_obj->properties()->begin();
       prop != browser_obj->properties()->end(); ++prop) {
    (*properties)[i] =
        ppb_var_deprecated->VarFromUtf8(
            browser_obj->module(),
            prop->first.c_str(),
            static_cast<uint32_t>(prop->first.size()));
    ++i;
  }
  for (meth = browser_obj->methods()->begin();
       meth != browser_obj->methods()->end(); ++meth) {
    (*properties)[i] =
        ppb_var_deprecated->VarFromUtf8(
            browser_obj->module(),
            meth->first.c_str(),
            static_cast<uint32_t>(meth->first.size()));
    ++i;
  }
}

void Object::SetProperty(PP_Var name,
                         PP_Var value,
                         PP_Var* exception) {
  UNREFERENCED_PARAMETER(exception);
  DebugPrintf("Object::SetProperty: object=%p, name=",
              reinterpret_cast<void*>(this));
  PluginVarDeprecated::Print(name);
  DebugPrintf(", value=");
  PluginVarDeprecated::Print(value);
  DebugPrintf("\n");
  Object* browser_obj = reinterpret_cast<Object*>(this);
  // Release the previous value in the map.
  Object::PropertyMap::iterator i =
      browser_obj->properties()->find(PluginVarDeprecated::DebugString(name));
  if (i != browser_obj->properties()->end()) {
    ppb_var_deprecated->Release(*(i->second));
  }
  PP_Var* newval = reinterpret_cast<PP_Var*>(malloc(sizeof(*newval)));
  *newval = value;
  ppb_var_deprecated->AddRef(*newval);
  (*browser_obj->properties())[PluginVarDeprecated::DebugString(name)] = newval;
}

void Object::RemoveProperty(PP_Var name,
                            PP_Var* exception) {
  UNREFERENCED_PARAMETER(exception);
  DebugPrintf("Object::RemoveProperty: object=%p, name=",
              reinterpret_cast<void*>(this));
  PluginVarDeprecated::Print(name);
  DebugPrintf("\n");
  Object* browser_obj = reinterpret_cast<Object*>(this);
  // Release the value.
  Object::PropertyMap::iterator i =
      browser_obj->properties()->find(PluginVarDeprecated::DebugString(name));
  if (i != browser_obj->properties()->end()) {
    ppb_var_deprecated->Release(*(i->second));
    browser_obj->properties()->erase(i);
  }
}

PP_Var Object::Call(PP_Var method_name,
                    uint32_t argc,
                    PP_Var* argv,
                    PP_Var* exception) {
  Object* browser_obj = reinterpret_cast<Object*>(this);
  UNREFERENCED_PARAMETER(exception);
  DebugPrintf("Object::Call: object=%p, method_name=",
              reinterpret_cast<void*>(this));
  PluginVarDeprecated::Print(method_name);
  DebugPrintf(", argc=%"NACL_PRIu32", argv={ ", argc);
  for (uint32_t i = 0; i < argc; ++i) {
    PluginVarDeprecated::Print(argv[i]);
    if (i < argc - 1) {
      DebugPrintf(", ");
    }
  }
  DebugPrintf(" }\n");
  Object::MethodMap::iterator i =
      browser_obj->methods()->find(
          PluginVarDeprecated::DebugString(method_name));
  if (i != browser_obj->methods()->end()) {
    return (i->second)(browser_obj, argc, argv, exception);
  }
  return PP_MakeUndefined();
}

PP_Var Object::Construct(uint32_t argc,
                         PP_Var* argv,
                         PP_Var* exception) {
  UNREFERENCED_PARAMETER(exception);
  DebugPrintf("Object::Construct: object=%p",
              reinterpret_cast<void*>(this));
  DebugPrintf(", argc=%"NACL_PRIu32", argv={ ", argc);
  for (uint32_t i = 0; i < argc; ++i) {
    PluginVarDeprecated::Print(argv[i]);
    if (i < argc - 1) {
      DebugPrintf(", ");
    }
  }
  DebugPrintf(" }\n");
  NACL_UNIMPLEMENTED();
  return PP_MakeUndefined();
}

void Object::Deallocate() {
  DebugPrintf("Object::Deallocate: object=%p\n",
              reinterpret_cast<void*>(this));
  delete reinterpret_cast<Object*>(this);
}

Object::Object(PP_Module module,
               PP_Instance instance,
               const PropertyMap& properties,
               const MethodMap& methods)
    : module_(module),
      instance_(instance) {
  PropertyMap::const_iterator prop;
  for (prop = properties.begin(); prop != properties.end(); ++prop) {
    properties_[prop->first] = prop->second;
  }
  MethodMap::const_iterator meth;
  for (meth = methods.begin(); meth != methods.end(); ++meth) {
    methods_[meth->first] = meth->second;
  }
}

PP_Var Object::New(PP_Module module,
                   PP_Instance instance,
                   const PropertyMap& properties,
                   const MethodMap& methods) {
  // Save the PPB_Var_Deprecated interface to be used in constructing objects.
  if (ppb_var_deprecated == NULL) {
    ppb_var_deprecated = reinterpret_cast<const PPB_Var_Deprecated*>(
        PluginVarDeprecated::GetInterface());
    if (ppb_var_deprecated == NULL) {
      return PP_MakeUndefined();
    }
  }
  Object* obj = new Object(module, instance, properties, methods);
  if (obj == NULL) {
    return PP_MakeUndefined();
  }
  return ppb_var_deprecated->CreateObject(instance,
                                          &ppapi_proxy::Object::object_class,
                                          obj);
}

}  // namespace fake_browser_ppapi
