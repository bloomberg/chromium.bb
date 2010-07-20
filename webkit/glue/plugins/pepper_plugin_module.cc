// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/glue/plugins/pepper_plugin_module.h"

#include <set>

#include "base/command_line.h"
#include "base/message_loop.h"
#include "base/message_loop_proxy.h"
#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "base/time.h"
#include "third_party/ppapi/c/ppb_buffer.h"
#include "third_party/ppapi/c/ppb_core.h"
#include "third_party/ppapi/c/ppb_device_context_2d.h"
#include "third_party/ppapi/c/ppb_file_io.h"
#include "third_party/ppapi/c/ppb_file_io_trusted.h"
#include "third_party/ppapi/c/ppb_file_system.h"
#include "third_party/ppapi/c/ppb_image_data.h"
#include "third_party/ppapi/c/ppb_instance.h"
#include "third_party/ppapi/c/ppb_find.h"
#include "third_party/ppapi/c/ppb_font.h"
#include "third_party/ppapi/c/ppb_scrollbar.h"
#include "third_party/ppapi/c/ppb_testing.h"
#include "third_party/ppapi/c/ppb_url_loader.h"
#include "third_party/ppapi/c/ppb_url_request_info.h"
#include "third_party/ppapi/c/ppb_url_response_info.h"
#include "third_party/ppapi/c/ppb_var.h"
#include "third_party/ppapi/c/ppb_widget.h"
#include "third_party/ppapi/c/ppp.h"
#include "third_party/ppapi/c/ppp_instance.h"
#include "third_party/ppapi/c/pp_module.h"
#include "third_party/ppapi/c/pp_resource.h"
#include "third_party/ppapi/c/pp_var.h"
#include "webkit/glue/plugins/pepper_buffer.h"
#include "webkit/glue/plugins/pepper_device_context_2d.h"
#include "webkit/glue/plugins/pepper_directory_reader.h"
#include "webkit/glue/plugins/pepper_file_io.h"
#include "webkit/glue/plugins/pepper_file_ref.h"
#include "webkit/glue/plugins/pepper_file_system.h"
#include "webkit/glue/plugins/pepper_font.h"
#include "webkit/glue/plugins/pepper_image_data.h"
#include "webkit/glue/plugins/pepper_plugin_instance.h"
#include "webkit/glue/plugins/pepper_private.h"
#include "webkit/glue/plugins/pepper_resource_tracker.h"
#include "webkit/glue/plugins/pepper_scrollbar.h"
#include "webkit/glue/plugins/pepper_url_loader.h"
#include "webkit/glue/plugins/pepper_url_request_info.h"
#include "webkit/glue/plugins/pepper_url_response_info.h"
#include "webkit/glue/plugins/pepper_var.h"
#include "webkit/glue/plugins/pepper_widget.h"
#include "webkit/glue/plugins/ppb_private.h"

namespace pepper {

namespace {

// Maintains all currently loaded plugin libs for validating PP_Module
// identifiers.
typedef std::set<PluginModule*> PluginModuleSet;

PluginModuleSet* GetLivePluginSet() {
  static PluginModuleSet live_plugin_libs;
  return &live_plugin_libs;
}

base::MessageLoopProxy* GetMainThreadMessageLoop() {
  static scoped_refptr<base::MessageLoopProxy> proxy(
      base::MessageLoopProxy::CreateForCurrentThread());
  return proxy.get();
}

// PPB_Core --------------------------------------------------------------------

void AddRefResource(PP_Resource resource) {
  if (!ResourceTracker::Get()->AddRefResource(resource)) {
    DLOG(WARNING) << "AddRefResource()ing a nonexistent resource";
  }
}

void ReleaseResource(PP_Resource resource) {
  if (!ResourceTracker::Get()->UnrefResource(resource)) {
    DLOG(WARNING) << "ReleaseResource()ing a nonexistent resource";
  }
}

void* MemAlloc(size_t num_bytes) {
  return malloc(num_bytes);
}

void MemFree(void* ptr) {
  free(ptr);
}

double GetTime() {
  return base::Time::Now().ToDoubleT();
}

void CallOnMainThread(int delay_in_msec,
                      PP_CompletionCallback callback,
                      int32_t result) {
  GetMainThreadMessageLoop()->PostDelayedTask(
      FROM_HERE,
      NewRunnableFunction(callback.func, callback.user_data, result),
      delay_in_msec);
}

const PPB_Core core_interface = {
  &AddRefResource,
  &ReleaseResource,
  &MemAlloc,
  &MemFree,
  &GetTime,
  &CallOnMainThread
};

// PPB_Testing -----------------------------------------------------------------

bool ReadImageData(PP_Resource device_context_2d,
                   PP_Resource image,
                   const PP_Point* top_left) {
  scoped_refptr<DeviceContext2D> context(
      Resource::GetAs<DeviceContext2D>(device_context_2d));
  if (!context.get())
    return false;
  return context->ReadImageData(image, top_left);
}

void RunMessageLoop() {
  bool old_state = MessageLoop::current()->NestableTasksAllowed();
  MessageLoop::current()->SetNestableTasksAllowed(true);
  MessageLoop::current()->Run();
  MessageLoop::current()->SetNestableTasksAllowed(old_state);
}

void QuitMessageLoop() {
  MessageLoop::current()->Quit();
}

const PPB_Testing testing_interface = {
  &ReadImageData,
  &RunMessageLoop,
  &QuitMessageLoop,
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
  if (strcmp(name, PPB_URLLOADER_INTERFACE) == 0)
    return URLLoader::GetInterface();
  if (strcmp(name, PPB_URLREQUESTINFO_INTERFACE) == 0)
    return URLRequestInfo::GetInterface();
  if (strcmp(name, PPB_URLRESPONSEINFO_INTERFACE) == 0)
    return URLResponseInfo::GetInterface();
  if (strcmp(name, PPB_BUFFER_INTERFACE) == 0)
    return Buffer::GetInterface();
  if (strcmp(name, PPB_FILEREF_INTERFACE) == 0)
    return FileRef::GetInterface();
  if (strcmp(name, PPB_FILEIO_INTERFACE) == 0)
    return FileIO::GetInterface();
  if (strcmp(name, PPB_FILEIOTRUSTED_INTERFACE) == 0)
    return FileIO::GetTrustedInterface();
  if (strcmp(name, PPB_FILESYSTEM_INTERFACE) == 0)
    return FileSystem::GetInterface();
  if (strcmp(name, PPB_DIRECTORYREADER_INTERFACE) == 0)
    return DirectoryReader::GetInterface();
  if (strcmp(name, PPB_WIDGET_INTERFACE) == 0)
    return Widget::GetInterface();
  if (strcmp(name, PPB_SCROLLBAR_INTERFACE) == 0)
    return Scrollbar::GetInterface();
  if (strcmp(name, PPB_FONT_INTERFACE) == 0)
    return Font::GetInterface();
  if (strcmp(name, PPB_FIND_INTERFACE) == 0)
    return PluginInstance::GetFindInterface();
  if (strcmp(name, PPB_PRIVATE_INTERFACE) == 0)
    return Private::GetInterface();

  // Only support the testing interface when the command line switch is
  // specified. This allows us to prevent people from (ab)using this interface
  // in production code.
  if (strcmp(name, PPB_TESTING_INTERFACE) == 0) {
    if (CommandLine::ForCurrentProcess()->HasSwitch("enable-pepper-testing"))
      return &testing_interface;
  }
  return NULL;
}

}  // namespace

PluginModule::PluginModule()
    : initialized_(false),
      library_(NULL) {
  GetMainThreadMessageLoop();  // Initialize the main thread message loop.
  GetLivePluginSet()->insert(this);
}

PluginModule::~PluginModule() {
  // When the module is being deleted, there should be no more instances still
  // holding a reference to us.
  DCHECK(instances_.empty());

  GetLivePluginSet()->erase(this);

  if (entry_points_.shutdown_module)
    entry_points_.shutdown_module();

  if (library_)
    base::UnloadNativeLibrary(library_);
}

// static
scoped_refptr<PluginModule> PluginModule::CreateModule(
    const FilePath& path) {
  // FIXME(brettw) do uniquifying of the plugin here like the NPAPI one.

  scoped_refptr<PluginModule> lib(new PluginModule());
  if (!lib->InitFromFile(path))
    return NULL;

  return lib;
}

scoped_refptr<PluginModule> PluginModule::CreateInternalModule(
    EntryPoints entry_points) {
  scoped_refptr<PluginModule> lib(new PluginModule());
  if (!lib->InitFromEntryPoints(entry_points))
    return NULL;

  return lib;
}

// static
PluginModule* PluginModule::FromPPModule(PP_Module module) {
  PluginModule* lib = reinterpret_cast<PluginModule*>(module);
  if (GetLivePluginSet()->find(lib) == GetLivePluginSet()->end())
    return NULL;  // Invalid plugin.
  return lib;
}

// static
const PPB_Core* PluginModule::GetCore() {
  return &core_interface;
}

bool PluginModule::InitFromEntryPoints(const EntryPoints& entry_points) {
  if (initialized_)
    return true;

  // Attempt to run the initialization funciton.
  int retval = entry_points.initialize_module(GetPPModule(), &GetInterface);
  if (retval != 0) {
    LOG(WARNING) << "PPP_InitializeModule returned failure " << retval;
    return false;
  }

  entry_points_ = entry_points;
  initialized_ = true;
  return true;
}

bool PluginModule::InitFromFile(const FilePath& path) {
  if (initialized_)
    return true;

  base::NativeLibrary library = base::LoadNativeLibrary(path);
  if (!library)
    return false;

  EntryPoints entry_points;
  if (!LoadEntryPoints(library, &entry_points) ||
      !InitFromEntryPoints(entry_points)) {
    base::UnloadNativeLibrary(library);
    return false;
  }

  // We let InitFromEntryPoints() handle setting the all the internal state
  // of the object other than the |library_| reference.
  library_ = library;
  return true;
}

// static
bool PluginModule::LoadEntryPoints(const base::NativeLibrary& library,
                                   EntryPoints* entry_points) {

  entry_points->get_interface =
      reinterpret_cast<PPP_GetInterfaceFunc>(
          base::GetFunctionPointerFromNativeLibrary(library,
                                                    "PPP_GetInterface"));
  if (!entry_points->get_interface) {
    LOG(WARNING) << "No PPP_GetInterface in plugin library";
    return false;
  }

  entry_points->initialize_module =
      reinterpret_cast<PPP_InitializeModuleFunc>(
          base::GetFunctionPointerFromNativeLibrary(library,
                                                    "PPP_InitializeModule"));
  if (!entry_points->initialize_module) {
    LOG(WARNING) << "No PPP_InitializeModule in plugin library";
    return false;
  }

  // It's okay for PPP_ShutdownModule to not be defined and shutdown_module to
  // be NULL.
  entry_points->shutdown_module =
      reinterpret_cast<PPP_ShutdownModuleFunc>(
          base::GetFunctionPointerFromNativeLibrary(library,
                                                    "PPP_ShutdownModule"));

  return true;
}

PP_Module PluginModule::GetPPModule() const {
  return reinterpret_cast<intptr_t>(this);
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
  if (!entry_points_.get_interface)
    return NULL;
  return entry_points_.get_interface(name);
}

void PluginModule::InstanceCreated(PluginInstance* instance) {
  instances_.insert(instance);
}

void PluginModule::InstanceDeleted(PluginInstance* instance) {
  instances_.erase(instance);
}

}  // namespace pepper
