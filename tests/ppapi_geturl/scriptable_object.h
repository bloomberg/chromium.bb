// Copyright 2010 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.
#ifndef TESTS_PPAPI_GETURL_SCRIPTABLE_OBJECT_H_
#define TESTS_PPAPI_GETURL_SCRIPTABLE_OBJECT_H_

#include <ppapi/c/pp_instance.h>
#include <ppapi/c/dev/ppp_class_deprecated.h>
#include <string>

// ppapi_geturl example is deliberately using C PPAPI interface.
// C++ PPAPI layer has pp::ScriptableObject (now deprecated) wrapper class.
class ScriptableObject {
 public:
  explicit ScriptableObject(PP_Instance instance);
  const PPP_Class_Deprecated* ppp_class() const;

  bool LoadUrl(std::string url, std::string* error);

 protected:
  static bool HasProperty(void* object,
                          PP_Var name,
                          PP_Var* exception);
  static bool HasMethod(void* object,
                        PP_Var name,
                        PP_Var* exception);
  static PP_Var GetProperty(void* object,
                            PP_Var name,
                            PP_Var* exception);
  static void GetAllPropertyNames(void* object,
                                  uint32_t* property_count,
                                  PP_Var** properties,
                                  PP_Var* exception);
  static void SetProperty(void* object,
                          PP_Var name,
                          PP_Var value,
                          PP_Var* exception);
  static void RemoveProperty(void* object,
                             PP_Var name,
                             PP_Var* exception);
  static PP_Var Call(void* object,
                     PP_Var method_name,
                     uint32_t argc,
                     PP_Var* argv,
                     PP_Var* exception);
  static PP_Var Construct(void* object,
                          uint32_t argc,
                          PP_Var* argv,
                          PP_Var* exception);
  static void Deallocate(void* object);

  PPP_Class_Deprecated ppp_class_;
  PP_Instance instance_;

  ScriptableObject(const ScriptableObject&);
  void operator=(const ScriptableObject&);
};

#endif  // TESTS_PPAPI_GETURL_SCRIPTABLE_OBJECT_H_

