// Copyright (c) 2011 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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

bool ScriptableObject::HasProperty(void* /*object*/,
                                   PP_Var /*name*/,
                                   PP_Var* /*exception*/) {
  return false;
}

bool ScriptableObject::HasMethod(void* /*object*/,
                                 PP_Var name,
                                 PP_Var* /*exception*/) {
  return (Module::VarToStr(name) == "loadUrl");
}

PP_Var ScriptableObject::GetProperty(void* /*object*/,
                                     PP_Var /*name*/,
                                     PP_Var* /*exception*/) {
  return PP_MakeUndefined();
}

void ScriptableObject::GetAllPropertyNames(void* object,
                                           uint32_t* /*property_count*/,
                                           PP_Var** /*properties*/,
                                           PP_Var* /*exception*/) {
  printf("--- ScriptableObject::GetAllPropertyNames(%p)\n", object);
}

void ScriptableObject::SetProperty(void* object,
                                   PP_Var /*name*/,
                                   PP_Var /*value*/,
                                   PP_Var* /*exception*/) {
  printf("--- ScriptableObject::SetProperty(%p)\n", object);
}

void ScriptableObject::RemoveProperty(void* object,
                                      PP_Var /*name*/,
                                      PP_Var* /*exception*/) {
  printf("--- ScriptableObject::RemoveProperty(%p)\n", object);
}

PP_Var ScriptableObject::Call(void* object,
                              PP_Var method_name,
                              uint32_t argc,
                              PP_Var* argv,
                              PP_Var* /*exception*/) {
  std::string name = Module::VarToStr(method_name);
  printf("--- ScriptableObject::Call(%p, '%s'", object, name.c_str());
  for (uint32_t i = 0; i < argc; i++) {
    printf(", '%s'", Module::VarToStr(argv[i]).c_str());
  }
  printf(")\n");
  if (name == "loadUrl") {
    std::string url;
    if (argc > 0) {
      url = Module::VarToStr(argv[0]);
    }
    bool stream_as_file = false;
    if (argc > 1) {
      stream_as_file = argv[1].value.as_bool;
    }
    ScriptableObject* this_object = static_cast<ScriptableObject*>(object);
    if (this_object) {
      // Note that we don't set the exception when LoadUrl fails because
      // error reporting is done via ReportResult(). Otherwise,
      // we will trigger "Uncaught Error: Error calling method on NPObject.",
      // which will make JS handling code more complicated if we want
      // the rest of the statements after the failing call to be executed.
      this_object->LoadUrl(stream_as_file, url);
    }
  }
  PP_Var var = PP_MakeUndefined();
  printf("--- ScriptableObject::Call(%s): done\n", name.c_str());
  return var;
}

PP_Var ScriptableObject::Construct(void* object,
                                   uint32_t /*argc*/,
                                   PP_Var* /*argv*/,
                                   PP_Var* /*exception*/) {
  printf("--- ScriptableObject::Construct(%p)\n", object);
  PP_Var var = PP_MakeUndefined();
  return var;
}

void ScriptableObject::Deallocate(void* object) {
  printf("--- ScriptableObject::Deallocate(%p)\n", object);
  delete static_cast<ScriptableObject*>(object);
}

void ScriptableObject::LoadUrl(bool stream_as_file, std::string url) {
  printf("--- ScriptableObject::LoadUrl('%s')\n", url.c_str());
  UrlLoadRequest* url_load_request = new UrlLoadRequest(instance_);
  if (NULL == url_load_request) {
    Module::Get()->ReportResult(instance_,
                                url.c_str(),
                                stream_as_file,
                                "LoadUrl: memory allocation failed",
                                false);
    return;
  }
  // On success or failure url_load_request will call ReportResult().
  // This is the time to clean it up.
  url_load_request->set_delete_this_after_report();
  url_load_request->Load(stream_as_file, url);
}
