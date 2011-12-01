// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/plugins/ppapi/plugin_module.h"

#include <set>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/message_loop_proxy.h"
#include "base/time.h"
#include "ppapi/c/dev/ppb_audio_input_dev.h"
#include "ppapi/c/dev/ppb_buffer_dev.h"
#include "ppapi/c/dev/ppb_char_set_dev.h"
#include "ppapi/c/dev/ppb_console_dev.h"
#include "ppapi/c/dev/ppb_crypto_dev.h"
#include "ppapi/c/dev/ppb_cursor_control_dev.h"
#include "ppapi/c/dev/ppb_directory_reader_dev.h"
#include "ppapi/c/dev/ppb_file_chooser_dev.h"
#include "ppapi/c/dev/ppb_find_dev.h"
#include "ppapi/c/dev/ppb_font_dev.h"
#include "ppapi/c/dev/ppb_fullscreen_dev.h"
#include "ppapi/c/dev/ppb_gles_chromium_texture_mapping_dev.h"
#include "ppapi/c/dev/ppb_layer_compositor_dev.h"
#include "ppapi/c/dev/ppb_memory_dev.h"
#include "ppapi/c/dev/ppb_scrollbar_dev.h"
#include "ppapi/c/dev/ppb_testing_dev.h"
#include "ppapi/c/dev/ppb_text_input_dev.h"
#include "ppapi/c/dev/ppb_transport_dev.h"
#include "ppapi/c/dev/ppb_url_util_dev.h"
#include "ppapi/c/dev/ppb_var_deprecated.h"
#include "ppapi/c/dev/ppb_video_capture_dev.h"
#include "ppapi/c/dev/ppb_video_decoder_dev.h"
#include "ppapi/c/dev/ppb_video_layer_dev.h"
#include "ppapi/c/dev/ppb_widget_dev.h"
#include "ppapi/c/dev/ppb_zoom_dev.h"
#include "ppapi/c/pp_module.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/pp_var.h"
#include "ppapi/c/ppb_audio.h"
#include "ppapi/c/ppb_audio_config.h"
#include "ppapi/c/ppb_core.h"
#include "ppapi/c/ppb_file_io.h"
#include "ppapi/c/ppb_file_ref.h"
#include "ppapi/c/ppb_file_system.h"
#include "ppapi/c/ppb_fullscreen.h"
#include "ppapi/c/ppb_graphics_2d.h"
#include "ppapi/c/ppb_graphics_3d.h"
#include "ppapi/c/ppb_image_data.h"
#include "ppapi/c/ppb_instance.h"
#include "ppapi/c/ppb_messaging.h"
#include "ppapi/c/ppb_mouse_lock.h"
#include "ppapi/c/ppb_opengles2.h"
#include "ppapi/c/ppb_url_loader.h"
#include "ppapi/c/ppb_url_request_info.h"
#include "ppapi/c/ppb_url_response_info.h"
#include "ppapi/c/ppb_var.h"
#include "ppapi/c/ppp.h"
#include "ppapi/c/ppp_instance.h"
#include "ppapi/c/private/ppb_flash.h"
#include "ppapi/c/private/ppb_flash_clipboard.h"
#include "ppapi/c/private/ppb_flash_file.h"
#include "ppapi/c/private/ppb_flash_fullscreen.h"
#include "ppapi/c/private/ppb_flash_tcp_socket.h"
#include "ppapi/c/private/ppb_gpu_blacklist_private.h"
#include "ppapi/c/private/ppb_instance_private.h"
#include "ppapi/c/private/ppb_pdf.h"
#include "ppapi/c/private/ppb_proxy_private.h"
#include "ppapi/c/private/ppb_tcp_socket_private.h"
#include "ppapi/c/private/ppb_udp_socket_private.h"
#include "ppapi/c/private/ppb_uma_private.h"
#include "ppapi/c/trusted/ppb_audio_input_trusted_dev.h"
#include "ppapi/c/trusted/ppb_audio_trusted.h"
#include "ppapi/c/trusted/ppb_broker_trusted.h"
#include "ppapi/c/trusted/ppb_buffer_trusted.h"
#include "ppapi/c/trusted/ppb_file_chooser_trusted.h"
#include "ppapi/c/trusted/ppb_file_io_trusted.h"
#include "ppapi/c/trusted/ppb_graphics_3d_trusted.h"
#include "ppapi/c/trusted/ppb_image_data_trusted.h"
#include "ppapi/c/trusted/ppb_url_loader_trusted.h"
#include "ppapi/shared_impl/input_event_impl.h"
#include "ppapi/shared_impl/time_conversion.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/thunk.h"
#include "webkit/plugins/plugin_switches.h"
#include "webkit/plugins/ppapi/callbacks.h"
#include "webkit/plugins/ppapi/common.h"
#include "webkit/plugins/ppapi/host_globals.h"
#include "webkit/plugins/ppapi/host_resource_tracker.h"
#include "webkit/plugins/ppapi/ppapi_interface_factory.h"
#include "webkit/plugins/ppapi/ppapi_plugin_instance.h"
#include "webkit/plugins/ppapi/ppb_directory_reader_impl.h"
#include "webkit/plugins/ppapi/ppb_flash_clipboard_impl.h"
#include "webkit/plugins/ppapi/ppb_flash_file_impl.h"
#include "webkit/plugins/ppapi/ppb_flash_impl.h"
#include "webkit/plugins/ppapi/ppb_flash_menu_impl.h"
#include "webkit/plugins/ppapi/ppb_flash_net_connector_impl.h"
#include "webkit/plugins/ppapi/ppb_font_impl.h"
#include "webkit/plugins/ppapi/ppb_gpu_blacklist_private_impl.h"
#include "webkit/plugins/ppapi/ppb_graphics_2d_impl.h"
#include "webkit/plugins/ppapi/ppb_image_data_impl.h"
#include "webkit/plugins/ppapi/ppb_layer_compositor_impl.h"
#include "webkit/plugins/ppapi/ppb_opengles_impl.h"
#include "webkit/plugins/ppapi/ppb_proxy_impl.h"
#include "webkit/plugins/ppapi/ppb_scrollbar_impl.h"
#include "webkit/plugins/ppapi/ppb_uma_private_impl.h"
#include "webkit/plugins/ppapi/ppb_var_impl.h"
#include "webkit/plugins/ppapi/ppb_video_capture_impl.h"
#include "webkit/plugins/ppapi/ppb_video_decoder_impl.h"
#include "webkit/plugins/ppapi/ppb_video_layer_impl.h"
#include "webkit/plugins/ppapi/webkit_forwarding_impl.h"

using ppapi::InputEventData;
using ppapi::PpapiGlobals;
using ppapi::TimeTicksToPPTimeTicks;
using ppapi::TimeToPPTime;
using ppapi::thunk::EnterResource;
using ppapi::thunk::PPB_Graphics2D_API;
using ppapi::thunk::PPB_InputEvent_API;

namespace webkit {
namespace ppapi {

namespace {

// Global tracking info for PPAPI plugins. This is lazily created before the
// first plugin is allocated, and leaked on shutdown.
//
// Note that we don't want a Singleton here since destroying this object will
// try to free some stuff that requires WebKit, and Singletons are destroyed
// after WebKit.
webkit::ppapi::HostGlobals* host_globals = NULL;

// Maintains all currently loaded plugin libs for validating PP_Module
// identifiers.
typedef std::set<PluginModule*> PluginModuleSet;

PluginModuleSet* GetLivePluginSet() {
  CR_DEFINE_STATIC_LOCAL(PluginModuleSet, live_plugin_libs, ());
  return &live_plugin_libs;
}

base::MessageLoopProxy* GetMainThreadMessageLoop() {
  CR_DEFINE_STATIC_LOCAL(scoped_refptr<base::MessageLoopProxy>, proxy,
                         (base::MessageLoopProxy::current()));
  return proxy.get();
}

// PPB_Core --------------------------------------------------------------------

void AddRefResource(PP_Resource resource) {
  PpapiGlobals::Get()->GetResourceTracker()->AddRefResource(resource);
}

void ReleaseResource(PP_Resource resource) {
  PpapiGlobals::Get()->GetResourceTracker()->ReleaseResource(resource);
}

PP_Time GetTime() {
  return TimeToPPTime(base::Time::Now());
}

PP_TimeTicks GetTickTime() {
  return TimeTicksToPPTimeTicks(base::TimeTicks::Now());
}

void CallOnMainThread(int delay_in_msec,
                      PP_CompletionCallback callback,
                      int32_t result) {
  if (callback.func) {
    GetMainThreadMessageLoop()->PostDelayedTask(
        FROM_HERE,
        base::Bind(callback.func, callback.user_data, result),
        delay_in_msec);
  }
}

PP_Bool IsMainThread() {
  return BoolToPPBool(GetMainThreadMessageLoop()->BelongsToCurrentThread());
}

const PPB_Core core_interface = {
  &AddRefResource,
  &ReleaseResource,
  &GetTime,
  &GetTickTime,
  &CallOnMainThread,
  &IsMainThread
};

// PPB_Testing -----------------------------------------------------------------

PP_Bool ReadImageData(PP_Resource device_context_2d,
                      PP_Resource image,
                      const PP_Point* top_left) {
  EnterResource<PPB_Graphics2D_API> enter(device_context_2d, true);
  if (enter.failed())
    return PP_FALSE;
  return BoolToPPBool(static_cast<PPB_Graphics2D_Impl*>(enter.object())->
      ReadImageData(image, top_left));
}

void RunMessageLoop(PP_Instance instance) {
  bool old_state = MessageLoop::current()->NestableTasksAllowed();
  MessageLoop::current()->SetNestableTasksAllowed(true);
  MessageLoop::current()->Run();
  MessageLoop::current()->SetNestableTasksAllowed(old_state);
}

void QuitMessageLoop(PP_Instance instance) {
  MessageLoop::current()->QuitNow();
}

uint32_t GetLiveObjectsForInstance(PP_Instance instance_id) {
  return HostGlobals::Get()->host_resource_tracker()->GetLiveObjectsForInstance(
      instance_id);
}

PP_Bool IsOutOfProcess() {
  return PP_FALSE;
}

void SimulateInputEvent(PP_Instance instance, PP_Resource input_event) {
  PluginInstance* plugin_instance = host_globals->GetInstance(instance);
  if (!plugin_instance)
    return;

  EnterResource<PPB_InputEvent_API> enter(input_event, false);
  if (enter.failed())
    return;

  const InputEventData& input_event_data = enter.object()->GetInputEventData();
  plugin_instance->SimulateInputEvent(input_event_data);
}

const PPB_Testing_Dev testing_interface = {
  &ReadImageData,
  &RunMessageLoop,
  &QuitMessageLoop,
  &GetLiveObjectsForInstance,
  &IsOutOfProcess,
  &SimulateInputEvent
};

// GetInterface ----------------------------------------------------------------

const void* GetInterface(const char* name) {
  // All interfaces should be used on the main thread.
  CHECK(IsMainThread());

  // Allow custom interface factories first stab at the GetInterface call.
  const void* custom_interface =
      PpapiInterfaceFactoryManager::GetInstance()->GetInterface(name);
  if (custom_interface)
    return custom_interface;

  // TODO(brettw) put these in a hash map for better performance.
  #define UNPROXIED_IFACE(api_name, iface_str, iface_struct) \
      if (strcmp(name, iface_str) == 0) \
        return ::ppapi::thunk::Get##iface_struct##_Thunk();
  #define PROXIED_IFACE(api_name, iface_str, iface_struct) \
      UNPROXIED_IFACE(api_name, iface_str, iface_struct)

  #include "ppapi/thunk/interfaces_ppb_public_stable.h"
  #include "ppapi/thunk/interfaces_ppb_public_dev.h"
  #include "ppapi/thunk/interfaces_ppb_private.h"

  #undef UNPROXIED_API
  #undef PROXIED_IFACE

  // Please keep alphabetized by interface macro name with "special" stuff at
  // the bottom.
  if (strcmp(name, PPB_AUDIO_INPUT_TRUSTED_DEV_INTERFACE) == 0)
    return ::ppapi::thunk::GetPPB_AudioInputTrusted_Thunk();
  if (strcmp(name, PPB_AUDIO_TRUSTED_INTERFACE) == 0)
    return ::ppapi::thunk::GetPPB_AudioTrusted_Thunk();
  if (strcmp(name, PPB_BUFFER_TRUSTED_INTERFACE) == 0)
    return ::ppapi::thunk::GetPPB_BufferTrusted_Thunk();
  if (strcmp(name, PPB_CORE_INTERFACE) == 0)
    return &core_interface;
  if (strcmp(name, PPB_FILEIOTRUSTED_INTERFACE) == 0)
    return ::ppapi::thunk::GetPPB_FileIOTrusted_Thunk();
  if (strcmp(name, PPB_FILECHOOSER_TRUSTED_INTERFACE) == 0)
    return ::ppapi::thunk::GetPPB_FileChooser_Trusted_Thunk();
  if (strcmp(name, PPB_FLASH_INTERFACE) == 0)
    return PPB_Flash_Impl::GetInterface();
  if (strcmp(name, PPB_FLASH_CLIPBOARD_INTERFACE) == 0)
    return ::ppapi::thunk::GetPPB_Flash_Clipboard_Thunk();
  if (strcmp(name, PPB_FLASH_CLIPBOARD_INTERFACE_3_LEGACY) == 0)
    return ::ppapi::thunk::GetPPB_Flash_Clipboard_Thunk();
  if (strcmp(name, PPB_FLASH_FILE_FILEREF_INTERFACE) == 0)
    return PPB_Flash_File_FileRef_Impl::GetInterface();
  if (strcmp(name, PPB_FLASH_FILE_MODULELOCAL_INTERFACE) == 0)
    return PPB_Flash_File_ModuleLocal_Impl::GetInterface();
  if (strcmp(name, PPB_FLASH_MENU_INTERFACE) == 0)
    return ::ppapi::thunk::GetPPB_Flash_Menu_Thunk();
  if (strcmp(name, PPB_FLASH_TCPSOCKET_INTERFACE) == 0)
    return ::ppapi::thunk::GetPPB_TCPSocket_Private_Thunk();
  if (strcmp(name, PPB_FULLSCREEN_DEV_INTERFACE) == 0)
    return ::ppapi::thunk::GetPPB_Fullscreen_Thunk();
  if (strcmp(name, PPB_GPU_BLACKLIST_INTERFACE) == 0)
    return PPB_GpuBlacklist_Private_Impl::GetInterface();
  if (strcmp(name, PPB_GRAPHICS_3D_TRUSTED_INTERFACE) == 0)
    return ::ppapi::thunk::GetPPB_Graphics3DTrusted_Thunk();
  if (strcmp(name, PPB_IMAGEDATA_TRUSTED_INTERFACE) == 0)
    return ::ppapi::thunk::GetPPB_ImageDataTrusted_Thunk();
  if (strcmp(name, PPB_INPUT_EVENT_INTERFACE_1_0) == 0)
    return ::ppapi::thunk::GetPPB_InputEvent_Thunk();
  if (strcmp(name, PPB_INSTANCE_PRIVATE_INTERFACE) == 0)
    return ::ppapi::thunk::GetPPB_Instance_Private_Thunk();
  if (strcmp(name, PPB_OPENGLES2_INTERFACE) == 0)
    return PPB_OpenGLES_Impl::GetInterface();
  if (strcmp(name, PPB_PROXY_PRIVATE_INTERFACE) == 0)
    return PPB_Proxy_Impl::GetInterface();
  if (strcmp(name, PPB_UMA_PRIVATE_INTERFACE) == 0)
    return PPB_UMA_Private_Impl::GetInterface();
  if (strcmp(name, PPB_URLLOADERTRUSTED_INTERFACE) == 0)
    return ::ppapi::thunk::GetPPB_URLLoaderTrusted_Thunk();
  if (strcmp(name, PPB_VAR_DEPRECATED_INTERFACE) == 0)
    return PPB_Var_Impl::GetVarDeprecatedInterface();
  if (strcmp(name, PPB_VAR_INTERFACE_1_0) == 0)
    return PPB_Var_Impl::GetVarInterface();

#ifdef ENABLE_FLAPPER_HACKS
  if (strcmp(name, PPB_FLASH_NETCONNECTOR_INTERFACE) == 0)
    return ::ppapi::thunk::GetPPB_Flash_NetConnector_Thunk();
#endif  // ENABLE_FLAPPER_HACKS

  // Only support the testing interface when the command line switch is
  // specified. This allows us to prevent people from (ab)using this interface
  // in production code.
  if (CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnablePepperTesting)) {
    if (strcmp(name, PPB_TESTING_DEV_INTERFACE) == 0 ||
        strcmp(name, PPB_TESTING_DEV_INTERFACE_0_7) == 0) {
      return &testing_interface;
    }
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

PluginModule::PluginModule(const std::string& name,
                           const FilePath& path,
                           PluginDelegate::ModuleLifetime* lifetime_delegate)
    : lifetime_delegate_(lifetime_delegate),
      callback_tracker_(new CallbackTracker),
      is_in_destructor_(false),
      is_crashed_(false),
      broker_(NULL),
      library_(NULL),
      name_(name),
      path_(path),
      reserve_instance_id_(NULL) {
  // Ensure the globals object is created.
  if (!host_globals)
    host_globals = new HostGlobals;

  memset(&entry_points_, 0, sizeof(entry_points_));
  pp_module_ = HostGlobals::Get()->AddModule(this);
  GetMainThreadMessageLoop();  // Initialize the main thread message loop.
  GetLivePluginSet()->insert(this);
}

PluginModule::~PluginModule() {
  // In the past there have been crashes reentering the plugin module
  // destructor. Catch if that happens again earlier.
  CHECK(!is_in_destructor_);
  is_in_destructor_ = true;

  // When the module is being deleted, there should be no more instances still
  // holding a reference to us.
  DCHECK(instances_.empty());

  GetLivePluginSet()->erase(this);

  callback_tracker_->AbortAll();

  if (entry_points_.shutdown_module)
    entry_points_.shutdown_module();

  if (library_)
    base::UnloadNativeLibrary(library_);

  // Notifications that we've been deleted should be last.
  HostGlobals::Get()->ModuleDeleted(pp_module_);
  if (!is_crashed_) {
    // When the plugin crashes, we immediately tell the lifetime delegate that
    // we're gone, so we don't want to tell it again.
    lifetime_delegate_->PluginModuleDead(this);
  }

  // Don't add stuff here, the two notifications that the module object has
  // been deleted should be last. This allows, for example,
  // PPB_Proxy.IsInModuleDestructor to map PP_Module to this class during the
  // previous parts of the destructor.
}

bool PluginModule::InitAsInternalPlugin(const EntryPoints& entry_points) {
  if (InitializeModule(entry_points)) {
    entry_points_ = entry_points;
    return true;
  }
  return false;
}

bool PluginModule::InitAsLibrary(const FilePath& path) {
  base::NativeLibrary library = base::LoadNativeLibrary(path, NULL);
  if (!library)
    return false;

  EntryPoints entry_points;

  if (!LoadEntryPointsFromLibrary(library, &entry_points) ||
      !InitializeModule(entry_points)) {
    base::UnloadNativeLibrary(library);
    return false;
  }
  entry_points_ = entry_points;
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
  PluginInstance* instance(NULL);
  const void* ppp_instance = GetPluginInterface(PPP_INSTANCE_INTERFACE_1_0);
  if (ppp_instance) {
    instance = PluginInstance::Create1_0(delegate, this, ppp_instance);
  } if (!instance) {
    LOG(WARNING) << "Plugin doesn't support instance interface, failing.";
    return NULL;
  }
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

scoped_refptr<CallbackTracker> PluginModule::GetCallbackTracker() {
  return callback_tracker_;
}

void PluginModule::PluginCrashed() {
  DCHECK(!is_crashed_);  // Should only get one notification.
  is_crashed_ = true;

  // Notify all instances that they crashed.
  for (PluginInstanceSet::iterator i = instances_.begin();
       i != instances_.end(); ++i)
    (*i)->InstanceCrashed();

  lifetime_delegate_->PluginModuleDead(this);
}

void PluginModule::SetReserveInstanceIDCallback(
    PP_Bool (*reserve)(PP_Module, PP_Instance)) {
  DCHECK(!reserve_instance_id_) << "Only expect one set.";
  reserve_instance_id_ = reserve;
}

bool PluginModule::ReserveInstanceID(PP_Instance instance) {
  if (reserve_instance_id_)
    return PPBoolToBool(reserve_instance_id_(pp_module_, instance));
  return true;  // Instance ID is usable.
}

void PluginModule::SetBroker(PluginDelegate::PpapiBroker* broker) {
  DCHECK(!broker_ || !broker);
  broker_ = broker;
}

PluginDelegate::PpapiBroker* PluginModule::GetBroker() {
  return broker_;
}

::ppapi::WebKitForwarding* PluginModule::GetWebKitForwarding() {
  if (!webkit_forwarding_.get())
    webkit_forwarding_.reset(new WebKitForwardingImpl);
  return webkit_forwarding_.get();
}

bool PluginModule::InitializeModule(const EntryPoints& entry_points) {
  DCHECK(!out_of_process_proxy_.get()) << "Don't call for proxied modules.";
  DCHECK(entry_points.initialize_module != NULL);
  int retval = entry_points.initialize_module(pp_module(), &GetInterface);
  if (retval != 0) {
    LOG(WARNING) << "PPP_InitializeModule returned failure " << retval;
    return false;
  }
  return true;
}

}  // namespace ppapi
}  // namespace webkit
