// Copyright 2010 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#include "native_client/tests/ppapi_geturl/scriptable_object.h"

#include <stdio.h>
#include <string.h>

#include "native_client/tests/ppapi_geturl/module.h"
#include "native_client/tests/ppapi_geturl/url_load_request.h"

ScriptableObject::ScriptableObject(PP_Instance instance)
  :instance_(instance) {
  memset(&ppp_class_, 0, sizeof(ppp_class_));
  ppp_class_.HasProperty = HasProperty;
  ppp_class_.HasMethod = HasMethod;
  ppp_class_.GetProperty = GetProperty;
  ppp_class_.GetAllPropertyNames = GetAllPropertyNames;
  ppp_class_.SetProperty = SetProperty;
  ppp_class_.RemoveProperty = RemoveProperty;
  ppp_class_.Call = Call;
  ppp_class_.Construct = Construct;
  ppp_class_.Deallocate = Deallocate;
}

const PPP_Class_Deprecated* ScriptableObject::ppp_class() const {
  return &ppp_class_;
}

bool ScriptableObject::HasProperty(void* object,
                                   PP_Var name,
                                   PP_Var* exception) {
  bool result = false;
  std::string str = Module::VarToStr(name);
  if (str == "__moduleReady")
    result = true;
  return result;
}

bool ScriptableObject::HasMethod(void* object,
                                 PP_Var name,
                                 PP_Var* exception) {
  bool result = false;
  std::string str = Module::VarToStr(name);
  if (str == "loadUrl")
    result = true;
  return result;
}

PP_Var ScriptableObject::GetProperty(void* object,
                                     PP_Var name,
                                     PP_Var* exception) {
  std::string str = Module::VarToStr(name);
  PP_Var var = PP_MakeUndefined();
  if (str == "__moduleReady")
    var = PP_MakeInt32(1);

  return var;
}

void ScriptableObject::GetAllPropertyNames(void* object,
                                           uint32_t* property_count,
                                           PP_Var** properties,
                                           PP_Var* exception) {
  printf("--- ScriptableObject::GetAllPropertyNames(%p)\n", object);
}

void ScriptableObject::SetProperty(void* object,
                                   PP_Var name,
                                   PP_Var value,
                                   PP_Var* exception) {
  printf("--- ScriptableObject::SetProperty(%p)\n", object);
}

void ScriptableObject::RemoveProperty(void* object,
                                      PP_Var name,
                                      PP_Var* exception) {
  printf("--- ScriptableObject::RemoveProperty(%p)\n", object);
}

PP_Var ScriptableObject::Call(void* object,
                              PP_Var method_name,
                              uint32_t argc,
                              PP_Var* argv,
                              PP_Var* exception) {
  ScriptableObject* this_object = static_cast<ScriptableObject*>(object);
  std::string str = Module::VarToStr(method_name);
  printf("--- ScriptableObject::Call(%p, '%s'", object, str.c_str());
  for (uint32_t i = 0; i < argc; i++) {
    std::string str =  Module::VarToStr(argv[i]);
    printf(", '%s'", str.c_str());
  }
  printf(")\n");
  if (str == "loadUrl") {
    std::string url;
    if (argc > 0)
      url = Module::VarToStr(argv[0]);

    ScriptableObject* this_obj = static_cast<ScriptableObject*>(object);
    if (this_obj) {
      std::string error;
      if (!this_obj->LoadUrl(url, &error)) {
        *exception = Module::StrToVar(error);
        Module::Get()->ReportResult(this_object->instance_,
                                    url.c_str(),
                                    error.c_str(),
                                    false);
      }
    }
  }
  PP_Var var = PP_MakeUndefined();
  return var;
}

PP_Var ScriptableObject::Construct(void* object,
                                   uint32_t argc,
                                   PP_Var* argv,
                                   PP_Var* exception) {
  printf("--- ScriptableObject::Construct(%p)\n", object);
  PP_Var var = PP_MakeUndefined();
  return var;
}

void ScriptableObject::Deallocate(void* object) {
  printf("--- ScriptableObject::Deallocate(%p)\n", object);
  delete static_cast<ScriptableObject*>(object);
}

bool ScriptableObject::LoadUrl(std::string url,
                               std::string* error) {
  printf("--- ScriptableObject::LoadUrl('%s')\n", url.c_str());
  UrlLoadRequest* url_req = new UrlLoadRequest(instance_);
  if (NULL == url_req) {
    *error = "Memory allocation failed";
    return false;
  }
  if (!url_req->Init(url, error)) {
    delete url_req;
    return false;
  }
  return true;
}
