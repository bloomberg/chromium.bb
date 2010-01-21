/*
 * Copyright 2008, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


#ifndef NATIVE_CLIENT_TESTS_NPAPI_BRIDGE_PLUGIN_H_
#define NATIVE_CLIENT_TESTS_NPAPI_BRIDGE_PLUGIN_H_

#include <map>

#include <nacl/nacl_htp.h>
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
                     nacl::HtpHandle handle);

  NPP npp_;

  static NPObject* window_object;

  static NPVariant canvas_name;
  static NPVariant proxy_canvas_name;

  static std::map<NPIdentifier, Method>* method_table;
  static std::map<NPIdentifier, Property>* property_table;
};

#endif  // NATIVE_CLIENT_TESTS_NPAPI_BRIDGE_PLUGIN_H_
