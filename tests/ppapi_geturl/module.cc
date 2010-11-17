// Copyright 2010 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#include "native_client/tests/ppapi_geturl/module.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "native_client/tests/ppapi_geturl/scriptable_object.h"

#include "ppapi/c/pp_errors.h"
#include "ppapi/c/ppb_instance.h"
#include "ppapi/c/dev/ppp_class_deprecated.h"

namespace {
PPP_Instance instance_interface;
Module* singeton_ = NULL;
}

PP_Bool Instance_DidCreate(PP_Instance pp_instance,
                        uint32_t argc,
                        const char* argn[],
                        const char* argv[]) {
  printf("--- Instance_DidCreate\n");
  return PP_TRUE;
}

void Instance_DidDestroy(PP_Instance instance) {
  printf("--- Instance_DidDestroy\n");
}

void Instance_DidChangeView(PP_Instance pp_instance,
                            const PP_Rect* position,
                            const PP_Rect* clip) {
  printf("--- Instance_DidChangeView\n");
}

void Instance_DidChangeFocus(PP_Instance pp_instance, PP_Bool has_focus) {
  printf("--- Instance_DidChangeFocus\n");
}

PP_Bool Instance_HandleInputEvent(PP_Instance pp_instance,
                               const PP_InputEvent* event) {
  printf("--- Instance_HandleInputEvent\n");
  return PP_FALSE;
}

PP_Bool Instance_HandleDocumentLoad(PP_Instance pp_instance,
                                 PP_Resource pp_url_loader) {
  printf("--- Instance_HandleDocumentLoad\n");
  return PP_FALSE;
}

PP_Var Instance_GetInstanceObject(PP_Instance pp_instance) {
  printf("--- Instance_GetInstanceObject\n");
  PP_Var v = PP_MakeUndefined();
  Module* module = Module::Get();
  if (module) {
    const PPB_Var_Deprecated* var_interface = module->var_interface();
    if (var_interface) {
      ScriptableObject* obj = new ScriptableObject(pp_instance);
      v = var_interface->CreateObject(
          module->module_id(),
          obj->ppp_class(),
          obj);
    }
  }
  return v;
}

Module* Module::Create(PP_Module module_id,
                       PPB_GetInterface get_browser_interface) {
  if (NULL == singeton_) {
    singeton_ = new Module(module_id, get_browser_interface);
  }
  return singeton_;
}

void Module::Free() {
  if (NULL != singeton_) {
    delete singeton_;
    singeton_ = NULL;
  }
}

Module::Module(PP_Module module_id, PPB_GetInterface get_browser_interface)
  : module_id_(module_id),
    get_browser_interface_(get_browser_interface),
    core_interface_(NULL),
    var_interface_(NULL) {
  printf("--- Module::Module\n");
  memset(&instance_interface, 0, sizeof(instance_interface));
  instance_interface.DidCreate = Instance_DidCreate;
  instance_interface.DidDestroy = Instance_DidDestroy;
  instance_interface.DidChangeView = Instance_DidChangeView;
  instance_interface.DidChangeFocus = Instance_DidChangeFocus;
  instance_interface.HandleInputEvent = Instance_HandleInputEvent;
  instance_interface.HandleDocumentLoad = Instance_HandleDocumentLoad;
  instance_interface.GetInstanceObject = Instance_GetInstanceObject;
  core_interface_ =
      reinterpret_cast<const PPB_Core*>(
          GetBrowserInterface(PPB_CORE_INTERFACE));
  var_interface_ =
      reinterpret_cast<const PPB_Var_Deprecated*>(
          GetBrowserInterface(PPB_VAR_DEPRECATED_INTERFACE));
}

Module::~Module() {
}

Module* Module::Get() {
  return singeton_;
}

const PPB_Var_Deprecated* Module::var_interface() const {
  return var_interface_;
}

PP_Module Module::module_id() {
  return module_id_;
}

const PPB_Core* Module::core_interface() {
  return core_interface_;
}

const void* Module::GetPluginInterface(const char* interface_name) {
  printf("--- Module::GetPluginInterface(%s)\n", interface_name);
  if (strcmp(interface_name, PPP_INSTANCE_INTERFACE) == 0)
    return &instance_interface;
  return NULL;
}

const void* Module::GetBrowserInterface(const char* interface_name) {
  if (NULL == get_browser_interface_)
    return NULL;
  return (*get_browser_interface_)(interface_name);
}

char* Module::VarToCStr(const PP_Var& var) {
  Module* module = Get();
  if (NULL == module)
    return NULL;
  const PPB_Var_Deprecated* vi = module->var_interface();
  if (NULL == vi)
    return NULL;
  uint32_t len = 0;
  const char* pp_str = vi->VarToUtf8(var, &len);
  if (NULL == pp_str)
    return NULL;
  char* str = static_cast<char*>(malloc(len + 1));
  if (NULL == str)
    return NULL;
  memcpy(str, pp_str, len);
  str[len] = 0;
  return str;
}

std::string Module::VarToStr(const PP_Var& var) {
  std::string str;
  char* cstr = VarToCStr(var);
  if (cstr) {
    str = cstr;
    free(cstr);
  }
  return str;
}

PP_Var Module::StrToVar(const char* str) {
  Module* module = Get();
  if (NULL == module)
    return PP_MakeUndefined();
  const PPB_Var_Deprecated* vi = module->var_interface();
  if (NULL != vi)
    return vi->VarFromUtf8(module->module_id(), str, strlen(str));
  return PP_MakeUndefined();
}

PP_Var Module::StrToVar(const std::string& str) {
  return Module::StrToVar(str.c_str());
}

std::string Module::ErrorCodeToStr(int32_t error_code) {
  switch (error_code) {
    case PP_OK: return "PP_OK";
    case PP_ERROR_WOULDBLOCK: return "PP_ERROR_WOULDBLOCK";
    case PP_ERROR_FAILED: return "PP_ERROR_FAILED";
    case PP_ERROR_ABORTED: return "PP_ERROR_ABORTED";
    case PP_ERROR_BADARGUMENT: return "PP_ERROR_BADARGUMENT";
    case PP_ERROR_BADRESOURCE: return "PP_ERROR_BADRESOURCE";
    case PP_ERROR_NOINTERFACE: return "PP_ERROR_NOINTERFACE";
    case PP_ERROR_NOACCESS: return "PP_ERROR_NOACCESS";
    case PP_ERROR_NOMEMORY: return "PP_ERROR_NOMEMORY";
    case PP_ERROR_NOSPACE: return "PP_ERROR_NOSPACE";
    case PP_ERROR_NOQUOTA: return "PP_ERROR_NOQUOTA";
    case PP_ERROR_INPROGRESS: return "PP_ERROR_INPROGRESS";
    case PP_ERROR_FILENOTFOUND: return "PP_ERROR_FILENOTFOUND";
    case PP_ERROR_FILEEXISTS: return "PP_ERROR_FILEEXISTS";
    case PP_ERROR_FILETOOBIG: return "PP_ERROR_FILETOOBIG";
    case PP_ERROR_FILECHANGED: return "PP_ERROR_FILECHANGED";
    case PP_ERROR_TIMEDOUT: return "PP_ERROR_TIMEDOUT";
  }
  return "N/A";
}

void Module::ReportResult(PP_Instance pp_instance,
                          const char* fname,
                          const char* text,
                          bool success) {
  const PPB_Instance* ppb_instance =
      static_cast<const PPB_Instance*>(Get()->GetBrowserInterface(
          PPB_INSTANCE_INTERFACE));
  if (NULL == ppb_instance) {
    printf("--- GetBrowserInterface("PPB_INSTANCE_INTERFACE") failed\n");
    return;
  }
  const PPB_Var_Deprecated* ppb_var_interface = Get()->var_interface();
  if (NULL == ppb_var_interface) {
    printf("--- Module::var_interface() failed\n");
    return;
  }
  PP_Var window_var = ppb_instance->GetWindowObject(pp_instance);
  if (PP_VARTYPE_OBJECT != window_var.type) {
    printf("--- ppb_instance->GetWindowObject() failed\n");
    return;
  }
  PP_Var method_name_var = Module::StrToVar("ReportResult");
  PP_Var exception_var = PP_MakeUndefined();
  PP_Var args[3];
  args[0] = Module::StrToVar(fname);
  args[1] = Module::StrToVar(text);
  args[2] = PP_MakeBool(success ? PP_TRUE : PP_FALSE);

  ppb_var_interface->Call(window_var,
                          method_name_var,
                          sizeof(args) / sizeof(args[0]),
                          args,
                          &exception_var);

  ppb_var_interface->Release(method_name_var);
  ppb_var_interface->Release(args[0]);
  ppb_var_interface->Release(args[1]);
  ppb_var_interface->Release(exception_var);
  ppb_var_interface->Release(window_var);
}
