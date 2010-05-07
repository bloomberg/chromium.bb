// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/glue/plugins/pepper_plugin_module.h"

#include <set>

#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "third_party/ppapi/c/ppb_core.h"
#include "third_party/ppapi/c/ppb_device_context_2d.h"
#include "third_party/ppapi/c/ppb_image_data.h"
#include "third_party/ppapi/c/ppb_instance.h"
#include "third_party/ppapi/c/ppb_var.h"
#include "third_party/ppapi/c/ppp.h"
#include "third_party/ppapi/c/ppp_instance.h"
#include "third_party/ppapi/c/pp_module.h"
#include "third_party/ppapi/c/pp_resource.h"
#include "third_party/ppapi/c/pp_var.h"
#include "webkit/glue/plugins/pepper_device_context_2d.h"
#include "webkit/glue/plugins/pepper_image_data.h"
#include "webkit/glue/plugins/pepper_plugin_instance.h"
#include "webkit/glue/plugins/pepper_resource_tracker.h"
#include "webkit/glue/plugins/pepper_var.h"

typedef bool (*PPP_InitializeModuleFunc)(PP_Module, PPB_GetInterface);
typedef void (*PPP_ShutdownModuleFunc)();

namespace pepper {

namespace {

// Maintains all currently loaded plugin libs for validating PP_Module
// identifiers.
typedef std::set<PluginModule*> PluginModuleSet;

PluginModuleSet* GetLivePluginSet() {
  static PluginModuleSet live_plugin_libs;
  return &live_plugin_libs;
}

// PPB_Core --------------------------------------------------------------------

void AddRefResource(PP_Resource resource) {
  Resource* res = ResourceTracker::Get()->GetResource(resource);
  if (!res) {
    DLOG(WARNING) << "AddRef()ing a nonexistent resource";
    return;
  }
  res->AddRef();
}

void ReleaseResource(PP_Resource resource) {
  Resource* res = ResourceTracker::Get()->GetResource(resource);
  if (!res) {
    DLOG(WARNING) << "Release()ing a nonexistent resource";
    return;
  }
  res->Release();
}

const PPB_Core core_interface = {
  &AddRefResource,
  &ReleaseResource,
};

// GetInterface ----------------------------------------------------------------

const void* GetInterface(const char* name) {
  if (strcmp(name, PPB_CORE_INTERFACE) == 0)
    return &core_interface;
  if (strcmp(name, PPB_VAR_INTERFACE) == 0)
    return GetVarInterface();
  if (strcmp(name, PPB_INSTANCE_INTERFACE) == 0)
    return PluginInstance::GetInterface();
  if (strcmp(name, PPB_IMAGEDATA_INTERFACE) == 0)
    return ImageData::GetInterface();
  if (strcmp(name, PPB_DEVICECONTEXT2D_INTERFACE) == 0)
    return DeviceContext2D::GetInterface();
  return NULL;
}

}  // namespace

PluginModule::PluginModule(const FilePath& filename)
    : filename_(filename),
      initialized_(false),
      library_(0),
      ppp_get_interface_(NULL) {
  GetLivePluginSet()->insert(this);
}

PluginModule::~PluginModule() {
  // When the module is being deleted, there should be no more instances still
  // holding a reference to us.
  DCHECK(instances_.empty());

  GetLivePluginSet()->erase(this);

  if (library_) {
    PPP_ShutdownModuleFunc shutdown_module =
        reinterpret_cast<PPP_ShutdownModuleFunc>(
            base::GetFunctionPointerFromNativeLibrary(library_,
                                                      "PPP_ShutdownModule"));
    if (shutdown_module)
      shutdown_module();
    base::UnloadNativeLibrary(library_);
  }
}

// static
scoped_refptr<PluginModule> PluginModule::CreateModule(
    const FilePath& filename) {
  // FIXME(brettw) do uniquifying of the plugin here like the NPAPI one.

  scoped_refptr<PluginModule> lib(new PluginModule(filename));
  if (!lib->Load())
    lib = NULL;
  return lib;
}

// static
PluginModule* PluginModule::FromPPModule(PP_Module module) {
  PluginModule* lib = reinterpret_cast<PluginModule*>(module.id);
  if (GetLivePluginSet()->find(lib) == GetLivePluginSet()->end())
    return NULL;  // Invalid plugin.
  return lib;
}

bool PluginModule::Load() {
  if (initialized_)
    return true;
  initialized_ = true;

  library_ = base::LoadNativeLibrary(filename_);
  if (!library_)
    return false;

  // Save the GetInterface function pointer for later.
  ppp_get_interface_ =
      reinterpret_cast<PPP_GetInterfaceFunc>(
          base::GetFunctionPointerFromNativeLibrary(library_,
                                                    "PPP_GetInterface"));
  if (!ppp_get_interface_) {
    LOG(WARNING) << "No PPP_GetInterface in plugin library";
    return false;
  }

  // Call the plugin initialize function.
  PPP_InitializeModuleFunc initialize_module =
      reinterpret_cast<PPP_InitializeModuleFunc>(
          base::GetFunctionPointerFromNativeLibrary(library_,
                                                    "PPP_InitializeModule"));
  if (!initialize_module) {
    LOG(WARNING) << "No PPP_InitializeModule in plugin library";
    return false;
  }
  int retval = initialize_module(GetPPModule(), &GetInterface);
  if (retval != 0) {
    LOG(WARNING) << "PPP_InitializeModule returned failure " << retval;
    return false;
  }

  return true;
}

PP_Module PluginModule::GetPPModule() const {
  PP_Module ret;
  ret.id = reinterpret_cast<intptr_t>(this);
  return ret;
}

PluginInstance* PluginModule::CreateInstance(PluginDelegate* delegate) {
  const PPP_Instance* plugin_instance_interface =
      reinterpret_cast<const PPP_Instance*>(GetPluginInterface(
          PPP_INSTANCE_INTERFACE));
  if (!plugin_instance_interface) {
    LOG(WARNING) << "Plugin doesn't support instance interface, failing.";
    return NULL;
  }
  return new PluginInstance(delegate, this, plugin_instance_interface);
}

PluginInstance* PluginModule::GetSomeInstance() const {
  // This will generally crash later if there is not actually any instance to
  // return, so we force a crash now to make bugs easier to track down.
  CHECK(!instances_.empty());
  return *instances_.begin();
}

const void* PluginModule::GetPluginInterface(const char* name) const {
  if (!ppp_get_interface_)
    return NULL;
  return ppp_get_interface_(name);
}

void PluginModule::InstanceCreated(PluginInstance* instance) {
  instances_.insert(instance);
}

void PluginModule::InstanceDeleted(PluginInstance* instance) {
  instances_.erase(instance);
}

}  // namespace pepper
