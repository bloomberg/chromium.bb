// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/plugins/ppapi/plugin_module.h"

#include <set>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/message_loop_proxy.h"
#include "base/time.h"
#include "ppapi/c/dev/ppb_buffer_dev.h"
#include "ppapi/c/dev/ppb_char_set_dev.h"
#include "ppapi/c/dev/ppb_console_dev.h"
#include "ppapi/c/dev/ppb_context_3d_dev.h"
#include "ppapi/c/dev/ppb_context_3d_trusted_dev.h"
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
#include "ppapi/c/dev/ppb_query_policy_dev.h"
#include "ppapi/c/dev/ppb_scrollbar_dev.h"
#include "ppapi/c/dev/ppb_surface_3d_dev.h"
#include "ppapi/c/dev/ppb_testing_dev.h"
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
#include "ppapi/c/ppb_graphics_2d.h"
#include "ppapi/c/ppb_graphics_3d.h"
#include "ppapi/c/ppb_image_data.h"
#include "ppapi/c/ppb_instance.h"
#include "ppapi/c/ppb_messaging.h"
#include "ppapi/c/ppb_opengles.h"
#include "ppapi/c/ppb_url_loader.h"
#include "ppapi/c/ppb_url_request_info.h"
#include "ppapi/c/ppb_url_response_info.h"
#include "ppapi/c/ppb_var.h"
#include "ppapi/c/ppp.h"
#include "ppapi/c/ppp_instance.h"
#include "ppapi/c/private/ppb_flash.h"
#include "ppapi/c/private/ppb_flash_clipboard.h"
#include "ppapi/c/private/ppb_flash_file.h"
#include "ppapi/c/private/ppb_flash_tcp_socket.h"
#include "ppapi/c/private/ppb_gpu_blacklist_private.h"
#include "ppapi/c/private/ppb_instance_private.h"
#include "ppapi/c/private/ppb_pdf.h"
#include "ppapi/c/private/ppb_proxy_private.h"
#include "ppapi/c/private/ppb_uma_private.h"
#include "ppapi/c/trusted/ppb_audio_trusted.h"
#include "ppapi/c/trusted/ppb_broker_trusted.h"
#include "ppapi/c/trusted/ppb_buffer_trusted.h"
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
#include "webkit/plugins/ppapi/ppapi_interface_factory.h"
#include "webkit/plugins/ppapi/ppapi_plugin_instance.h"
#include "webkit/plugins/ppapi/ppb_console_impl.h"
#include "webkit/plugins/ppapi/ppb_crypto_impl.h"
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
#include "webkit/plugins/ppapi/ppb_memory_impl.h"
#include "webkit/plugins/ppapi/ppb_opengles_impl.h"
#include "webkit/plugins/ppapi/ppb_proxy_impl.h"
#include "webkit/plugins/ppapi/ppb_scrollbar_impl.h"
#include "webkit/plugins/ppapi/ppb_uma_private_impl.h"
#include "webkit/plugins/ppapi/ppb_url_util_impl.h"
#include "webkit/plugins/ppapi/ppb_var_impl.h"
#include "webkit/plugins/ppapi/ppb_video_capture_impl.h"
#include "webkit/plugins/ppapi/ppb_video_decoder_impl.h"
#include "webkit/plugins/ppapi/ppb_video_layer_impl.h"
#include "webkit/plugins/ppapi/resource_tracker.h"
#include "webkit/plugins/ppapi/webkit_forwarding_impl.h"

using ppapi::TimeTicksToPPTimeTicks;
using ppapi::TimeToPPTime;
using ppapi::thunk::EnterResource;
using ppapi::thunk::PPB_Graphics2D_API;

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
      base::MessageLoopProxy::current());
  return proxy.get();
}

// PPB_Core --------------------------------------------------------------------

void AddRefResource(PP_Resource resource) {
  ResourceTracker::Get()->AddRefResource(resource);
}

void ReleaseResource(PP_Resource resource) {
  ResourceTracker::Get()->ReleaseResource(resource);
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
  return ResourceTracker::Get()->GetLiveObjectsForInstance(instance_id);
}

PP_Bool IsOutOfProcess() {
  return PP_FALSE;
}

const PPB_Testing_Dev testing_interface = {
  &ReadImageData,
  &RunMessageLoop,
  &QuitMessageLoop,
  &GetLiveObjectsForInstance,
  &IsOutOfProcess
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

  // Please keep alphabetized by interface macro name with "special" stuff at
  // the bottom.
  if (strcmp(name, PPB_AUDIO_CONFIG_INTERFACE_1_0) == 0)
    return ::ppapi::thunk::GetPPB_AudioConfig_Thunk();
  if (strcmp(name, PPB_AUDIO_INTERFACE_1_0) == 0)
    return ::ppapi::thunk::GetPPB_Audio_Thunk();
  if (strcmp(name, PPB_AUDIO_TRUSTED_INTERFACE) == 0)
    return ::ppapi::thunk::GetPPB_AudioTrusted_Thunk();
  if (strcmp(name, PPB_BROKER_TRUSTED_INTERFACE) == 0)
    return ::ppapi::thunk::GetPPB_Broker_Thunk();
  if (strcmp(name, PPB_BUFFER_DEV_INTERFACE) == 0)
    return ::ppapi::thunk::GetPPB_Buffer_Thunk();
  if (strcmp(name, PPB_BUFFER_TRUSTED_INTERFACE) == 0)
    return ::ppapi::thunk::GetPPB_BufferTrusted_Thunk();
  if (strcmp(name, PPB_CHAR_SET_DEV_INTERFACE) == 0)
    return ::ppapi::thunk::GetPPB_CharSet_Thunk();
  if (strcmp(name, PPB_CONSOLE_DEV_INTERFACE) == 0)
    return PPB_Console_Impl::GetInterface();
  if (strcmp(name, PPB_CORE_INTERFACE) == 0)
    return &core_interface;
  if (strcmp(name, PPB_CRYPTO_DEV_INTERFACE) == 0)
    return PPB_Crypto_Impl::GetInterface();
  if (strcmp(name, PPB_CURSOR_CONTROL_DEV_INTERFACE) == 0)
    return ::ppapi::thunk::GetPPB_CursorControl_Thunk();
  if (strcmp(name, PPB_DIRECTORYREADER_DEV_INTERFACE) == 0)
    return ::ppapi::thunk::GetPPB_DirectoryReader_Thunk();
  if (strcmp(name, PPB_FILECHOOSER_DEV_INTERFACE) == 0)
    return ::ppapi::thunk::GetPPB_FileChooser_Thunk();
  if (strcmp(name, PPB_FILEIO_INTERFACE_1_0) == 0)
    return ::ppapi::thunk::GetPPB_FileIO_Thunk();
  if (strcmp(name, PPB_FILEIOTRUSTED_INTERFACE) == 0)
    return ::ppapi::thunk::GetPPB_FileIOTrusted_Thunk();
  if (strcmp(name, PPB_FILEREF_INTERFACE_1_0) == 0)
    return ::ppapi::thunk::GetPPB_FileRef_Thunk();
  if (strcmp(name, PPB_FILESYSTEM_INTERFACE_1_0) == 0)
    return ::ppapi::thunk::GetPPB_FileSystem_Thunk();
  if (strcmp(name, PPB_FIND_DEV_INTERFACE) == 0)
    return ::ppapi::thunk::GetPPB_Find_Thunk();
  if (strcmp(name, PPB_FLASH_INTERFACE) == 0)
    return PPB_Flash_Impl::GetInterface();
  if (strcmp(name, PPB_FLASH_CLIPBOARD_INTERFACE) == 0)
    return PPB_Flash_Clipboard_Impl::GetInterface();
  if (strcmp(name, PPB_FLASH_FILE_FILEREF_INTERFACE) == 0)
    return PPB_Flash_File_FileRef_Impl::GetInterface();
  if (strcmp(name, PPB_FLASH_FILE_MODULELOCAL_INTERFACE) == 0)
    return PPB_Flash_File_ModuleLocal_Impl::GetInterface();
  if (strcmp(name, PPB_FLASH_MENU_INTERFACE) == 0)
    return ::ppapi::thunk::GetPPB_Flash_Menu_Thunk();
  if (strcmp(name, PPB_FLASH_TCPSOCKET_INTERFACE) == 0)
    return ::ppapi::thunk::GetPPB_Flash_TCPSocket_Thunk();
  if (strcmp(name, PPB_FONT_DEV_INTERFACE) == 0)
    return ::ppapi::thunk::GetPPB_Font_Thunk();
  if (strcmp(name, PPB_FULLSCREEN_DEV_INTERFACE) == 0)
    return ::ppapi::thunk::GetPPB_Fullscreen_Thunk();
  if (strcmp(name, PPB_GPU_BLACKLIST_INTERFACE) == 0)
    return PPB_GpuBlacklist_Private_Impl::GetInterface();
  if (strcmp(name, PPB_GRAPHICS_2D_INTERFACE_1_0) == 0)
    return ::ppapi::thunk::GetPPB_Graphics2D_Thunk();
  if (strcmp(name, PPB_IMAGEDATA_INTERFACE_1_0) == 0)
    return ::ppapi::thunk::GetPPB_ImageData_Thunk();
  if (strcmp(name, PPB_IMAGEDATA_TRUSTED_INTERFACE) == 0)
    return ::ppapi::thunk::GetPPB_ImageDataTrusted_Thunk();
  if (strcmp(name, PPB_INPUT_EVENT_INTERFACE_1_0) == 0)
    return ::ppapi::thunk::GetPPB_InputEvent_Thunk();
  if (strcmp(name, PPB_INSTANCE_INTERFACE_1_0) == 0)
    return ::ppapi::thunk::GetPPB_Instance_1_0_Thunk();
  if (strcmp(name, PPB_INSTANCE_PRIVATE_INTERFACE) == 0)
    return ::ppapi::thunk::GetPPB_Instance_Private_Thunk();
  if (strcmp(name, PPB_KEYBOARD_INPUT_EVENT_INTERFACE_1_0) == 0)
    return ::ppapi::thunk::GetPPB_KeyboardInputEvent_Thunk();
  if (strcmp(name, PPB_MEMORY_DEV_INTERFACE) == 0)
    return PPB_Memory_Impl::GetInterface();
  if (strcmp(name, PPB_MESSAGING_INTERFACE_1_0) == 0)
    return ::ppapi::thunk::GetPPB_Messaging_Thunk();
  if (strcmp(name, PPB_MOUSE_INPUT_EVENT_INTERFACE_1_0) == 0)
    return ::ppapi::thunk::GetPPB_MouseInputEvent_1_0_Thunk();
  if (strcmp(name, PPB_MOUSE_INPUT_EVENT_INTERFACE_1_1) == 0)
    return ::ppapi::thunk::GetPPB_MouseInputEvent_1_1_Thunk();
  if (strcmp(name, PPB_PROXY_PRIVATE_INTERFACE) == 0)
    return PPB_Proxy_Impl::GetInterface();
  if (strcmp(name, PPB_QUERY_POLICY_DEV_INTERFACE_0_1) == 0)
    return ::ppapi::thunk::GetPPB_QueryPolicy_Thunk();
  if (strcmp(name, PPB_SCROLLBAR_DEV_INTERFACE_0_4) == 0)
    return PPB_Scrollbar_Impl::Get0_4Interface();
  if (strcmp(name, PPB_SCROLLBAR_DEV_INTERFACE_0_3) == 0)
    return PPB_Scrollbar_Impl::Get0_3Interface();
  if (strcmp(name, PPB_SCROLLBAR_DEV_INTERFACE_0_5) == 0)
    return ::ppapi::thunk::GetPPB_Scrollbar_Thunk();
  if (strcmp(name, PPB_UMA_PRIVATE_INTERFACE) == 0)
    return PPB_UMA_Private_Impl::GetInterface();
  if (strcmp(name, PPB_URLLOADER_INTERFACE_1_0) == 0)
    return ::ppapi::thunk::GetPPB_URLLoader_Thunk();
  if (strcmp(name, PPB_URLLOADERTRUSTED_INTERFACE) == 0)
    return ::ppapi::thunk::GetPPB_URLLoaderTrusted_Thunk();
  if (strcmp(name, PPB_URLREQUESTINFO_INTERFACE_1_0) == 0)
    return ::ppapi::thunk::GetPPB_URLRequestInfo_Thunk();
  if (strcmp(name, PPB_URLRESPONSEINFO_INTERFACE_1_0) == 0)
    return ::ppapi::thunk::GetPPB_URLResponseInfo_Thunk();
  if (strcmp(name, PPB_URLUTIL_DEV_INTERFACE) == 0)
    return PPB_URLUtil_Impl::GetInterface();
  if (strcmp(name, PPB_VAR_DEPRECATED_INTERFACE) == 0)
    return PPB_Var_Impl::GetVarDeprecatedInterface();
  if (strcmp(name, PPB_VAR_INTERFACE_1_0) == 0)
    return PPB_Var_Impl::GetVarInterface();
  if (strcmp(name, PPB_VIDEODECODER_DEV_INTERFACE) == 0)
    return ::ppapi::thunk::GetPPB_VideoDecoder_Thunk();
  if (strcmp(name, PPB_VIDEO_CAPTURE_DEV_INTERFACE) == 0)
    return ::ppapi::thunk::GetPPB_VideoCapture_Thunk();
  if (strcmp(name, PPB_VIDEOLAYER_DEV_INTERFACE) == 0)
    return ::ppapi::thunk::GetPPB_VideoLayer_Thunk();
  if (strcmp(name, PPB_WHEEL_INPUT_EVENT_INTERFACE_1_0) == 0)
    return ::ppapi::thunk::GetPPB_WheelInputEvent_Thunk();
  if (strcmp(name, PPB_WIDGET_DEV_INTERFACE) == 0)
    return ::ppapi::thunk::GetPPB_Widget_Thunk();
  if (strcmp(name, PPB_ZOOM_DEV_INTERFACE) == 0)
    return ::ppapi::thunk::GetPPB_Zoom_Thunk();

#ifdef ENABLE_GPU
  if (strcmp(name, PPB_GRAPHICS_3D_INTERFACE) == 0)
    return ::ppapi::thunk::GetPPB_Graphics3D_Thunk();
  if (strcmp(name, PPB_GRAPHICS_3D_TRUSTED_INTERFACE) == 0)
    return ::ppapi::thunk::GetPPB_Graphics3DTrusted_Thunk();
  if (strcmp(name, PPB_CONTEXT_3D_DEV_INTERFACE) == 0)
    return ::ppapi::thunk::GetPPB_Context3D_Thunk();
  if (strcmp(name, PPB_CONTEXT_3D_TRUSTED_DEV_INTERFACE) == 0)
    return ::ppapi::thunk::GetPPB_Context3DTrusted_Thunk();
  if (strcmp(name, PPB_GLES_CHROMIUM_TEXTURE_MAPPING_DEV_INTERFACE) == 0)
    return ::ppapi::thunk::GetPPB_GLESChromiumTextureMapping_Thunk();
  if (strcmp(name, PPB_OPENGLES2_INTERFACE) == 0)
    return PPB_OpenGLES_Impl::GetInterface();
  if (strcmp(name, PPB_SURFACE_3D_DEV_INTERFACE) == 0)
    return ::ppapi::thunk::GetPPB_Surface3D_Thunk();
  if (strcmp(name, PPB_LAYER_COMPOSITOR_DEV_INTERFACE) == 0)
    return ::ppapi::thunk::GetPPB_LayerCompositor_Thunk();
#endif  // ENABLE_GPU

#ifdef ENABLE_FLAPPER_HACKS
  if (strcmp(name, PPB_FLASH_NETCONNECTOR_INTERFACE) == 0)
    return ::ppapi::thunk::GetPPB_Flash_NetConnector_Thunk();
#endif  // ENABLE_FLAPPER_HACKS

#if defined(ENABLE_P2P_APIS)
  if (strcmp(name, PPB_TRANSPORT_DEV_INTERFACE) == 0)
    return ::ppapi::thunk::GetPPB_Transport_Thunk();
#endif

  // Only support the testing interface when the command line switch is
  // specified. This allows us to prevent people from (ab)using this interface
  // in production code.
  if (strcmp(name, PPB_TESTING_DEV_INTERFACE) == 0) {
    if (CommandLine::ForCurrentProcess()->HasSwitch(
            switches::kEnablePepperTesting))
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

PluginModule::PluginModule(const std::string& name,
                           const FilePath& path,
                           PluginDelegate::ModuleLifetime* lifetime_delegate)
    : lifetime_delegate_(lifetime_delegate),
      callback_tracker_(new CallbackTracker),
      is_crashed_(false),
      broker_(NULL),
      library_(NULL),
      name_(name),
      path_(path),
      reserve_instance_id_(NULL) {
  memset(&entry_points_, 0, sizeof(entry_points_));
  pp_module_ = ResourceTracker::Get()->AddModule(this);
  GetMainThreadMessageLoop();  // Initialize the main thread message loop.
  GetLivePluginSet()->insert(this);
}

PluginModule::~PluginModule() {
  // When the module is being deleted, there should be no more instances still
  // holding a reference to us.
  DCHECK(instances_.empty());

  GetLivePluginSet()->erase(this);

  callback_tracker_->AbortAll();

  if (entry_points_.shutdown_module)
    entry_points_.shutdown_module();

  if (library_)
    base::UnloadNativeLibrary(library_);

  ResourceTracker::Get()->ModuleDeleted(pp_module_);

  // When the plugin crashes, we immediately tell the lifetime delegate that
  // we're gone, so we don't want to tell it again.
  if (!is_crashed_)
    lifetime_delegate_->PluginModuleDead(this);
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
const PPB_Memory_Dev* PluginModule::GetMemoryDev() {
  return static_cast<const PPB_Memory_Dev*>(PPB_Memory_Impl::GetInterface());
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
