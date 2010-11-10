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
#include "ppapi/c/trusted/ppb_image_data_trusted.h"
#include "ppapi/c/trusted/ppb_url_loader_trusted.h"
#include "ppapi/proxy/host_dispatcher.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "webkit/glue/plugins/pepper_audio.h"
#include "webkit/glue/plugins/pepper_buffer.h"
#include "webkit/glue/plugins/pepper_common.h"
#include "webkit/glue/plugins/pepper_char_set.h"
#include "webkit/glue/plugins/pepper_class.h"
#include "webkit/glue/plugins/pepper_cursor_control.h"
#include "webkit/glue/plugins/pepper_directory_reader.h"
#include "webkit/glue/plugins/pepper_file_chooser.h"
#include "webkit/glue/plugins/pepper_file_io.h"
#include "webkit/glue/plugins/pepper_file_ref.h"
#include "webkit/glue/plugins/pepper_file_system.h"
#include "webkit/glue/plugins/pepper_font.h"
#include "webkit/glue/plugins/pepper_graphics_2d.h"
#include "webkit/glue/plugins/pepper_image_data.h"
#include "webkit/glue/plugins/pepper_plugin_instance.h"
#include "webkit/glue/plugins/pepper_plugin_object.h"
#include "webkit/glue/plugins/pepper_private.h"
#include "webkit/glue/plugins/pepper_private2.h"
#include "webkit/glue/plugins/pepper_resource_tracker.h"
#include "webkit/glue/plugins/pepper_scrollbar.h"
#include "webkit/glue/plugins/pepper_transport.h"
#include "webkit/glue/plugins/pepper_url_loader.h"
#include "webkit/glue/plugins/pepper_url_request_info.h"
#include "webkit/glue/plugins/pepper_url_response_info.h"
#include "webkit/glue/plugins/pepper_url_util.h"
#include "webkit/glue/plugins/pepper_var.h"
#include "webkit/glue/plugins/pepper_video_decoder.h"
#include "webkit/glue/plugins/pepper_widget.h"
#include "webkit/glue/plugins/ppb_private.h"
#include "webkit/glue/plugins/ppb_private2.h"

#ifdef ENABLE_GPU
#include "webkit/glue/plugins/pepper_graphics_3d.h"
#endif  // ENABLE_GPU

#if defined(OS_POSIX)
#include "ipc/ipc_channel_posix.h"
#endif

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
  scoped_refptr<Graphics2D> context(
      Resource::GetAs<Graphics2D>(device_context_2d));
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
  if (strcmp(name, PPB_CORE_INTERFACE) == 0)
    return &core_interface;
  if (strcmp(name, PPB_VAR_DEPRECATED_INTERFACE) == 0)
    return Var::GetDeprecatedInterface();
  if (strcmp(name, PPB_VAR_INTERFACE) == 0)
    return Var::GetInterface();
  if (strcmp(name, PPB_INSTANCE_INTERFACE) == 0)
    return PluginInstance::GetInterface();
  if (strcmp(name, PPB_IMAGEDATA_INTERFACE) == 0)
    return ImageData::GetInterface();
  if (strcmp(name, PPB_IMAGEDATA_TRUSTED_INTERFACE) == 0)
    return ImageData::GetTrustedInterface();
  if (strcmp(name, PPB_AUDIO_CONFIG_DEV_INTERFACE) == 0)
    return AudioConfig::GetInterface();
  if (strcmp(name, PPB_AUDIO_DEV_INTERFACE) == 0)
    return Audio::GetInterface();
  if (strcmp(name, PPB_AUDIO_TRUSTED_DEV_INTERFACE) == 0)
    return Audio::GetTrustedInterface();
  if (strcmp(name, PPB_GRAPHICS_2D_INTERFACE) == 0)
    return Graphics2D::GetInterface();
#ifdef ENABLE_GPU
  if (strcmp(name, PPB_GRAPHICS_3D_DEV_INTERFACE) == 0)
    return Graphics3D::GetInterface();
  if (strcmp(name, PPB_OPENGLES_DEV_INTERFACE) == 0)
    return Graphics3D::GetOpenGLESInterface();
#endif  // ENABLE_GPU
  if (strcmp(name, PPB_TRANSPORT_DEV_INTERFACE) == 0)
    return Transport::GetInterface();
  if (strcmp(name, PPB_URLLOADER_INTERFACE) == 0)
    return URLLoader::GetInterface();
  if (strcmp(name, PPB_URLLOADERTRUSTED_INTERFACE) == 0)
    return URLLoader::GetTrustedInterface();
  if (strcmp(name, PPB_URLREQUESTINFO_INTERFACE) == 0)
    return URLRequestInfo::GetInterface();
  if (strcmp(name, PPB_URLRESPONSEINFO_INTERFACE) == 0)
    return URLResponseInfo::GetInterface();
  if (strcmp(name, PPB_BUFFER_DEV_INTERFACE) == 0)
    return Buffer::GetInterface();
  if (strcmp(name, PPB_FILEREF_DEV_INTERFACE) == 0)
    return FileRef::GetInterface();
  if (strcmp(name, PPB_FILEIO_DEV_INTERFACE) == 0)
    return FileIO::GetInterface();
  if (strcmp(name, PPB_FILEIOTRUSTED_DEV_INTERFACE) == 0)
    return FileIO::GetTrustedInterface();
  if (strcmp(name, PPB_FILESYSTEM_DEV_INTERFACE) == 0)
    return FileSystem::GetInterface();
  if (strcmp(name, PPB_DIRECTORYREADER_DEV_INTERFACE) == 0)
    return DirectoryReader::GetInterface();
  if (strcmp(name, PPB_WIDGET_DEV_INTERFACE) == 0)
    return Widget::GetInterface();
  if (strcmp(name, PPB_SCROLLBAR_DEV_INTERFACE) == 0)
    return Scrollbar::GetInterface();
  if (strcmp(name, PPB_FONT_DEV_INTERFACE) == 0)
    return Font::GetInterface();
  if (strcmp(name, PPB_FIND_DEV_INTERFACE) == 0)
    return PluginInstance::GetFindInterface();
  if (strcmp(name, PPB_FULLSCREEN_DEV_INTERFACE) == 0)
    return PluginInstance::GetFullscreenInterface();
  if (strcmp(name, PPB_URLUTIL_DEV_INTERFACE) == 0)
    return UrlUtil::GetInterface();
  if (strcmp(name, PPB_PRIVATE_INTERFACE) == 0)
    return Private::GetInterface();
  if (strcmp(name, PPB_PRIVATE2_INTERFACE) == 0)
    return Private2::GetInterface();
  if (strcmp(name, PPB_FILECHOOSER_DEV_INTERFACE) == 0)
    return FileChooser::GetInterface();
  if (strcmp(name, PPB_VIDEODECODER_DEV_INTERFACE) == 0)
    return VideoDecoder::GetInterface();
  if (strcmp(name, PPB_CHAR_SET_DEV_INTERFACE) == 0)
    return CharSet::GetInterface();
  if (strcmp(name, PPB_CURSOR_CONTROL_DEV_INTERFACE) == 0)
    return GetCursorControlInterface();
  if (strcmp(name, PPB_ZOOM_DEV_INTERFACE) == 0)
    return PluginInstance::GetZoomInterface();
  if (strcmp(name, PPB_CLASS_INTERFACE) == 0)
    return VarObjectClass::GetInterface();

  // Only support the testing interface when the command line switch is
  // specified. This allows us to prevent people from (ab)using this interface
  // in production code.
  if (strcmp(name, PPB_TESTING_DEV_INTERFACE) == 0) {
    if (CommandLine::ForCurrentProcess()->HasSwitch("enable-pepper-testing"))
      return &testing_interface;
  }
  return NULL;
}

}  // namespace

PluginModule::PluginModule()
    : initialized_(false),
      library_(NULL) {
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

// static
scoped_refptr<PluginModule> PluginModule::CreateModule(
    const FilePath& path) {
  // FIXME(brettw) do uniquifying of the plugin here like the NPAPI one.

  scoped_refptr<PluginModule> lib(new PluginModule());
  if (!lib->InitFromFile(path))
    return NULL;

  return lib;
}

// static
scoped_refptr<PluginModule> PluginModule::CreateInternalModule(
    EntryPoints entry_points) {
  scoped_refptr<PluginModule> lib(new PluginModule());
  if (!lib->InitFromEntryPoints(entry_points))
    return NULL;

  return lib;
}

// static
scoped_refptr<PluginModule> PluginModule::CreateOutOfProcessModule(
    MessageLoop* ipc_message_loop,
    const IPC::ChannelHandle& handle,
    base::WaitableEvent* shutdown_event) {
  scoped_refptr<PluginModule> lib(new PluginModule);
  if (!lib->InitForOutOfProcess(ipc_message_loop, handle, shutdown_event))
    return NULL;
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
  int retval = entry_points.initialize_module(pp_module(), &GetInterface);
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

bool PluginModule::InitForOutOfProcess(MessageLoop* ipc_message_loop,
                                       const IPC::ChannelHandle& handle,
                                       base::WaitableEvent* shutdown_event) {
  const PPB_Var_Deprecated* var_interface =
      reinterpret_cast<const PPB_Var_Deprecated*>(
          GetInterface(PPB_VAR_DEPRECATED_INTERFACE));
  dispatcher_.reset(new pp::proxy::HostDispatcher(var_interface,
                                                  pp_module(), &GetInterface));

#if defined(OS_POSIX)
  // If we received a ChannelHandle, register it now.
  if (handle.socket.fd >= 0)
    IPC::AddChannelSocket(handle.name, handle.socket.fd);
#endif

  if (!dispatcher_->InitWithChannel(ipc_message_loop, handle.name, true,
                                    shutdown_event)) {
    dispatcher_.reset();
    return false;
  }

  bool init_result = false;
  dispatcher_->Send(new PpapiMsg_InitializeModule(pp_module(), &init_result));

  if (!init_result) {
    // TODO(brettw) does the module get unloaded in this case?
    dispatcher_.reset();
    return false;
  }
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
  if (dispatcher_.get()) {
    pp::proxy::HostDispatcher::SetForInstance(instance->pp_instance(),
                                              dispatcher_.get());
  }
  return instance;
}

PluginInstance* PluginModule::GetSomeInstance() const {
  // This will generally crash later if there is not actually any instance to
  // return, so we force a crash now to make bugs easier to track down.
  CHECK(!instances_.empty());
  return *instances_.begin();
}

const void* PluginModule::GetPluginInterface(const char* name) const {
  if (dispatcher_.get())
    return dispatcher_->GetProxiedInterface(name);

  // In-process plugins.
  if (!entry_points_.get_interface)
    return NULL;
  return entry_points_.get_interface(name);
}

void PluginModule::InstanceCreated(PluginInstance* instance) {
  instances_.insert(instance);
}

void PluginModule::InstanceDeleted(PluginInstance* instance) {
  pp::proxy::HostDispatcher::RemoveForInstance(instance->pp_instance());
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

}  // namespace pepper
