// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/plugins/ppapi/ppapi_unittest.h"

#include "ppapi/c/pp_var.h"
#include "ppapi/c/ppp_instance.h"
#include "webkit/plugins/ppapi/mock_plugin_delegate.h"
#include "webkit/plugins/ppapi/plugin_module.h"
#include "webkit/plugins/ppapi/ppapi_plugin_instance.h"

namespace webkit {
namespace ppapi {

namespace {

PpapiUnittest* current_unittest = NULL;

const void* MockGetInterface(const char* interface_name) {
  return current_unittest->GetMockInterface(interface_name);
}

int MockInitializeModule(PP_Module, PPB_GetInterface) {
  return PP_OK;
}

// PPP_Instance implementation ------------------------------------------------

PP_Bool Instance_DidCreate(PP_Instance pp_instance,
                           uint32_t argc,
                           const char* argn[],
                           const char* argv[]) {
  return PP_TRUE;
}

void Instance_DidDestroy(PP_Instance instance) {
}

void Instance_DidChangeView(PP_Instance pp_instance,
                            const PP_Rect* position,
                            const PP_Rect* clip) {
}

void Instance_DidChangeFocus(PP_Instance pp_instance, PP_Bool has_focus) {
}

PP_Bool Instance_HandleInputEvent(PP_Instance pp_instance,
                                  const PP_InputEvent* event) {
  return PP_FALSE;
}

PP_Bool Instance_HandleDocumentLoad(PP_Instance pp_instance,
                                    PP_Resource pp_url_loader) {
  return PP_FALSE;
}

PP_Var Instance_GetInstanceObject(PP_Instance pp_instance) {
  return PP_MakeUndefined();
}

static PPP_Instance mock_instance_interface = {
  &Instance_DidCreate,
  &Instance_DidDestroy,
  &Instance_DidChangeView,
  &Instance_DidChangeFocus,
  &Instance_HandleInputEvent,
  &Instance_HandleDocumentLoad,
  &Instance_GetInstanceObject
};

}  // namespace

// PpapiUnittest --------------------------------------------------------------

PpapiUnittest::PpapiUnittest() {
  DCHECK(!current_unittest);
  current_unittest = this;
}

PpapiUnittest::~PpapiUnittest() {
  DCHECK(current_unittest == this);
  current_unittest = NULL;
}

void PpapiUnittest::SetUp() {
  delegate_.reset(new MockPluginDelegate);

  // Initialize the mock module.
  module_ = new PluginModule(this);
  PluginModule::EntryPoints entry_points;
  entry_points.get_interface = &MockGetInterface;
  entry_points.initialize_module = &MockInitializeModule;
  ASSERT_TRUE(module_->InitAsInternalPlugin(entry_points));

  // Initialize the mock instance.
  instance_ = new PluginInstance(delegate_.get(), module(),
      static_cast<const PPP_Instance*>(
          GetMockInterface(PPP_INSTANCE_INTERFACE)));
}

void PpapiUnittest::TearDown() {
  instance_ = NULL;
  module_ = NULL;
}

const void* PpapiUnittest::GetMockInterface(const char* interface_name) const {
  if (strcmp(interface_name, PPP_INSTANCE_INTERFACE) == 0)
    return &mock_instance_interface;
  return NULL;
}

void PpapiUnittest::ShutdownModule() {
  DCHECK(instance_->HasOneRef());
  instance_ = NULL;
  DCHECK(module_->HasOneRef());
  module_ = NULL;
}

void PpapiUnittest::PluginModuleDestroyed(PluginModule* destroyed_module) {
  // Nothing needed (this is necessary to make the module compile).
}

}  // namespace ppapi
}  // namespace webkit
