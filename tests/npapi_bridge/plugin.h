/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */


#ifndef NATIVE_CLIENT_TESTS_NPAPI_BRIDGE_PLUGIN_H_
#define NATIVE_CLIENT_TESTS_NPAPI_BRIDGE_PLUGIN_H_

#include <map>

#include <nacl/nacl_imc.h>
#include <nacl/nacl_npapi.h>
#include "native_client/tests/npapi_bridge/base_object.h"

class Plugin {
 public:
  Plugin(NPP npp, const char* canvas, bool start_cycle_thread);
  ~Plugin();

  NPObject* GetScriptableObject();
  NPError SetWindow(NPWindow* window);
  bool Paint();

 private:
  NPP       npp_;
  NPObject* scriptable_object_;
  NPObject* window_object_;

  NPWindow* window_;
  void* bitmap_data_;
  size_t bitmap_size_;
};

class ScriptablePluginObject : public BaseObject {
 public:
  explicit ScriptablePluginObject(NPP npp)
    : npp_(npp) {
  }

  NPP npp() const { return npp_; }

  virtual bool HasMethod(NPIdentifier name);
  virtual bool Invoke(NPIdentifier name,
                      const NPVariant* args,
                      uint32_t arg_count,
                      NPVariant* result);
  virtual bool HasProperty(NPIdentifier name);
  virtual bool GetProperty(NPIdentifier name, NPVariant* result);

  static bool InitializeIdentifiers(NPObject* window_object,
                                    const char* canvas);

  static NPClass np_class;

  // Define the identifiers for everyone.
  static NPIdentifier id_append_child;
  static NPIdentifier id_bar;
  static NPIdentifier id_body;
  static NPIdentifier id_create_element;
  static NPIdentifier id_create_text_node;
  static NPIdentifier id_cycling;
  static NPIdentifier id_document;
  static NPIdentifier id_get_context;
  static NPIdentifier id_get_element_by_id;
  static NPIdentifier id_fill_rect;
  static NPIdentifier id_fill_style;
  static NPIdentifier id_inner_text;
  static NPIdentifier id_null_method;
  static NPIdentifier id_paint;
  static NPIdentifier id_proxy;
  static NPIdentifier id_set_proxy;
  static NPIdentifier id_text_content;
  static NPIdentifier id_use_proxy;

 private:
  // TODO(sehr): use DISALLOW_COPY_AND_ASSIGN.
  ScriptablePluginObject(const ScriptablePluginObject&);
  void operator=(const ScriptablePluginObject&);

  typedef bool (ScriptablePluginObject::*Method)(const NPVariant* args,
                                                 uint32_t arg_count,
                                                 NPVariant* result);

  typedef bool (ScriptablePluginObject::*Property)(NPVariant* result);

  // properties:
  bool GetBar(NPVariant* result);

  // methods:
  bool Cycling(const NPVariant* args, uint32_t arg_count, NPVariant* result);
  bool FillRect(const NPVariant* args, uint32_t arg_count, NPVariant* result);
  bool SetProxy(const NPVariant* args, uint32_t arg_count, NPVariant* result);
  bool UseProxy(const NPVariant* args, uint32_t arg_count, NPVariant* result);
  bool NullMethod(const NPVariant* args,
                  uint32_t arg_count,
                  NPVariant* result);
  bool Paint(const NPVariant* args, uint32_t arg_count, NPVariant* result);

  static void Notify(const char* url,
                     void* notify_data,
                     nacl::Handle handle);

  NPP npp_;

  static NPObject* window_object;

  static NPVariant canvas_name;
  static NPVariant proxy_canvas_name;

  static std::map<NPIdentifier, Method>* method_table;
  static std::map<NPIdentifier, Property>* property_table;
};

#endif  // NATIVE_CLIENT_TESTS_NPAPI_BRIDGE_PLUGIN_H_
