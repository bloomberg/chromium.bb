// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/plugins/ppapi/plugin_module.h"

#include <set>

#include "base/command_line.h"
#include "base/message_loop.h"
#include "base/message_loop_proxy.h"
#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "base/time.h"
#include "ppapi/c/dev/ppb_buffer_dev.h"
#include "ppapi/c/dev/ppb_char_set_dev.h"
#include "ppapi/c/dev/ppb_cursor_control_dev.h"
#include "ppapi/c/dev/ppb_directory_reader_dev.h"
#include "ppapi/c/dev/ppb_file_io_dev.h"
#include "ppapi/c/dev/ppb_file_io_trusted_dev.h"
#include "ppapi/c/dev/ppb_file_system_dev.h"
#include "ppapi/c/dev/ppb_find_dev.h"
#include "ppapi/c/dev/ppb_font_dev.h"
#include "ppapi/c/dev/ppb_fullscreen_dev.h"
#include "ppapi/c/dev/ppb_graphics_3d_dev.h"
#include "ppapi/c/dev/ppb_opengles_dev.h"
#include "ppapi/c/dev/ppb_scrollbar_dev.h"
#include "ppapi/c/dev/ppb_testing_dev.h"
#include "ppapi/c/dev/ppb_transport_dev.h"
#include "ppapi/c/dev/ppb_url_util_dev.h"
#include "ppapi/c/dev/ppb_var_deprecated.h"
#include "ppapi/c/dev/ppb_video_decoder_dev.h"
#include "ppapi/c/dev/ppb_widget_dev.h"
#include "ppapi/c/dev/ppb_zoom_dev.h"
#include "ppapi/c/pp_module.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/pp_var.h"
#include "ppapi/c/ppb_class.h"
#include "ppapi/c/ppb_core.h"
#include "ppapi/c/ppb_graphics_2d.h"
#include "ppapi/c/ppb_image_data.h"
#include "ppapi/c/ppb_instance.h"
#include "ppapi/c/ppb_url_loader.h"
#include "ppapi/c/ppb_url_request_info.h"
#include "ppapi/c/ppb_url_response_info.h"
#include "ppapi/c/ppb_var.h"
#include "ppapi/c/ppp.h"
#include "ppapi/c/ppp_instance.h"
#include "ppapi/c/private/ppb_nacl_util_private.h"
#include "ppapi/c/trusted/ppb_image_data_trusted.h"
#include "ppapi/c/trusted/ppb_url_loader_trusted.h"
#include "webkit/plugins/ppapi/common.h"
#include "webkit/plugins/ppapi/plugin_object.h"
#include "webkit/plugins/ppapi/ppapi_plugin_instance.h"
#include "webkit/plugins/ppapi/ppb_audio_impl.h"
#include "webkit/plugins/ppapi/ppb_buffer_impl.h"
#include "webkit/plugins/ppapi/ppb_char_set_impl.h"
#include "webkit/plugins/ppapi/ppb_cursor_control_impl.h"
#include "webkit/plugins/ppapi/ppb_directory_reader_impl.h"
#include "webkit/plugins/ppapi/ppb_file_chooser_impl.h"
#include "webkit/plugins/ppapi/ppb_file_io_impl.h"
#include "webkit/plugins/ppapi/ppb_file_ref_impl.h"
#include "webkit/plugins/ppapi/ppb_file_system_impl.h"
#include "webkit/plugins/ppapi/ppb_flash.h"
#include "webkit/plugins/ppapi/ppb_flash_impl.h"
#include "webkit/plugins/ppapi/ppb_font_impl.h"
#include "webkit/plugins/ppapi/ppb_graphics_2d_impl.h"
#include "webkit/plugins/ppapi/ppb_image_data_impl.h"
#include "webkit/plugins/ppapi/ppb_nacl_util_private_impl.h"
#include "webkit/plugins/ppapi/ppb_pdf.h"
#include "webkit/plugins/ppapi/ppb_pdf_impl.h"
#include "webkit/plugins/ppapi/ppb_scrollbar_impl.h"
#include "webkit/plugins/ppapi/ppb_transport_impl.h"
#include "webkit/plugins/ppapi/ppb_url_loader_impl.h"
#include "webkit/plugins/ppapi/ppb_url_request_info_impl.h"
#include "webkit/plugins/ppapi/ppb_url_response_info_impl.h"
#include "webkit/plugins/ppapi/ppb_url_util_impl.h"
#include "webkit/plugins/ppapi/ppb_video_decoder_impl.h"
#include "webkit/plugins/ppapi/ppb_widget_impl.h"
#include "webkit/plugins/ppapi/resource_tracker.h"
#include "webkit/plugins/ppapi/var.h"
#include "webkit/plugins/ppapi/var_object_class.h"

#ifdef ENABLE_GPU
#include "webkit/plugins/ppapi/ppb_graphics_3d_impl.h"
#endif  // ENABLE_GPU

namespace webkit {
namespace ppapi {

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

double GetTickTime() {
  // TODO(brettw) http://code.google.com/p/chromium/issues/detail?id=57448
  // This should be a tick timer rather than wall clock time, but needs to
  // match message times, which also currently use wall clock time.
  return GetTime();
}

void CallOnMainThread(int delay_in_msec,
                      PP_CompletionCallback callback,
                      int32_t result) {
  GetMainThreadMessageLoop()->PostDelayedTask(
      FROM_HERE,
      NewRunnableFunction(callback.func, callback.user_data, result),
      delay_in_msec);
}

PP_Bool IsMainThread() {
  return BoolToPPBool(GetMainThreadMessageLoop()->BelongsToCurrentThread());
}

const PPB_Core core_interface = {
  &AddRefResource,
  &ReleaseResource,
  &MemAlloc,
  &MemFree,
  &GetTime,
  &GetTickTime,
  &CallOnMainThread,
  &IsMainThread
};

// PPB_Testing -----------------------------------------------------------------

PP_Bool ReadImageData(PP_Resource device_context_2d,
                      PP_Resource image,
                      const PP_Point* top_left) {
  scoped_refptr<PPB_Graphics2D_Impl> context(
      Resource::GetAs<PPB_Graphics2D_Impl>(device_context_2d));
  if (!context.get())
    return PP_FALSE;
  return BoolToPPBool(context->ReadImageData(image, top_left));
}

void RunMessageLoop() {
  bool old_state = MessageLoop::current()->NestableTasksAllowed();
  MessageLoop::current()->SetNestableTasksAllowed(true);
  MessageLoop::current()->Run();
  MessageLoop::current()->SetNestableTasksAllowed(old_state);
}

void QuitMessageLoop() {
  MessageLoop::current()->QuitNow();
}

uint32_t GetLiveObjectCount(PP_Module module_id) {
  PluginModule* module = ResourceTracker::Get()->GetModule(module_id);
  if (!module)
    return static_cast<uint32_t>(-1);
  return ResourceTracker::Get()->GetLiveObjectsForModule(module);
}

const PPB_Testing_Dev testing_interface = {
  &ReadImageData,
  &RunMessageLoop,
  &QuitMessageLoop,
  &GetLiveObjectCount
};

// GetInterface ----------------------------------------------------------------

const void* GetInterface(const char* name) {
  // Please keep alphabetized by interface macro name with "special" stuff at
  // the bottom.
  if (strcmp(name, PPB_AUDIO_CONFIG_DEV_INTERFACE) == 0)
    return PPB_AudioConfig_Impl::GetInterface();
  if (strcmp(name, PPB_AUDIO_DEV_INTERFACE) == 0)
    return PPB_Audio_Impl::GetInterface();
  if (strcmp(name, PPB_AUDIO_TRUSTED_DEV_INTERFACE) == 0)
    return PPB_Audio_Impl::GetTrustedInterface();
  if (strcmp(name, PPB_BUFFER_DEV_INTERFACE) == 0)
    return PPB_Buffer_Impl::GetInterface();
  if (strcmp(name, PPB_CHAR_SET_DEV_INTERFACE) == 0)
    return PPB_CharSet_Impl::GetInterface();
  if (strcmp(name, PPB_CLASS_INTERFACE) == 0)
    return VarObjectClass::GetInterface();
  if (strcmp(name, PPB_CORE_INTERFACE) == 0)
    return &core_interface;
  if (strcmp(name, PPB_CURSOR_CONTROL_DEV_INTERFACE) == 0)
    return GetCursorControlInterface();
  if (strcmp(name, PPB_DIRECTORYREADER_DEV_INTERFACE) == 0)
    return PPB_DirectoryReader_Impl::GetInterface();
  if (strcmp(name, PPB_FILECHOOSER_DEV_INTERFACE) == 0)
    return PPB_FileChooser_Impl::GetInterface();
  if (strcmp(name, PPB_FILEIO_DEV_INTERFACE) == 0)
    return PPB_FileIO_Impl::GetInterface();
  if (strcmp(name, PPB_NACLUTIL_PRIVATE_INTERFACE) == 0)
    return PPB_NaClUtil_Private_Impl::GetInterface();
  if (strcmp(name, PPB_FILEIOTRUSTED_DEV_INTERFACE) == 0)
    return PPB_FileIO_Impl::GetTrustedInterface();
  if (strcmp(name, PPB_FILEREF_DEV_INTERFACE) == 0)
    return PPB_FileRef_Impl::GetInterface();
  if (strcmp(name, PPB_FILESYSTEM_DEV_INTERFACE) == 0)
    return PPB_FileSystem_Impl::GetInterface();
  if (strcmp(name, PPB_FIND_DEV_INTERFACE) == 0)
    return PluginInstance::GetFindInterface();
  if (strcmp(name, PPB_FLASH_INTERFACE) == 0)
    return PPB_Flash_Impl::GetInterface();
  if (strcmp(name, PPB_FONT_DEV_INTERFACE) == 0)
    return PPB_Font_Impl::GetInterface();
  if (strcmp(name, PPB_FULLSCREEN_DEV_INTERFACE) == 0)
    return PluginInstance::GetFullscreenInterface();
  if (strcmp(name, PPB_GRAPHICS_2D_INTERFACE) == 0)
    return PPB_Graphics2D_Impl::GetInterface();
  if (strcmp(name, PPB_IMAGEDATA_INTERFACE) == 0)
    return PPB_ImageData_Impl::GetInterface();
  if (strcmp(name, PPB_IMAGEDATA_TRUSTED_INTERFACE) == 0)
    return PPB_ImageData_Impl::GetTrustedInterface();
  if (strcmp(name, PPB_INSTANCE_INTERFACE) == 0)
    return PluginInstance::GetInterface();
  if (strcmp(name, PPB_PDF_INTERFACE) == 0)
    return PPB_PDF_Impl::GetInterface();
  if (strcmp(name, PPB_SCROLLBAR_DEV_INTERFACE) == 0)
    return PPB_Scrollbar_Impl::GetInterface();
  if (strcmp(name, PPB_TRANSPORT_DEV_INTERFACE) == 0)
    return PPB_Transport_Impl::GetInterface();
  if (strcmp(name, PPB_URLLOADER_INTERFACE) == 0)
    return PPB_URLLoader_Impl::GetInterface();
  if (strcmp(name, PPB_URLLOADERTRUSTED_INTERFACE) == 0)
    return PPB_URLLoader_Impl::GetTrustedInterface();
  if (strcmp(name, PPB_URLREQUESTINFO_INTERFACE) == 0)
    return PPB_URLRequestInfo_Impl::GetInterface();
  if (strcmp(name, PPB_URLRESPONSEINFO_INTERFACE) == 0)
    return PPB_URLResponseInfo_Impl::GetInterface();
  if (strcmp(name, PPB_URLUTIL_DEV_INTERFACE) == 0)
    return PPB_UrlUtil_Impl::GetInterface();
  if (strcmp(name, PPB_VAR_DEPRECATED_INTERFACE) == 0)
    return Var::GetDeprecatedInterface();
  if (strcmp(name, PPB_VAR_INTERFACE) == 0)
    return Var::GetInterface();
  if (strcmp(name, PPB_VIDEODECODER_DEV_INTERFACE) == 0)
    return PPB_VideoDecoder_Impl::GetInterface();
  if (strcmp(name, PPB_WIDGET_DEV_INTERFACE) == 0)
    return PPB_Widget_Impl::GetInterface();
  if (strcmp(name, PPB_ZOOM_DEV_INTERFACE) == 0)
    return PluginInstance::GetZoomInterface();

#ifdef ENABLE_GPU
  if (strcmp(name, PPB_GRAPHICS_3D_DEV_INTERFACE) == 0)
    return PPB_Graphics3D_Impl::GetInterface();
  if (strcmp(name, PPB_OPENGLES_DEV_INTERFACE) == 0)
    return PPB_Graphics3D_Impl::GetOpenGLESInterface();
#endif  // ENABLE_GPU

  // Only support the testing interface when the command line switch is
  // specified. This allows us to prevent people from (ab)using this interface
  // in production code.
  if (strcmp(name, PPB_TESTING_DEV_INTERFACE) == 0) {
    if (CommandLine::ForCurrentProcess()->HasSwitch("enable-pepper-testing"))
      return &testing_interface;
  }
  return NULL;
}

// Gets the PPAPI entry points from the given library and places them into the
// given structure. Returns true on success.
bool LoadEntryPointsFromLibrary(const base::NativeLibrary& library,
                                PluginModule::EntryPoints* entry_points) {
  entry_points->get_interface =
      reinterpret_cast<PluginModule::GetInterfaceFunc>(
          base::GetFunctionPointerFromNativeLibrary(library,
                                                    "PPP_GetInterface"));
  if (!entry_points->get_interface) {
    LOG(WARNING) << "No PPP_GetInterface in plugin library";
    return false;
  }

  entry_points->initialize_module =
      reinterpret_cast<PluginModule::PPP_InitializeModuleFunc>(
          base::GetFunctionPointerFromNativeLibrary(library,
                                                    "PPP_InitializeModule"));
  if (!entry_points->initialize_module) {
    LOG(WARNING) << "No PPP_InitializeModule in plugin library";
    return false;
  }

  // It's okay for PPP_ShutdownModule to not be defined and shutdown_module to
  // be NULL.
  entry_points->shutdown_module =
      reinterpret_cast<PluginModule::PPP_ShutdownModuleFunc>(
          base::GetFunctionPointerFromNativeLibrary(library,
                                                    "PPP_ShutdownModule"));

  return true;
}

}  // namespace

PluginModule::EntryPoints::EntryPoints()
    : get_interface(NULL),
      initialize_module(NULL),
      shutdown_module(NULL) {
}

// PluginModule ----------------------------------------------------------------

PluginModule::PluginModule() : library_(NULL) {
  pp_module_ = ResourceTracker::Get()->AddModule(this);
  GetMainThreadMessageLoop();  // Initialize the main thread message loop.
  GetLivePluginSet()->insert(this);
}

PluginModule::~PluginModule() {
  // Free all the plugin objects. This will automatically clear the back-
  // pointer from the NPObject so WebKit can't call into the plugin any more.
  //
  // Swap out the set so we can delete from it (the objects will try to
  // unregister themselves inside the delete call).
  PluginObjectSet plugin_object_copy;
  live_plugin_objects_.swap(plugin_object_copy);
  for (PluginObjectSet::iterator i = live_plugin_objects_.begin();
       i != live_plugin_objects_.end(); ++i)
    delete *i;

  // When the module is being deleted, there should be no more instances still
  // holding a reference to us.
  DCHECK(instances_.empty());

  GetLivePluginSet()->erase(this);

  if (entry_points_.shutdown_module)
    entry_points_.shutdown_module();

  if (library_)
    base::UnloadNativeLibrary(library_);

  ResourceTracker::Get()->ModuleDeleted(pp_module_);
}

bool PluginModule::InitAsInternalPlugin(const EntryPoints& entry_points) {
  entry_points_ = entry_points;
  return InitializeModule();
}

bool PluginModule::InitAsLibrary(const FilePath& path) {
  base::NativeLibrary library = base::LoadNativeLibrary(path);
  if (!library)
    return false;

  if (!LoadEntryPointsFromLibrary(library, &entry_points_) ||
      !InitializeModule()) {
    base::UnloadNativeLibrary(library);
    return false;
  }

  library_ = library;
  return true;
}

void PluginModule::InitAsProxied(
    PluginDelegate::OutOfProcessProxy* out_of_process_proxy) {
  DCHECK(!out_of_process_proxy_.get());
  out_of_process_proxy_.reset(out_of_process_proxy);
}

// static
const PPB_Core* PluginModule::GetCore() {
  return &core_interface;
}

// static
PluginModule::GetInterfaceFunc PluginModule::GetLocalGetInterfaceFunc() {
  return &GetInterface;
}

PluginInstance* PluginModule::CreateInstance(PluginDelegate* delegate) {
  const PPP_Instance* plugin_instance_interface =
      reinterpret_cast<const PPP_Instance*>(GetPluginInterface(
          PPP_INSTANCE_INTERFACE));
  if (!plugin_instance_interface) {
    LOG(WARNING) << "Plugin doesn't support instance interface, failing.";
    return NULL;
  }
  PluginInstance* instance = new PluginInstance(delegate, this,
                                                plugin_instance_interface);
  if (out_of_process_proxy_.get())
    out_of_process_proxy_->AddInstance(instance->pp_instance());
  return instance;
}

PluginInstance* PluginModule::GetSomeInstance() const {
  // This will generally crash later if there is not actually any instance to
  // return, so we force a crash now to make bugs easier to track down.
  CHECK(!instances_.empty());
  return *instances_.begin();
}

const void* PluginModule::GetPluginInterface(const char* name) const {
  if (out_of_process_proxy_.get())
    return out_of_process_proxy_->GetProxiedInterface(name);

  // In-process plugins.
  if (!entry_points_.get_interface)
    return NULL;
  return entry_points_.get_interface(name);
}

void PluginModule::InstanceCreated(PluginInstance* instance) {
  instances_.insert(instance);
}

void PluginModule::InstanceDeleted(PluginInstance* instance) {
  if (out_of_process_proxy_.get())
    out_of_process_proxy_->RemoveInstance(instance->pp_instance());
  instances_.erase(instance);
}

void PluginModule::AddNPObjectVar(ObjectVar* object_var) {
  DCHECK(np_object_to_object_var_.find(object_var->np_object()) ==
         np_object_to_object_var_.end()) << "ObjectVar already in map";
  np_object_to_object_var_[object_var->np_object()] = object_var;
}

void PluginModule::RemoveNPObjectVar(ObjectVar* object_var) {
  NPObjectToObjectVarMap::iterator found =
      np_object_to_object_var_.find(object_var->np_object());
  if (found == np_object_to_object_var_.end()) {
    NOTREACHED() << "ObjectVar not registered.";
    return;
  }
  if (found->second != object_var) {
    NOTREACHED() << "ObjectVar doesn't match.";
    return;
  }
  np_object_to_object_var_.erase(found);
}

ObjectVar* PluginModule::ObjectVarForNPObject(NPObject* np_object) const {
  NPObjectToObjectVarMap::const_iterator found =
      np_object_to_object_var_.find(np_object);
  if (found == np_object_to_object_var_.end())
    return NULL;
  return found->second;
}

void PluginModule::AddPluginObject(PluginObject* plugin_object) {
  DCHECK(live_plugin_objects_.find(plugin_object) ==
         live_plugin_objects_.end());
  live_plugin_objects_.insert(plugin_object);
}

void PluginModule::RemovePluginObject(PluginObject* plugin_object) {
  // Don't actually verify that the object is in the set since during module
  // deletion we'll be in the process of freeing them.
  live_plugin_objects_.erase(plugin_object);
}

bool PluginModule::InitializeModule() {
  DCHECK(!out_of_process_proxy_.get()) << "Don't call for proxied modules.";
  int retval = entry_points_.initialize_module(pp_module(), &GetInterface);
  if (retval != 0) {
    LOG(WARNING) << "PPP_InitializeModule returned failure " << retval;
    return false;
  }
  return true;
}

}  // namespace ppapi
}  // namespace webkit

