/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */


#ifndef NATIVE_CLIENT_TESTS_NPAPI_PI_PLUGIN_H_
#define NATIVE_CLIENT_TESTS_NPAPI_PI_PLUGIN_H_

#include <pthread.h>
#include <nacl/nacl_npapi.h>
#include <map>

#include "native_client/tests/npapi_pi/base_object.h"

class Plugin {
  NPP       npp_;
  NPObject* scriptable_object_;

  NPWindow* window_;
  void* bitmap_data_;
  size_t bitmap_size_;

  bool quit_;
  pthread_t thread_;
  double pi_;

  static void* pi(void* param);

 public:
  explicit Plugin(NPP npp);
  ~Plugin();

  NPObject* GetScriptableObject();
  NPError SetWindow(NPWindow* window);
  bool Paint();
  bool quit() const {
    return quit_;
  }
  double pi() const {
    return pi_;
  }
};

class ScriptablePluginObject : public BaseObject {
  NPP npp_;

  typedef bool (ScriptablePluginObject::*Method)(const NPVariant* args,
                                                 uint32_t arg_count,
                                                 NPVariant* result);
  typedef bool (ScriptablePluginObject::*Property)(NPVariant* result);

  bool Paint(const NPVariant* args, uint32_t arg_count, NPVariant* result);

 public:
  explicit ScriptablePluginObject(NPP npp)
    : npp_(npp) {
  }

  virtual bool HasMethod(NPIdentifier name);
  virtual bool Invoke(NPIdentifier name,
                      const NPVariant* args, uint32_t arg_count,
                      NPVariant* result);
  virtual bool HasProperty(NPIdentifier name);
  virtual bool GetProperty(NPIdentifier name, NPVariant* result);

  static bool InitializeIdentifiers();

  static NPClass np_class;

 private:
  static NPIdentifier id_paint;

  static std::map<NPIdentifier, Method>* method_table;
  static std::map<NPIdentifier, Property>* property_table;
};

#endif  // NATIVE_CLIENT_TESTS_NPAPI_PI_PLUGIN_H_
