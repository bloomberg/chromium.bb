// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/plugin/pepper_entrypoints.h"

#include "remoting/client/plugin/chromoting_plugin.h"
#include "third_party/ppapi/c/pp_instance.h"
#include "third_party/ppapi/c/pp_module.h"
#include "third_party/ppapi/c/ppp_instance.h"
#include "third_party/ppapi/cpp/instance.h"
#include "third_party/ppapi/cpp/module.h"

static const int kModuleInitSuccess = 0;
static const int kModuleInitFailure = 1;

namespace remoting {

// Fork of ppapi::Module
//
// TODO(ajwong): Generalize this into something that other internal plugins can
// use.  Either that, or attempt to refactor the external ppapi C++ wrapper to
// make it friendly for multiple Modules in one process.  I think we can do this
// by:
//    1) Moving the singleton Module instance + C-bindings into another class
//       (eg., ModuleExporter) under a different gyp targe.
//    2) Extracting the idea of a "Browser" out of the module that returns
//       PPB_Core, etc.  This can be a singleton per process regardless of
//       module.
//    3) Migrate all PPB related objects to get data out of Browser interface
//       instead of Module::Get().
class ChromotingModule {
 public:
  ChromotingModule() {}

  // This function will be automatically called after the object is created.
  // This is where you can put functions that rely on other parts of the API,
  // now that the module has been created.
  virtual bool Init() { return true; }

  PP_Module pp_module() const { return pp_module_; }
  const PPB_Core& core() const { return *core_; }

  // Implements GetInterface for the browser to get plugin interfaces. Override
  // if you need to implement your own interface types that this wrapper
  // doesn't support.
  virtual const void* GetInstanceInterface(const char* interface_name) {
    if (strcmp(interface_name, PPP_INSTANCE_INTERFACE) == 0)
      return &instance_interface_;

    return NULL;
  }

  // Returns an interface in the browser.
  const void* GetBrowserInterface(const char* interface_name) {
    return get_browser_interface_(interface_name);
  }

  // Returns the object associated with this PP_Instance, or NULL if one is
  // not found.
  ChromotingPlugin* InstanceForPPInstance(PP_Instance instance) {
    InstanceMap::iterator found = current_instances_.find(instance);
    if (found == current_instances_.end())
      return NULL;
    return found->second;
  }

  // Sets the browser interface and calls the regular init function that
  // can be overridden by the base classes.
  //
  // TODO(brettw) make this private when I can figure out how to make the
  // initialize function a friend.
  bool InternalInit(PP_Module mod,
                    PPB_GetInterface get_browser_interface) {
    pp_module_ = mod;
    get_browser_interface_ = get_browser_interface;
    core_ = reinterpret_cast<const PPB_Core*>(GetBrowserInterface(
        PPB_CORE_INTERFACE));
    if (!core_)
      return false;  // Can't run without the core interface.

    return Init();
  }

  // Implementation of Global PPP functions ---------------------------------
  static int PPP_InitializeModule(PP_Module module_id,
                                  PPB_GetInterface get_browser_interface) {
    ChromotingModule* module = new ChromotingModule();
    if (!module)
      return kModuleInitFailure;

    if (!module->InternalInit(module_id, get_browser_interface)) {
      delete module;
      return kModuleInitFailure;
    }

    module_singleton_ = module;
    return kModuleInitSuccess;
  }

  static void PPP_ShutdownModule() {
    delete module_singleton_;
    module_singleton_ = NULL;
  }

  static const void* PPP_GetInterface(const char* interface_name) {
    if (!module_singleton_)
      return NULL;
    return module_singleton_->GetInstanceInterface(interface_name);
  }

 protected:
  virtual ChromotingPlugin* CreateInstance(PP_Instance instance) {
    const PPB_Instance* ppb_instance_funcs =
        reinterpret_cast<const PPB_Instance *>(
            module_singleton_->GetBrowserInterface(PPB_INSTANCE_INTERFACE));
    return new ChromotingPlugin(instance, ppb_instance_funcs);
  }

 private:
  static bool Instance_New(PP_Instance instance) {
    if (!module_singleton_)
      return false;
    ChromotingPlugin* obj = module_singleton_->CreateInstance(instance);
    if (obj) {
      module_singleton_->current_instances_[instance] = obj;
      return true;
    }
    return false;
  }

  static void Instance_Delete(PP_Instance instance) {
    if (!module_singleton_)
      return;
    ChromotingModule::InstanceMap::iterator found =
        module_singleton_->current_instances_.find(instance);
    if (found == module_singleton_->current_instances_.end())
      return;

    // Remove it from the map before deleting to try to catch reentrancy.
    ChromotingPlugin* obj = found->second;
    module_singleton_->current_instances_.erase(found);
    delete obj;
  }

  static bool Instance_Initialize(PP_Instance pp_instance,
                                  uint32_t argc,
                                  const char* argn[],
                                  const char* argv[]) {
    if (!module_singleton_)
      return false;
    ChromotingPlugin* instance =
        module_singleton_->InstanceForPPInstance(pp_instance);
    if (!instance)
      return false;
    return instance->Init(argc, argn, argv);
  }

  static bool Instance_HandleDocumentLoad(PP_Instance pp_instance,
                                          PP_Resource url_loader) {
    return false;
  }

  static bool Instance_HandleEvent(PP_Instance pp_instance,
                                   const PP_Event* event) {
    if (!module_singleton_)
      return false;
    ChromotingPlugin* instance =
        module_singleton_->InstanceForPPInstance(pp_instance);
    if (!instance)
      return false;
    return instance->HandleEvent(*event);
  }

  static PP_Var Instance_GetInstanceObject(PP_Instance pp_instance) {
    PP_Var var;
    var.type = PP_VarType_Void;
    return var;
  }

  static void Instance_ViewChanged(PP_Instance pp_instance,
                                   const PP_Rect* position,
                                   const PP_Rect* clip) {
    if (!module_singleton_)
      return;
    ChromotingPlugin* instance =
        module_singleton_->InstanceForPPInstance(pp_instance);
    if (!instance)
      return;
    instance->ViewChanged(*position, *clip);
  }

  // Bindings and identifiers to and from the browser.
  PP_Module pp_module_;
  PPB_GetInterface get_browser_interface_;
  PPB_Core const* core_;

  // Instance tracking.
  typedef std::map<PP_Instance, ChromotingPlugin*> InstanceMap;
  InstanceMap current_instances_;

  // Static members for the ppapi C-bridge.
  static PPP_Instance instance_interface_;
  static ChromotingModule* module_singleton_;

  DISALLOW_COPY_AND_ASSIGN(ChromotingModule);
};

ChromotingModule* ChromotingModule::module_singleton_ = NULL;

PPP_Instance ChromotingModule::instance_interface_ = {
  &ChromotingModule::Instance_New,
  &ChromotingModule::Instance_Delete,
  &ChromotingModule::Instance_Initialize,
  &ChromotingModule::Instance_HandleDocumentLoad,
  &ChromotingModule::Instance_HandleEvent,
  &ChromotingModule::Instance_GetInstanceObject,
  &ChromotingModule::Instance_ViewChanged,
};

// Implementation of Global PPP functions ---------------------------------
//
// TODO(ajwong): This is to get around friending issues. Fix it after we decide
// whether or not ChromotingModule should be generalized.
int PPP_InitializeModule(PP_Module module_id,
                         PPB_GetInterface get_browser_interface) {
  return ChromotingModule::PPP_InitializeModule(module_id,
                                                get_browser_interface);
}

void PPP_ShutdownModule() {
  return ChromotingModule::PPP_ShutdownModule();
}

const void* PPP_GetInterface(const char* interface_name) {
  return ChromotingModule::PPP_GetInterface(interface_name);
}

}  // namespace remoting
