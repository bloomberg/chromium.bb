// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Multiply-included message header, no traditional include guard.
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/files/file_path.h"
#include "base/process.h"
#include "base/shared_memory.h"
#include "base/string16.h"
#include "base/sync_socket.h"
#include "gpu/command_buffer/common/command_buffer.h"
#include "gpu/ipc/gpu_command_buffer_traits.h"
#include "ipc/ipc_channel_handle.h"
#include "ipc/ipc_message_macros.h"
#include "ipc/ipc_message_utils.h"
#include "ipc/ipc_platform_file.h"
#include "ppapi/c/dev/pp_video_capture_dev.h"
#include "ppapi/c/dev/pp_video_dev.h"
#include "ppapi/c/dev/ppb_directory_reader_dev.h"
#include "ppapi/c/dev/ppb_text_input_dev.h"
#include "ppapi/c/dev/ppb_url_util_dev.h"
#include "ppapi/c/dev/ppp_printing_dev.h"
#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_file_info.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_module.h"
#include "ppapi/c/pp_point.h"
#include "ppapi/c/pp_rect.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/pp_size.h"
#include "ppapi/c/pp_time.h"
#include "ppapi/c/ppb_audio_config.h"
#include "ppapi/c/private/pp_content_decryptor.h"
#include "ppapi/c/private/pp_private_font_charset.h"
#include "ppapi/c/private/ppb_flash.h"
#include "ppapi/c/private/ppb_host_resolver_private.h"
#include "ppapi/c/private/ppb_net_address_private.h"
#include "ppapi/c/private/ppb_tcp_socket_private.h"
#include "ppapi/c/private/ppb_udp_socket_private.h"
#include "ppapi/c/private/ppp_flash_browser_operations.h"
#include "ppapi/proxy/host_resolver_private_resource.h"
#include "ppapi/proxy/ppapi_param_traits.h"
#include "ppapi/proxy/ppapi_proxy_export.h"
#include "ppapi/proxy/resource_message_params.h"
#include "ppapi/proxy/serialized_flash_menu.h"
#include "ppapi/proxy/serialized_handle.h"
#include "ppapi/proxy/serialized_structs.h"
#include "ppapi/proxy/serialized_var.h"
#include "ppapi/shared_impl/dir_contents.h"
#include "ppapi/shared_impl/file_path.h"
#include "ppapi/shared_impl/ppapi_preferences.h"
#include "ppapi/shared_impl/ppb_device_ref_shared.h"
#include "ppapi/shared_impl/ppb_input_event_shared.h"
#include "ppapi/shared_impl/ppb_network_list_private_shared.h"
#include "ppapi/shared_impl/ppb_view_shared.h"
#include "ppapi/shared_impl/ppp_flash_browser_operations_shared.h"
#include "ppapi/shared_impl/private/ppb_x509_certificate_private_shared.h"
#include "ppapi/shared_impl/url_request_info_data.h"
#include "ppapi/shared_impl/url_response_info_data.h"

#undef IPC_MESSAGE_EXPORT
#define IPC_MESSAGE_EXPORT PPAPI_PROXY_EXPORT

#define IPC_MESSAGE_START PpapiMsgStart

IPC_ENUM_TRAITS(PP_AudioSampleRate)
IPC_ENUM_TRAITS(PP_DeviceType_Dev)
IPC_ENUM_TRAITS(PP_DecryptorStreamType)
IPC_ENUM_TRAITS(PP_FileType)
IPC_ENUM_TRAITS(PP_Flash_BrowserOperations_Permission)
IPC_ENUM_TRAITS(PP_Flash_BrowserOperations_SettingType)
IPC_ENUM_TRAITS(PP_FlashSetting)
IPC_ENUM_TRAITS(PP_InputEvent_MouseButton)
IPC_ENUM_TRAITS(PP_InputEvent_Type)
IPC_ENUM_TRAITS(PP_NetAddressFamily_Private)
IPC_ENUM_TRAITS(PP_NetworkListState_Private)
IPC_ENUM_TRAITS(PP_NetworkListType_Private)
IPC_ENUM_TRAITS(PP_PrintOrientation_Dev)
IPC_ENUM_TRAITS(PP_PrintOutputFormat_Dev)
IPC_ENUM_TRAITS(PP_PrintScalingOption_Dev)
IPC_ENUM_TRAITS(PP_PrivateFontCharset)
IPC_ENUM_TRAITS(PP_TextInput_Type)
IPC_ENUM_TRAITS(PP_VideoDecodeError_Dev)
IPC_ENUM_TRAITS(PP_VideoDecoder_Profile)

IPC_STRUCT_TRAITS_BEGIN(PP_Point)
  IPC_STRUCT_TRAITS_MEMBER(x)
  IPC_STRUCT_TRAITS_MEMBER(y)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(PP_FloatPoint)
  IPC_STRUCT_TRAITS_MEMBER(x)
  IPC_STRUCT_TRAITS_MEMBER(y)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(PP_Size)
  IPC_STRUCT_TRAITS_MEMBER(height)
  IPC_STRUCT_TRAITS_MEMBER(width)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(PP_Rect)
  IPC_STRUCT_TRAITS_MEMBER(point)
  IPC_STRUCT_TRAITS_MEMBER(size)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(PP_PictureBuffer_Dev)
  IPC_STRUCT_TRAITS_MEMBER(id)
  IPC_STRUCT_TRAITS_MEMBER(size)
  IPC_STRUCT_TRAITS_MEMBER(texture_id)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(PP_Picture_Dev)
  IPC_STRUCT_TRAITS_MEMBER(picture_buffer_id)
  IPC_STRUCT_TRAITS_MEMBER(bitstream_buffer_id)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(PP_PrintPageNumberRange_Dev)
  IPC_STRUCT_TRAITS_MEMBER(first_page_number)
  IPC_STRUCT_TRAITS_MEMBER(last_page_number)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(PP_VideoCaptureDeviceInfo_Dev)
  IPC_STRUCT_TRAITS_MEMBER(width)
  IPC_STRUCT_TRAITS_MEMBER(height)
  IPC_STRUCT_TRAITS_MEMBER(frames_per_second)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(PP_HostResolver_Private_Hint)
  IPC_STRUCT_TRAITS_MEMBER(family)
  IPC_STRUCT_TRAITS_MEMBER(flags)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(PP_PrintSettings_Dev)
  IPC_STRUCT_TRAITS_MEMBER(printable_area)
  IPC_STRUCT_TRAITS_MEMBER(content_area)
  IPC_STRUCT_TRAITS_MEMBER(paper_size)
  IPC_STRUCT_TRAITS_MEMBER(dpi)
  IPC_STRUCT_TRAITS_MEMBER(orientation)
  IPC_STRUCT_TRAITS_MEMBER(print_scaling_option)
  IPC_STRUCT_TRAITS_MEMBER(grayscale)
  IPC_STRUCT_TRAITS_MEMBER(format)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(PP_URLComponent_Dev)
  IPC_STRUCT_TRAITS_MEMBER(begin)
  IPC_STRUCT_TRAITS_MEMBER(len)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(PP_URLComponents_Dev)
  IPC_STRUCT_TRAITS_MEMBER(scheme)
  IPC_STRUCT_TRAITS_MEMBER(username)
  IPC_STRUCT_TRAITS_MEMBER(password)
  IPC_STRUCT_TRAITS_MEMBER(host)
  IPC_STRUCT_TRAITS_MEMBER(port)
  IPC_STRUCT_TRAITS_MEMBER(path)
  IPC_STRUCT_TRAITS_MEMBER(query)
  IPC_STRUCT_TRAITS_MEMBER(ref)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(ppapi::DeviceRefData)
  IPC_STRUCT_TRAITS_MEMBER(type)
  IPC_STRUCT_TRAITS_MEMBER(name)
  IPC_STRUCT_TRAITS_MEMBER(id)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(ppapi::DirEntry)
  IPC_STRUCT_TRAITS_MEMBER(name)
  IPC_STRUCT_TRAITS_MEMBER(is_dir)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(ppapi::FlashSiteSetting)
  IPC_STRUCT_TRAITS_MEMBER(site)
  IPC_STRUCT_TRAITS_MEMBER(permission)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(ppapi::ViewData)
  IPC_STRUCT_TRAITS_MEMBER(rect)
  IPC_STRUCT_TRAITS_MEMBER(is_fullscreen)
  IPC_STRUCT_TRAITS_MEMBER(is_page_visible)
  IPC_STRUCT_TRAITS_MEMBER(clip_rect)
  IPC_STRUCT_TRAITS_MEMBER(device_scale)
  IPC_STRUCT_TRAITS_MEMBER(css_scale)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(PP_TouchPoint)
  IPC_STRUCT_TRAITS_MEMBER(id)
  IPC_STRUCT_TRAITS_MEMBER(position)
  IPC_STRUCT_TRAITS_MEMBER(radius)
  IPC_STRUCT_TRAITS_MEMBER(rotation_angle)
  IPC_STRUCT_TRAITS_MEMBER(pressure)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(ppapi::Preferences)
  IPC_STRUCT_TRAITS_MEMBER(standard_font_family_map)
  IPC_STRUCT_TRAITS_MEMBER(fixed_font_family_map)
  IPC_STRUCT_TRAITS_MEMBER(serif_font_family_map)
  IPC_STRUCT_TRAITS_MEMBER(sans_serif_font_family_map)
  IPC_STRUCT_TRAITS_MEMBER(default_font_size)
  IPC_STRUCT_TRAITS_MEMBER(default_fixed_font_size)
  IPC_STRUCT_TRAITS_MEMBER(number_of_cpu_cores)
  IPC_STRUCT_TRAITS_MEMBER(is_3d_supported)
  IPC_STRUCT_TRAITS_MEMBER(is_stage3d_supported)
  IPC_STRUCT_TRAITS_MEMBER(is_stage3d_baseline_supported)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(ppapi::InputEventData)
  IPC_STRUCT_TRAITS_MEMBER(is_filtered)
  IPC_STRUCT_TRAITS_MEMBER(event_type)
  IPC_STRUCT_TRAITS_MEMBER(event_time_stamp)
  IPC_STRUCT_TRAITS_MEMBER(event_modifiers)
  IPC_STRUCT_TRAITS_MEMBER(mouse_button)
  IPC_STRUCT_TRAITS_MEMBER(mouse_position)
  IPC_STRUCT_TRAITS_MEMBER(mouse_click_count)
  IPC_STRUCT_TRAITS_MEMBER(mouse_movement)
  IPC_STRUCT_TRAITS_MEMBER(wheel_delta)
  IPC_STRUCT_TRAITS_MEMBER(wheel_ticks)
  IPC_STRUCT_TRAITS_MEMBER(wheel_scroll_by_page)
  IPC_STRUCT_TRAITS_MEMBER(key_code)
  IPC_STRUCT_TRAITS_MEMBER(usb_key_code)
  IPC_STRUCT_TRAITS_MEMBER(character_text)
  IPC_STRUCT_TRAITS_MEMBER(composition_segment_offsets)
  IPC_STRUCT_TRAITS_MEMBER(composition_target_segment)
  IPC_STRUCT_TRAITS_MEMBER(composition_selection_start)
  IPC_STRUCT_TRAITS_MEMBER(composition_selection_end)
  IPC_STRUCT_TRAITS_MEMBER(touches)
  IPC_STRUCT_TRAITS_MEMBER(changed_touches)
  IPC_STRUCT_TRAITS_MEMBER(target_touches)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(ppapi::HostPortPair)
  IPC_STRUCT_TRAITS_MEMBER(host)
  IPC_STRUCT_TRAITS_MEMBER(port)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(ppapi::URLRequestInfoData)
  IPC_STRUCT_TRAITS_MEMBER(url)
  IPC_STRUCT_TRAITS_MEMBER(method)
  IPC_STRUCT_TRAITS_MEMBER(headers)
  IPC_STRUCT_TRAITS_MEMBER(stream_to_file)
  IPC_STRUCT_TRAITS_MEMBER(follow_redirects)
  IPC_STRUCT_TRAITS_MEMBER(record_download_progress)
  IPC_STRUCT_TRAITS_MEMBER(record_upload_progress)
  IPC_STRUCT_TRAITS_MEMBER(has_custom_referrer_url)
  IPC_STRUCT_TRAITS_MEMBER(custom_referrer_url)
  IPC_STRUCT_TRAITS_MEMBER(allow_cross_origin_requests)
  IPC_STRUCT_TRAITS_MEMBER(allow_credentials)
  IPC_STRUCT_TRAITS_MEMBER(has_custom_content_transfer_encoding)
  IPC_STRUCT_TRAITS_MEMBER(custom_content_transfer_encoding)
  IPC_STRUCT_TRAITS_MEMBER(prefetch_buffer_upper_threshold)
  IPC_STRUCT_TRAITS_MEMBER(prefetch_buffer_lower_threshold)
  IPC_STRUCT_TRAITS_MEMBER(has_custom_user_agent)
  IPC_STRUCT_TRAITS_MEMBER(custom_user_agent)
  IPC_STRUCT_TRAITS_MEMBER(body)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(ppapi::URLRequestInfoData::BodyItem)
  IPC_STRUCT_TRAITS_MEMBER(is_file)
  IPC_STRUCT_TRAITS_MEMBER(data)
  // Note: we don't serialize file_ref.
  IPC_STRUCT_TRAITS_MEMBER(file_ref_host_resource)
  IPC_STRUCT_TRAITS_MEMBER(start_offset)
  IPC_STRUCT_TRAITS_MEMBER(number_of_bytes)
  IPC_STRUCT_TRAITS_MEMBER(expected_last_modified_time)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(ppapi::URLResponseInfoData)
  IPC_STRUCT_TRAITS_MEMBER(url)
  IPC_STRUCT_TRAITS_MEMBER(headers)
  IPC_STRUCT_TRAITS_MEMBER(status_code)
  IPC_STRUCT_TRAITS_MEMBER(status_text)
  IPC_STRUCT_TRAITS_MEMBER(redirect_url)
  IPC_STRUCT_TRAITS_MEMBER(body_as_file_ref)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(ppapi::NetworkInfo)
  IPC_STRUCT_TRAITS_MEMBER(name)
  IPC_STRUCT_TRAITS_MEMBER(type)
  IPC_STRUCT_TRAITS_MEMBER(state)
  IPC_STRUCT_TRAITS_MEMBER(addresses)
  IPC_STRUCT_TRAITS_MEMBER(display_name)
  IPC_STRUCT_TRAITS_MEMBER(mtu)
IPC_STRUCT_TRAITS_END()

#if !defined(OS_NACL) && !defined(NACL_WIN64)

IPC_STRUCT_TRAITS_BEGIN(ppapi::proxy::PPPDecryptor_Buffer)
  IPC_STRUCT_TRAITS_MEMBER(resource)
  IPC_STRUCT_TRAITS_MEMBER(handle)
  IPC_STRUCT_TRAITS_MEMBER(size)
IPC_STRUCT_TRAITS_END()

#endif  // !defined(OS_NACL) && !defined(NACL_WIN64)

// These are from the browser to the plugin.
// Loads the given plugin.
IPC_MESSAGE_CONTROL2(PpapiMsg_LoadPlugin,
                     base::FilePath /* path */,
                     ppapi::PpapiPermissions /* permissions */)

// Creates a channel to talk to a renderer. The plugin will respond with
// PpapiHostMsg_ChannelCreated.
IPC_MESSAGE_CONTROL3(PpapiMsg_CreateChannel,
                     base::ProcessId /* renderer_pid */,
                     int /* renderer_child_id */,
                     bool /* incognito */)

// Creates a channel to talk to a renderer. This message is only used by the
// NaCl IPC proxy. It is intercepted by NaClIPCAdapter, which creates the
// actual channel and rewrites the message for the untrusted side.
IPC_MESSAGE_CONTROL4(PpapiMsg_CreateNaClChannel,
                     int /* renderer_id */,
                     ppapi::PpapiPermissions /* permissions */,
                     bool /* incognito */,
                     ppapi::proxy::SerializedHandle /* channel_handle */)

// Instructs the plugin process to crash.
IPC_MESSAGE_CONTROL0(PpapiMsg_Crash)

// Instructs the plugin process to hang.
IPC_MESSAGE_CONTROL0(PpapiMsg_Hang)

// Each plugin may be referenced by multiple renderers. We need the instance
// IDs to be unique within a plugin, despite coming from different renderers,
// and unique within a renderer, despite going to different plugins. This means
// that neither the renderer nor the plugin can generate instance IDs without
// consulting the other.
//
// We resolve this by having the renderer generate a unique instance ID inside
// its process. It then asks the plugin to reserve that ID by sending this sync
// message. If the plugin has not yet seen this ID, it will remember it as used
// (to prevent a race condition if another renderer tries to then use the same
// instance), and set usable as true.
//
// If the plugin has already seen the instance ID, it will set usable as false
// and the renderer must retry a new instance ID.
IPC_SYNC_MESSAGE_CONTROL1_1(PpapiMsg_ReserveInstanceId,
                            PP_Instance /* instance */,
                            bool /* usable */)

// Passes the WebKit preferences to the plugin.
IPC_MESSAGE_CONTROL1(PpapiMsg_SetPreferences,
                     ppapi::Preferences)

// Sent in both directions to see if the other side supports the given
// interface.
IPC_SYNC_MESSAGE_CONTROL1_1(PpapiMsg_SupportsInterface,
                            std::string /* interface_name */,
                            bool /* result */)

#if !defined(OS_NACL) && !defined(NACL_WIN64)
// Network state notification from the browser for implementing
// PPP_NetworkState_Dev.
IPC_MESSAGE_CONTROL1(PpapiMsg_SetNetworkState,
                     bool /* online */)

// Requests a list of sites that have data stored from the plugin. The plugin
// process will respond with PpapiHostMsg_GetSitesWithDataResult. This is used
// for Flash.
IPC_MESSAGE_CONTROL2(PpapiMsg_GetSitesWithData,
                     uint32 /* request_id */,
                     base::FilePath /* plugin_data_path */)
IPC_MESSAGE_CONTROL2(PpapiHostMsg_GetSitesWithDataResult,
                     uint32 /* request_id */,
                     std::vector<std::string> /* sites */)

// Instructs the plugin to clear data for the given site & time. The plugin
// process will respond with PpapiHostMsg_ClearSiteDataResult. This is used
// for Flash.
IPC_MESSAGE_CONTROL5(PpapiMsg_ClearSiteData,
                     uint32 /* request_id */,
                     base::FilePath /* plugin_data_path */,
                     std::string /* site */,
                     uint64 /* flags */,
                     uint64 /* max_age */)
IPC_MESSAGE_CONTROL2(PpapiHostMsg_ClearSiteDataResult,
                     uint32 /* request_id */,
                     bool /* success */)

IPC_MESSAGE_CONTROL2(PpapiMsg_DeauthorizeContentLicenses,
                     uint32 /* request_id */,
                     base::FilePath /* plugin_data_path */)
IPC_MESSAGE_CONTROL2(PpapiHostMsg_DeauthorizeContentLicensesResult,
                     uint32 /* request_id */,
                     bool /* success */)

IPC_MESSAGE_CONTROL3(PpapiMsg_GetPermissionSettings,
                     uint32 /* request_id */,
                     base::FilePath /* plugin_data_path */,
                     PP_Flash_BrowserOperations_SettingType /* setting_type */)
IPC_MESSAGE_CONTROL4(
    PpapiHostMsg_GetPermissionSettingsResult,
    uint32 /* request_id */,
    bool /* success */,
    PP_Flash_BrowserOperations_Permission /* default_permission */,
    ppapi::FlashSiteSettings /* sites */)

IPC_MESSAGE_CONTROL5(PpapiMsg_SetDefaultPermission,
                     uint32 /* request_id */,
                     base::FilePath /* plugin_data_path */,
                     PP_Flash_BrowserOperations_SettingType /* setting_type */,
                     PP_Flash_BrowserOperations_Permission /* permission */,
                     bool /* clear_site_specific */)
IPC_MESSAGE_CONTROL2(PpapiHostMsg_SetDefaultPermissionResult,
                     uint32 /* request_id */,
                     bool /* success */)

IPC_MESSAGE_CONTROL4(PpapiMsg_SetSitePermission,
                     uint32 /* request_id */,
                     base::FilePath /* plugin_data_path */,
                     PP_Flash_BrowserOperations_SettingType /* setting_type */,
                     ppapi::FlashSiteSettings /* sites */)
IPC_MESSAGE_CONTROL2(PpapiHostMsg_SetSitePermissionResult,
                     uint32 /* request_id */,
                     bool /* success */)

// Broker Process.
IPC_SYNC_MESSAGE_CONTROL2_1(PpapiMsg_ConnectToPlugin,
                            PP_Instance /* instance */,
                            IPC::PlatformFileForTransit /* handle */,
                            int32_t /* result */)
#endif  // !defined(OS_NACL) && !defined(NACL_WIN64)

// PPB_Audio.

// Notifies the result of the audio stream create call. This is called in
// both error cases and in the normal success case. These cases are
// differentiated by the result code, which is one of the standard PPAPI
// result codes.
//
// The handler of this message should always close all of the handles passed
// in, since some could be valid even in the error case.
IPC_MESSAGE_ROUTED4(PpapiMsg_PPBAudio_NotifyAudioStreamCreated,
                    ppapi::HostResource /* audio_id */,
                    int32_t /* result_code (will be != PP_OK on failure) */,
                    ppapi::proxy::SerializedHandle /* socket_handle */,
                    ppapi::proxy::SerializedHandle /* handle */)

// PPB_FileRef.
IPC_MESSAGE_ROUTED3(
    PpapiMsg_PPBFileRef_CallbackComplete,
    ppapi::HostResource /* resource */,
    int /* callback_id */,
    int32_t /* result */)

// PPB_FileSystem.
IPC_MESSAGE_ROUTED2(
    PpapiMsg_PPBFileSystem_OpenComplete,
    ppapi::HostResource /* filesystem */,
    int32_t /* result */)

// PPB_Graphics3D.
IPC_MESSAGE_ROUTED2(PpapiMsg_PPBGraphics3D_SwapBuffersACK,
                    ppapi::HostResource /* graphics_3d */,
                    int32_t /* pp_error */)

// PPB_ImageData.
IPC_MESSAGE_ROUTED1(PpapiMsg_PPBImageData_NotifyUnusedImageData,
                    ppapi::HostResource /* old_image_data */)

// PPB_Instance.
IPC_MESSAGE_ROUTED2(PpapiMsg_PPBInstance_MouseLockComplete,
                    PP_Instance /* instance */,
                    int32_t /* result */)

// PPP_Class.
IPC_SYNC_MESSAGE_ROUTED3_2(PpapiMsg_PPPClass_HasProperty,
                           int64 /* ppp_class */,
                           int64 /* object */,
                           ppapi::proxy::SerializedVar /* property */,
                           ppapi::proxy::SerializedVar /* out_exception */,
                           bool /* result */)
IPC_SYNC_MESSAGE_ROUTED3_2(PpapiMsg_PPPClass_HasMethod,
                           int64 /* ppp_class */,
                           int64 /* object */,
                           ppapi::proxy::SerializedVar /* method */,
                           ppapi::proxy::SerializedVar /* out_exception */,
                           bool /* result */)
IPC_SYNC_MESSAGE_ROUTED3_2(PpapiMsg_PPPClass_GetProperty,
                           int64 /* ppp_class */,
                           int64 /* object */,
                           ppapi::proxy::SerializedVar /* property */,
                           ppapi::proxy::SerializedVar /* out_exception */,
                           ppapi::proxy::SerializedVar /* result */)
IPC_SYNC_MESSAGE_ROUTED2_2(PpapiMsg_PPPClass_EnumerateProperties,
                           int64 /* ppp_class */,
                           int64 /* object */,
                           std::vector<ppapi::proxy::SerializedVar> /* props */,
                           ppapi::proxy::SerializedVar /* out_exception */)
IPC_SYNC_MESSAGE_ROUTED4_1(PpapiMsg_PPPClass_SetProperty,
                           int64 /* ppp_class */,
                           int64 /* object */,
                           ppapi::proxy::SerializedVar /* name */,
                           ppapi::proxy::SerializedVar /* value */,
                           ppapi::proxy::SerializedVar /* out_exception */)
IPC_SYNC_MESSAGE_ROUTED3_1(PpapiMsg_PPPClass_RemoveProperty,
                           int64 /* ppp_class */,
                           int64 /* object */,
                           ppapi::proxy::SerializedVar /* property */,
                           ppapi::proxy::SerializedVar /* out_exception */)
IPC_SYNC_MESSAGE_ROUTED4_2(PpapiMsg_PPPClass_Call,
                           int64 /* ppp_class */,
                           int64 /* object */,
                           ppapi::proxy::SerializedVar /* method_name */,
                           std::vector<ppapi::proxy::SerializedVar> /* args */,
                           ppapi::proxy::SerializedVar /* out_exception */,
                           ppapi::proxy::SerializedVar /* result */)
IPC_SYNC_MESSAGE_ROUTED3_2(PpapiMsg_PPPClass_Construct,
                           int64 /* ppp_class */,
                           int64 /* object */,
                           std::vector<ppapi::proxy::SerializedVar> /* args */,
                           ppapi::proxy::SerializedVar /* out_exception */,
                           ppapi::proxy::SerializedVar /* result */)
IPC_MESSAGE_ROUTED2(PpapiMsg_PPPClass_Deallocate,
                    int64 /* ppp_class */,
                    int64 /* object */)

// PPP_Graphics3D_Dev.
IPC_MESSAGE_ROUTED1(PpapiMsg_PPPGraphics3D_ContextLost,
                    PP_Instance /* instance */)

// PPP_InputEvent.
IPC_MESSAGE_ROUTED2(PpapiMsg_PPPInputEvent_HandleInputEvent,
                    PP_Instance /* instance */,
                    ppapi::InputEventData /* data */)
IPC_SYNC_MESSAGE_ROUTED2_1(PpapiMsg_PPPInputEvent_HandleFilteredInputEvent,
                           PP_Instance /* instance */,
                           ppapi::InputEventData /* data */,
                           PP_Bool /* result */)

// PPP_Instance.
IPC_SYNC_MESSAGE_ROUTED3_1(PpapiMsg_PPPInstance_DidCreate,
                           PP_Instance /* instance */,
                           std::vector<std::string> /* argn */,
                           std::vector<std::string> /* argv */,
                           PP_Bool /* result */)
IPC_SYNC_MESSAGE_ROUTED1_0(PpapiMsg_PPPInstance_DidDestroy,
                           PP_Instance /* instance */)
IPC_MESSAGE_ROUTED3(PpapiMsg_PPPInstance_DidChangeView,
                    PP_Instance /* instance */,
                    ppapi::ViewData /* new_data */,
                    PP_Bool /* flash_fullscreen */)
IPC_MESSAGE_ROUTED2(PpapiMsg_PPPInstance_DidChangeFocus,
                    PP_Instance /* instance */,
                    PP_Bool /* has_focus */)
IPC_SYNC_MESSAGE_ROUTED2_1(PpapiMsg_PPPInstance_HandleDocumentLoad,
                           PP_Instance /* instance */,
                           ppapi::HostResource /* url_loader */,
                           PP_Bool /* result */)

// PPP_Messaging.
IPC_MESSAGE_ROUTED2(PpapiMsg_PPPMessaging_HandleMessage,
                    PP_Instance /* instance */,
                    ppapi::proxy::SerializedVar /* message */)

// PPP_MouseLock.
IPC_MESSAGE_ROUTED1(PpapiMsg_PPPMouseLock_MouseLockLost,
                    PP_Instance /* instance */)

// PPB_NetworkMonitor_Private.
IPC_MESSAGE_ROUTED2(PpapiMsg_PPBNetworkMonitor_NetworkList,
                    uint32 /* plugin_dispatcher_id */,
                    ppapi::NetworkList /* network_list */)

// PPP_Printing
IPC_SYNC_MESSAGE_ROUTED1_1(PpapiMsg_PPPPrinting_QuerySupportedFormats,
                           PP_Instance /* instance */,
                           uint32_t /* result */)
IPC_SYNC_MESSAGE_ROUTED2_1(PpapiMsg_PPPPrinting_Begin,
                           PP_Instance /* instance */,
                           std::string /* settings_string */,
                           int32_t /* result */)
IPC_SYNC_MESSAGE_ROUTED2_1(PpapiMsg_PPPPrinting_PrintPages,
                           PP_Instance /* instance */,
                           std::vector<PP_PrintPageNumberRange_Dev> /* pages */,
                           ppapi::HostResource /* result */)
IPC_MESSAGE_ROUTED1(PpapiMsg_PPPPrinting_End,
                    PP_Instance /* instance */)
IPC_SYNC_MESSAGE_ROUTED1_1(PpapiMsg_PPPPrinting_IsScalingDisabled,
                           PP_Instance /* instance */,
                           bool /* result */)

// PPP_TextInput.
IPC_MESSAGE_ROUTED2(PpapiMsg_PPPTextInput_RequestSurroundingText,
                   PP_Instance /* instance */,
                   uint32_t /* desired_number_of_characters */)

// PPB_URLLoader
// (Messages from browser to plugin to notify it of changes in state.)
//
// NOTE: The ReadResponseBody_Ack message is a custom generated message
// with the following fields appended:
//   ppapi::HostResource
//   response data (array of bytes stored via WriteData)
//   int result
//
IPC_MESSAGE_ROUTED0(PpapiMsg_PPBURLLoader_ReadResponseBody_Ack)
IPC_MESSAGE_ROUTED2(PpapiMsg_PPBURLLoader_CallbackComplete,
                    ppapi::HostResource /* loader */,
                    int32_t /* result */)
#if !defined(OS_NACL) && !defined(NACL_WIN64)
// PPB_Broker.
IPC_MESSAGE_ROUTED3(
    PpapiMsg_PPBBroker_ConnectComplete,
    ppapi::HostResource /* broker */,
    IPC::PlatformFileForTransit /* handle */,
    int32_t /* result */)

// PPP_ContentDecryptor_Dev
IPC_MESSAGE_ROUTED4(PpapiMsg_PPPContentDecryptor_GenerateKeyRequest,
                    PP_Instance /* instance */,
                    ppapi::proxy::SerializedVar /* key_system, String */,
                    ppapi::proxy::SerializedVar /* type, String */,
                    ppapi::proxy::SerializedVar /* init_data, ArrayBuffer */)
IPC_MESSAGE_ROUTED4(PpapiMsg_PPPContentDecryptor_AddKey,
                    PP_Instance /* instance */,
                    ppapi::proxy::SerializedVar /* session_id, String */,
                    ppapi::proxy::SerializedVar /* key, ArrayBuffer */,
                    ppapi::proxy::SerializedVar /* init_data, ArrayBuffer */)
IPC_MESSAGE_ROUTED2(PpapiMsg_PPPContentDecryptor_CancelKeyRequest,
                    PP_Instance /* instance */,
                    ppapi::proxy::SerializedVar /* session_id, String */)
IPC_MESSAGE_ROUTED3(PpapiMsg_PPPContentDecryptor_Decrypt,
                    PP_Instance /* instance */,
                    ppapi::proxy::PPPDecryptor_Buffer /* buffer */,
                    std::string /* serialized_block_info */)
IPC_MESSAGE_ROUTED3(
    PpapiMsg_PPPContentDecryptor_InitializeAudioDecoder,
    PP_Instance /* instance */,
    std::string /* serialized_decoder_config */,
    ppapi::proxy::PPPDecryptor_Buffer /* extra_data_buffer */)
IPC_MESSAGE_ROUTED3(
    PpapiMsg_PPPContentDecryptor_InitializeVideoDecoder,
    PP_Instance /* instance */,
    std::string /* serialized_decoder_config */,
    ppapi::proxy::PPPDecryptor_Buffer /* extra_data_buffer. */)
IPC_MESSAGE_ROUTED3(PpapiMsg_PPPContentDecryptor_DeinitializeDecoder,
                    PP_Instance /* instance */,
                    PP_DecryptorStreamType /* decoder_type */,
                    uint32_t /* request_id */)
IPC_MESSAGE_ROUTED3(PpapiMsg_PPPContentDecryptor_ResetDecoder,
                    PP_Instance /* instance */,
                    PP_DecryptorStreamType /* decoder_type */,
                    uint32_t /* request_id */)
IPC_MESSAGE_ROUTED4(PpapiMsg_PPPContentDecryptor_DecryptAndDecode,
                    PP_Instance /* instance */,
                    PP_DecryptorStreamType /* decoder_type */,
                    ppapi::proxy::PPPDecryptor_Buffer /* buffer */,
                    std::string /* serialized_block_info */)
#endif  // !defined(OS_NACL) && !defined(NACL_WIN64)

// PPB_TCPSocket_Private.
IPC_MESSAGE_ROUTED5(PpapiMsg_PPBTCPSocket_ConnectACK,
                    uint32 /* plugin_dispatcher_id */,
                    uint32 /* socket_id */,
                    bool /* succeeded */,
                    PP_NetAddress_Private /* local_addr */,
                    PP_NetAddress_Private /* remote_addr */)
IPC_MESSAGE_ROUTED4(PpapiMsg_PPBTCPSocket_SSLHandshakeACK,
                    uint32 /* plugin_dispatcher_id */,
                    uint32 /* socket_id */,
                    bool /* succeeded */,
                    ppapi::PPB_X509Certificate_Fields /* certificate_fields */)
IPC_MESSAGE_ROUTED4(PpapiMsg_PPBTCPSocket_ReadACK,
                    uint32 /* plugin_dispatcher_id */,
                    uint32 /* socket_id */,
                    bool /* succeeded */,
                    std::string /* data */)
IPC_MESSAGE_ROUTED4(PpapiMsg_PPBTCPSocket_WriteACK,
                    uint32 /* plugin_dispatcher_id */,
                    uint32 /* socket_id */,
                    bool /* succeeded */,
                    int32_t /* bytes_written */)
IPC_MESSAGE_ROUTED3(PpapiMsg_PPBTCPSocket_SetBoolOptionACK,
                    uint32 /* plugin_dispatcher_id */,
                    uint32 /* socket_id */,
                    bool /* succeeded */)

// PPB_URLLoader_Trusted
IPC_MESSAGE_ROUTED1(
    PpapiMsg_PPBURLLoader_UpdateProgress,
    ppapi::proxy::PPBURLLoader_UpdateProgress_Params /* params */)

// PPB_TCPServerSocket_Private.

// |socket_resource| should not be used as Resource in browser. The
// only purpose of this argument is to be echoed back.
// |status| == PP_ERROR_NOSPACE means that the socket table is full
// and new socket can't be initialized.
// |status| == PP_ERROR_FAILED means that socket is correctly
// initialized (if needed) but Listen call is failed.
// |status| == PP_OK means that socket is correctly initialized (if
// needed) and Listen call succeeds.
IPC_MESSAGE_ROUTED4(PpapiMsg_PPBTCPServerSocket_ListenACK,
                    uint32 /* plugin_dispatcher_id */,
                    PP_Resource /* socket_resource */,
                    uint32 /* socket_id */,
                    int32_t /* status */)
IPC_MESSAGE_ROUTED5(PpapiMsg_PPBTCPServerSocket_AcceptACK,
                    uint32 /* plugin_dispatcher_id */,
                    uint32 /* server_socket_id */,
                    uint32 /* accepted_socket_id */,
                    PP_NetAddress_Private /* local_addr */,
                    PP_NetAddress_Private /* remote_addr */)

#if !defined(OS_NACL) && !defined(NACL_WIN64)
// PPP_Instance_Private.
IPC_SYNC_MESSAGE_ROUTED1_1(PpapiMsg_PPPInstancePrivate_GetInstanceObject,
                           PP_Instance /* instance */,
                           ppapi::proxy::SerializedVar /* result */)

// PPB_VideoDecoder_Dev.
// (Messages from renderer to plugin to notify it to run callbacks.)
IPC_MESSAGE_ROUTED3(PpapiMsg_PPBVideoDecoder_EndOfBitstreamACK,
                    ppapi::HostResource /* video_decoder */,
                    int32_t /* bitstream buffer id */,
                    int32_t /* PP_CompletionCallback result */)
IPC_MESSAGE_ROUTED2(PpapiMsg_PPBVideoDecoder_FlushACK,
                    ppapi::HostResource /* video_decoder */,
                    int32_t /* PP_CompletionCallback result  */)
IPC_MESSAGE_ROUTED2(PpapiMsg_PPBVideoDecoder_ResetACK,
                    ppapi::HostResource /* video_decoder */,
                    int32_t /* PP_CompletionCallback result */)

// PPP_VideoDecoder_Dev.
IPC_MESSAGE_ROUTED4(PpapiMsg_PPPVideoDecoder_ProvidePictureBuffers,
                    ppapi::HostResource /* video_decoder */,
                    uint32_t /* requested number of buffers */,
                    PP_Size /* dimensions of buffers */,
                    uint32_t /* texture_target */)
IPC_MESSAGE_ROUTED2(PpapiMsg_PPPVideoDecoder_DismissPictureBuffer,
                    ppapi::HostResource /* video_decoder */,
                    int32_t /* picture buffer id */)
IPC_MESSAGE_ROUTED2(PpapiMsg_PPPVideoDecoder_PictureReady,
                    ppapi::HostResource /* video_decoder */,
                    PP_Picture_Dev /* output picture */)
IPC_MESSAGE_ROUTED2(PpapiMsg_PPPVideoDecoder_NotifyError,
                    ppapi::HostResource /* video_decoder */,
                    PP_VideoDecodeError_Dev /* error */)
#endif  // !defined(OS_NACL) && !defined(NACL_WIN64)

// -----------------------------------------------------------------------------
// These are from the plugin to the renderer.

// Reply to PpapiMsg_CreateChannel. The handle will be NULL if the channel
// could not be established. This could be because the IPC could not be created
// for some weird reason, but more likely that the plugin failed to load or
// initialize properly.
IPC_MESSAGE_CONTROL1(PpapiHostMsg_ChannelCreated,
                     IPC::ChannelHandle /* handle */)

// Logs the given message to the console of all instances.
IPC_MESSAGE_CONTROL4(PpapiHostMsg_LogWithSource,
                     PP_Instance /* instance */,
                     int /* log_level */,
                     std::string /* source */,
                     std::string /* value */)

// PPB_Audio.
IPC_SYNC_MESSAGE_ROUTED3_1(PpapiHostMsg_PPBAudio_Create,
                           PP_Instance /* instance_id */,
                           int32_t /* sample_rate */,
                           uint32_t /* sample_frame_count */,
                           ppapi::HostResource /* result */)
IPC_MESSAGE_ROUTED2(PpapiHostMsg_PPBAudio_StartOrStop,
                    ppapi::HostResource /* audio_id */,
                    bool /* play */)

// PPB_Core.
IPC_MESSAGE_ROUTED1(PpapiHostMsg_PPBCore_AddRefResource,
                    ppapi::HostResource)
IPC_MESSAGE_ROUTED1(PpapiHostMsg_PPBCore_ReleaseResource,
                    ppapi::HostResource)

// PPB_FileRef.
IPC_SYNC_MESSAGE_ROUTED2_1(PpapiHostMsg_PPBFileRef_Create,
                           ppapi::HostResource /* file_system */,
                           std::string /* path */,
                           ppapi::PPB_FileRef_CreateInfo /* result */)
IPC_SYNC_MESSAGE_ROUTED1_1(PpapiHostMsg_PPBFileRef_GetParent,
                           ppapi::HostResource /* file_ref */,
                           ppapi::PPB_FileRef_CreateInfo /* result */)
IPC_MESSAGE_ROUTED3(PpapiHostMsg_PPBFileRef_MakeDirectory,
                    ppapi::HostResource /* file_ref */,
                    PP_Bool /* make_ancestors */,
                    int /* callback_id */)
IPC_MESSAGE_ROUTED4(PpapiHostMsg_PPBFileRef_Touch,
                    ppapi::HostResource /* file_ref */,
                    PP_Time /* last_access */,
                    PP_Time /* last_modified */,
                    int /* callback_id */)
IPC_MESSAGE_ROUTED2(PpapiHostMsg_PPBFileRef_Delete,
                    ppapi::HostResource /* file_ref */,
                    int /* callback_id */)
IPC_MESSAGE_ROUTED3(PpapiHostMsg_PPBFileRef_Rename,
                    ppapi::HostResource /* file_ref */,
                    ppapi::HostResource /* new_file_ref */,
                    int /* callback_id */)
IPC_SYNC_MESSAGE_ROUTED1_1(PpapiHostMsg_PPBFileRef_GetAbsolutePath,
                           ppapi::HostResource /* file_ref */,
                           ppapi::proxy::SerializedVar /* result */)

// PPB_FileSystem.
IPC_SYNC_MESSAGE_ROUTED2_1(PpapiHostMsg_PPBFileSystem_Create,
                           PP_Instance /* instance */,
                           int /* type */,
                           ppapi::HostResource /* result */)
IPC_MESSAGE_ROUTED2(PpapiHostMsg_PPBFileSystem_Open,
                    ppapi::HostResource /* result */,
                    int64_t /* expected_size */)

// PPB_Graphics3D.
IPC_SYNC_MESSAGE_ROUTED3_1(PpapiHostMsg_PPBGraphics3D_Create,
                           PP_Instance /* instance */,
                           ppapi::HostResource /* share_context */,
                           std::vector<int32_t> /* attrib_list */,
                           ppapi::HostResource /* result */)
IPC_SYNC_MESSAGE_ROUTED1_0(PpapiHostMsg_PPBGraphics3D_InitCommandBuffer,
                           ppapi::HostResource /* context */)
IPC_SYNC_MESSAGE_ROUTED2_0(PpapiHostMsg_PPBGraphics3D_SetGetBuffer,
                           ppapi::HostResource /* context */,
                           int32 /* transfer_buffer_id */)
IPC_SYNC_MESSAGE_ROUTED1_2(PpapiHostMsg_PPBGraphics3D_GetState,
                           ppapi::HostResource /* context */,
                           gpu::CommandBuffer::State /* state */,
                           bool /* success */)
IPC_SYNC_MESSAGE_ROUTED3_2(PpapiHostMsg_PPBGraphics3D_Flush,
                           ppapi::HostResource /* context */,
                           int32 /* put_offset */,
                           int32 /* last_known_get */,
                           gpu::CommandBuffer::State /* state */,
                           bool /* success */)
IPC_MESSAGE_ROUTED2(PpapiHostMsg_PPBGraphics3D_AsyncFlush,
                    ppapi::HostResource /* context */,
                    int32 /* put_offset */)
IPC_SYNC_MESSAGE_ROUTED2_1(PpapiHostMsg_PPBGraphics3D_CreateTransferBuffer,
                           ppapi::HostResource /* context */,
                           uint32 /* size */,
                           int32 /* id */)
IPC_SYNC_MESSAGE_ROUTED2_0(PpapiHostMsg_PPBGraphics3D_DestroyTransferBuffer,
                           ppapi::HostResource /* context */,
                           int32 /* id */)
IPC_SYNC_MESSAGE_ROUTED2_1(PpapiHostMsg_PPBGraphics3D_GetTransferBuffer,
                           ppapi::HostResource /* context */,
                           int32 /* id */,
                           ppapi::proxy::SerializedHandle /* transfer_buffer */)
IPC_MESSAGE_ROUTED1(PpapiHostMsg_PPBGraphics3D_SwapBuffers,
                    ppapi::HostResource /* graphics_3d */)

// PPB_ImageData.
IPC_SYNC_MESSAGE_ROUTED4_3(PpapiHostMsg_PPBImageData_Create,
                           PP_Instance /* instance */,
                           int32 /* format */,
                           PP_Size /* size */,
                           PP_Bool /* init_to_zero */,
                           ppapi::HostResource /* result_resource */,
                           std::string /* image_data_desc */,
                           ppapi::proxy::ImageHandle /* result */)
IPC_SYNC_MESSAGE_ROUTED4_3(PpapiHostMsg_PPBImageData_CreateNaCl,
                           PP_Instance /* instance */,
                           int32 /* format */,
                           PP_Size /* size */,
                           PP_Bool /* init_to_zero */,
                           ppapi::HostResource /* result_resource */,
                           std::string /* image_data_desc */,
                           ppapi::proxy::SerializedHandle /* result */)

// PPB_Instance.
IPC_SYNC_MESSAGE_ROUTED1_1(PpapiHostMsg_PPBInstance_GetWindowObject,
                           PP_Instance /* instance */,
                           ppapi::proxy::SerializedVar /* result */)
IPC_SYNC_MESSAGE_ROUTED1_1(PpapiHostMsg_PPBInstance_GetOwnerElementObject,
                           PP_Instance /* instance */,
                           ppapi::proxy::SerializedVar /* result */)
IPC_MESSAGE_ROUTED2(PpapiHostMsg_PPBInstance_BindGraphics,
                    PP_Instance /* instance */,
                    PP_Resource /* device */)
IPC_SYNC_MESSAGE_ROUTED1_1(
    PpapiHostMsg_PPBInstance_GetAudioHardwareOutputSampleRate,
                           PP_Instance /* instance */,
                           uint32_t /* result */)
IPC_SYNC_MESSAGE_ROUTED1_1(
    PpapiHostMsg_PPBInstance_GetAudioHardwareOutputBufferSize,
                           PP_Instance /* instance */,
                           uint32_t /* result */)
IPC_SYNC_MESSAGE_ROUTED1_1(PpapiHostMsg_PPBInstance_IsFullFrame,
                           PP_Instance /* instance */,
                           PP_Bool /* result */)
IPC_SYNC_MESSAGE_ROUTED2_2(PpapiHostMsg_PPBInstance_ExecuteScript,
                           PP_Instance /* instance */,
                           ppapi::proxy::SerializedVar /* script */,
                           ppapi::proxy::SerializedVar /* out_exception */,
                           ppapi::proxy::SerializedVar /* result */)
IPC_SYNC_MESSAGE_ROUTED1_1(PpapiHostMsg_PPBInstance_GetDefaultCharSet,
                           PP_Instance /* instance */,
                           ppapi::proxy::SerializedVar /* result */)
IPC_SYNC_MESSAGE_ROUTED2_1(PpapiHostMsg_PPBInstance_SetFullscreen,
                           PP_Instance /* instance */,
                           PP_Bool /* fullscreen */,
                           PP_Bool /* result */)
IPC_SYNC_MESSAGE_ROUTED1_2(PpapiHostMsg_PPBInstance_GetScreenSize,
                           PP_Instance /* instance */,
                           PP_Bool /* result */,
                           PP_Size /* size */)
IPC_MESSAGE_ROUTED3(PpapiHostMsg_PPBInstance_RequestInputEvents,
                    PP_Instance /* instance */,
                    bool /* is_filtering */,
                    uint32_t /* event_classes */)
IPC_MESSAGE_ROUTED2(PpapiHostMsg_PPBInstance_ClearInputEvents,
                    PP_Instance /* instance */,
                    uint32_t /* event_classes */)
IPC_MESSAGE_ROUTED2(PpapiHostMsg_PPBInstance_PostMessage,
                    PP_Instance /* instance */,
                    ppapi::proxy::SerializedVar /* message */)
IPC_MESSAGE_ROUTED1(PpapiHostMsg_PPBInstance_LockMouse,
                    PP_Instance /* instance */)
IPC_MESSAGE_ROUTED1(PpapiHostMsg_PPBInstance_UnlockMouse,
                    PP_Instance /* instance */)
IPC_SYNC_MESSAGE_ROUTED2_1(PpapiHostMsg_PPBInstance_ResolveRelativeToDocument,
                           PP_Instance /* instance */,
                           ppapi::proxy::SerializedVar /* relative */,
                           ppapi::proxy::SerializedVar /* result */)
IPC_SYNC_MESSAGE_ROUTED2_1(PpapiHostMsg_PPBInstance_DocumentCanRequest,
                           PP_Instance /* instance */,
                           ppapi::proxy::SerializedVar /* relative */,
                           PP_Bool /* result */)
IPC_SYNC_MESSAGE_ROUTED2_1(PpapiHostMsg_PPBInstance_DocumentCanAccessDocument,
                           PP_Instance /* active */,
                           PP_Instance /* target */,
                           PP_Bool /* result */)
IPC_SYNC_MESSAGE_ROUTED1_2(PpapiHostMsg_PPBInstance_GetDocumentURL,
                           PP_Instance /* active */,
                           PP_URLComponents_Dev /* components */,
                           ppapi::proxy::SerializedVar /* result */)
IPC_SYNC_MESSAGE_ROUTED1_1(PpapiHostMsg_PPBInstance_GetPluginInstanceURL,
                           PP_Instance /* active */,
                           ppapi::proxy::SerializedVar /* result */)
IPC_MESSAGE_ROUTED4(PpapiHostMsg_PPBInstance_SetCursor,
                    PP_Instance /* instance */,
                    int32_t /* type */,
                    ppapi::HostResource /* custom_image */,
                    PP_Point /* hot_spot */)
IPC_MESSAGE_ROUTED2(PpapiHostMsg_PPBInstance_SetTextInputType,
                    PP_Instance /* instance */,
                    PP_TextInput_Type /* type */)
IPC_MESSAGE_ROUTED3(PpapiHostMsg_PPBInstance_UpdateCaretPosition,
                    PP_Instance /* instance */,
                    PP_Rect /* caret */,
                    PP_Rect /* bounding_box */)
IPC_MESSAGE_ROUTED1(PpapiHostMsg_PPBInstance_CancelCompositionText,
                    PP_Instance /* instance */)
IPC_MESSAGE_ROUTED4(PpapiHostMsg_PPBInstance_UpdateSurroundingText,
                    PP_Instance /* instance */,
                    std::string /* text */,
                    uint32_t /* caret */,
                    uint32_t /* anchor */)

// PPB_URLLoader.
IPC_SYNC_MESSAGE_ROUTED1_1(PpapiHostMsg_PPBURLLoader_Create,
                           PP_Instance /* instance */,
                           ppapi::HostResource /* result */)
IPC_MESSAGE_ROUTED2(PpapiHostMsg_PPBURLLoader_Open,
                    ppapi::HostResource /* loader */,
                    ppapi::URLRequestInfoData /* request_data */)
IPC_MESSAGE_ROUTED1(PpapiHostMsg_PPBURLLoader_FollowRedirect,
                    ppapi::HostResource /* loader */)
IPC_SYNC_MESSAGE_ROUTED1_2(
    PpapiHostMsg_PPBURLLoader_GetResponseInfo,
    ppapi::HostResource /* loader */,
    bool /* success */,
    ppapi::URLResponseInfoData /* result */)
IPC_MESSAGE_ROUTED2(PpapiHostMsg_PPBURLLoader_ReadResponseBody,
                    ppapi::HostResource /* loader */,
                    int32_t /* bytes_to_read */)
IPC_MESSAGE_ROUTED1(PpapiHostMsg_PPBURLLoader_FinishStreamingToFile,
                    ppapi::HostResource /* loader */)
IPC_MESSAGE_ROUTED1(PpapiHostMsg_PPBURLLoader_Close,
                    ppapi::HostResource /* loader */)
IPC_MESSAGE_ROUTED1(PpapiHostMsg_PPBURLLoader_GrantUniversalAccess,
                    ppapi::HostResource /* loader */)

// PPB_Var.
IPC_SYNC_MESSAGE_ROUTED1_1(PpapiHostMsg_PPBVar_AddRefObject,
                           int64 /* object_id */,
                           int /* unused - need a return value for sync msgs */)
IPC_MESSAGE_ROUTED1(PpapiHostMsg_PPBVar_ReleaseObject,
                    int64 /* object_id */)
IPC_SYNC_MESSAGE_ROUTED2_2(PpapiHostMsg_PPBVar_HasProperty,
                           ppapi::proxy::SerializedVar /* object */,
                           ppapi::proxy::SerializedVar /* property */,
                           ppapi::proxy::SerializedVar /* out_exception */,
                           PP_Bool /* result */)
IPC_SYNC_MESSAGE_ROUTED2_2(PpapiHostMsg_PPBVar_HasMethodDeprecated,
                           ppapi::proxy::SerializedVar /* object */,
                           ppapi::proxy::SerializedVar /* method */,
                           ppapi::proxy::SerializedVar /* out_exception */,
                           PP_Bool /* result */)
IPC_SYNC_MESSAGE_ROUTED2_2(PpapiHostMsg_PPBVar_GetProperty,
                           ppapi::proxy::SerializedVar /* object */,
                           ppapi::proxy::SerializedVar /* property */,
                           ppapi::proxy::SerializedVar /* out_exception */,
                           ppapi::proxy::SerializedVar /* result */)
IPC_SYNC_MESSAGE_ROUTED2_2(PpapiHostMsg_PPBVar_DeleteProperty,
                           ppapi::proxy::SerializedVar /* object */,
                           ppapi::proxy::SerializedVar /* property */,
                           ppapi::proxy::SerializedVar /* out_exception */,
                           PP_Bool /* result */)
IPC_SYNC_MESSAGE_ROUTED1_2(PpapiHostMsg_PPBVar_EnumerateProperties,
                           ppapi::proxy::SerializedVar /* object */,
                           std::vector<ppapi::proxy::SerializedVar> /* props */,
                           ppapi::proxy::SerializedVar /* out_exception */)
IPC_SYNC_MESSAGE_ROUTED3_1(PpapiHostMsg_PPBVar_SetPropertyDeprecated,
                           ppapi::proxy::SerializedVar /* object */,
                           ppapi::proxy::SerializedVar /* name */,
                           ppapi::proxy::SerializedVar /* value */,
                           ppapi::proxy::SerializedVar /* out_exception */)
IPC_SYNC_MESSAGE_ROUTED3_2(PpapiHostMsg_PPBVar_CallDeprecated,
                           ppapi::proxy::SerializedVar /* object */,
                           ppapi::proxy::SerializedVar /* method_name */,
                           std::vector<ppapi::proxy::SerializedVar> /* args */,
                           ppapi::proxy::SerializedVar /* out_exception */,
                           ppapi::proxy::SerializedVar /* result */)
IPC_SYNC_MESSAGE_ROUTED2_2(PpapiHostMsg_PPBVar_Construct,
                           ppapi::proxy::SerializedVar /* object */,
                           std::vector<ppapi::proxy::SerializedVar> /* args */,
                           ppapi::proxy::SerializedVar /* out_exception */,
                           ppapi::proxy::SerializedVar /* result */)
IPC_SYNC_MESSAGE_ROUTED2_2(PpapiHostMsg_PPBVar_IsInstanceOfDeprecated,
                           ppapi::proxy::SerializedVar /* var */,
                           int64 /* object_class */,
                           int64 /* object-data */,
                           PP_Bool /* result */)
IPC_SYNC_MESSAGE_ROUTED3_1(PpapiHostMsg_PPBVar_CreateObjectDeprecated,
                           PP_Instance /* instance */,
                           int64 /* object_class */,
                           int64 /* object_data */,
                           ppapi::proxy::SerializedVar /* result */)

#if !defined(OS_NACL) && !defined(NACL_WIN64)
// PPB_Broker.
IPC_SYNC_MESSAGE_ROUTED1_1(PpapiHostMsg_PPBBroker_Create,
                           PP_Instance /* instance */,
                           ppapi::HostResource /* result_resource */)
IPC_MESSAGE_ROUTED1(PpapiHostMsg_PPBBroker_Connect,
                    ppapi::HostResource /* broker */)

// PPB_Buffer.
IPC_SYNC_MESSAGE_ROUTED2_2(
    PpapiHostMsg_PPBBuffer_Create,
    PP_Instance /* instance */,
    uint32_t /* size */,
    ppapi::HostResource /* result_resource */,
    ppapi::proxy::SerializedHandle /* result_shm_handle */)

// PPB_ContentDecryptor_Dev messages handled in PPB_Instance_Proxy.
IPC_MESSAGE_ROUTED4(PpapiHostMsg_PPBInstance_NeedKey,
                    PP_Instance /* instance */,
                    ppapi::proxy::SerializedVar /* key_system, String */,
                    ppapi::proxy::SerializedVar /* session_id, String */,
                    ppapi::proxy::SerializedVar /* init_data, ArrayBuffer */)
IPC_MESSAGE_ROUTED3(PpapiHostMsg_PPBInstance_KeyAdded,
                    PP_Instance /* instance */,
                    ppapi::proxy::SerializedVar /* key_system, String */,
                    ppapi::proxy::SerializedVar /* session_id, String */)
IPC_MESSAGE_ROUTED5(PpapiHostMsg_PPBInstance_KeyMessage,
                    PP_Instance /* instance */,
                    ppapi::proxy::SerializedVar /* key_system, String */,
                    ppapi::proxy::SerializedVar /* session_id, String */,
                    ppapi::proxy::SerializedVar /* message, ArrayBuffer */,
                    ppapi::proxy::SerializedVar /* default_url, String */)
IPC_MESSAGE_ROUTED5(PpapiHostMsg_PPBInstance_KeyError,
                    PP_Instance /* instance */,
                    ppapi::proxy::SerializedVar /* key_system, String */,
                    ppapi::proxy::SerializedVar /* session_id, String */,
                    int32_t /* media_error */,
                    int32_t /* system_code */)
IPC_MESSAGE_ROUTED3(PpapiHostMsg_PPBInstance_DeliverBlock,
                    PP_Instance /* instance */,
                    PP_Resource /* decrypted_block, PPB_Buffer_Dev */,
                    std::string /* serialized_block_info */)
IPC_MESSAGE_ROUTED4(PpapiHostMsg_PPBInstance_DecoderInitializeDone,
                    PP_Instance /* instance */,
                    PP_DecryptorStreamType /* decoder_type */,
                    uint32_t /* request_id */,
                    PP_Bool /* success */)
IPC_MESSAGE_ROUTED3(PpapiHostMsg_PPBInstance_DecoderDeinitializeDone,
                    PP_Instance /* instance */,
                    PP_DecryptorStreamType /* decoder_type */,
                    uint32_t /* request_id */)
IPC_MESSAGE_ROUTED3(PpapiHostMsg_PPBInstance_DecoderResetDone,
                    PP_Instance /* instance */,
                    PP_DecryptorStreamType /* decoder_type */,
                    uint32_t /* request_id */)
IPC_MESSAGE_ROUTED3(PpapiHostMsg_PPBInstance_DeliverFrame,
                    PP_Instance /* instance */,
                    PP_Resource /* decrypted_frame, PPB_Buffer_Dev */,
                    std::string /* serialized_block_info */)
IPC_MESSAGE_ROUTED3(PpapiHostMsg_PPBInstance_DeliverSamples,
                    PP_Instance /* instance */,
                    PP_Resource /* audio_frames, PPB_Buffer_Dev */,
                    std::string /* serialized_block_info */)
#endif  // !defined(OS_NACL) && !defined(NACL_WIN64)

// PPB_NetworkMonitor_Private.
IPC_MESSAGE_CONTROL1(PpapiHostMsg_PPBNetworkMonitor_Start,
                     uint32 /* plugin_dispatcher_id */)
IPC_MESSAGE_CONTROL1(PpapiHostMsg_PPBNetworkMonitor_Stop,
                     uint32 /* plugin_dispatcher_id */)

#if !defined(OS_NACL) && !defined(NACL_WIN64)
// PPB_PDF
IPC_SYNC_MESSAGE_ROUTED3_1(
    PpapiHostMsg_PPBPDF_GetFontFileWithFallback,
    PP_Instance /* instance */,
    ppapi::proxy::SerializedFontDescription /* description */,
    int32_t /* charset */,
    ppapi::HostResource /* result */)
IPC_SYNC_MESSAGE_ROUTED2_1(
    PpapiHostMsg_PPBPDF_GetFontTableForPrivateFontFile,
    ppapi::HostResource /* font_file */,
    uint32_t /* table */,
    std::string /* result */)
#endif  // !defined(OS_NACL) && !defined(NACL_WIN64)

// PPB_Testing.
IPC_SYNC_MESSAGE_ROUTED3_1(
    PpapiHostMsg_PPBTesting_ReadImageData,
    ppapi::HostResource /* device_context_2d */,
    ppapi::HostResource /* image */,
    PP_Point /* top_left */,
    PP_Bool /* result */)
IPC_SYNC_MESSAGE_ROUTED1_1(PpapiHostMsg_PPBTesting_GetLiveObjectsForInstance,
                           PP_Instance /* instance */,
                           uint32 /* result */)
IPC_MESSAGE_ROUTED2(PpapiHostMsg_PPBTesting_SimulateInputEvent,
                    PP_Instance /* instance */,
                    ppapi::InputEventData /* input_event */)

#if !defined(OS_NACL) && !defined(NACL_WIN64)

// PPB_VideoDecoder.
IPC_SYNC_MESSAGE_ROUTED3_1(PpapiHostMsg_PPBVideoDecoder_Create,
                           PP_Instance /* instance */,
                           ppapi::HostResource /* context */,
                           PP_VideoDecoder_Profile /* profile */,
                           ppapi::HostResource /* result */)
IPC_MESSAGE_ROUTED4(PpapiHostMsg_PPBVideoDecoder_Decode,
                    ppapi::HostResource /* video_decoder */,
                    ppapi::HostResource /* bitstream buffer */,
                    int32 /* bitstream buffer id */,
                    uint32 /* size of buffer */)
IPC_MESSAGE_ROUTED2(PpapiHostMsg_PPBVideoDecoder_AssignPictureBuffers,
                    ppapi::HostResource /* video_decoder */,
                    std::vector<PP_PictureBuffer_Dev> /* picture buffers */)
IPC_MESSAGE_ROUTED2(PpapiHostMsg_PPBVideoDecoder_ReusePictureBuffer,
                    ppapi::HostResource /* video_decoder */,
                    int32_t /* picture buffer id */)
IPC_MESSAGE_ROUTED1(PpapiHostMsg_PPBVideoDecoder_Flush,
                    ppapi::HostResource /* video_decoder */)
IPC_MESSAGE_ROUTED1(PpapiHostMsg_PPBVideoDecoder_Reset,
                    ppapi::HostResource /* video_decoder */)
IPC_SYNC_MESSAGE_ROUTED1_0(PpapiHostMsg_PPBVideoDecoder_Destroy,
                           ppapi::HostResource /* video_decoder */)

// PPB_Flash_MessageLoop.
IPC_SYNC_MESSAGE_ROUTED1_1(PpapiHostMsg_PPBFlashMessageLoop_Create,
                           PP_Instance /* instance */,
                           ppapi::HostResource /* result */)
IPC_SYNC_MESSAGE_ROUTED1_1(PpapiHostMsg_PPBFlashMessageLoop_Run,
                           ppapi::HostResource /* flash_message_loop */,
                           int32_t /* result */)
IPC_SYNC_MESSAGE_ROUTED1_0(PpapiHostMsg_PPBFlashMessageLoop_Quit,
                           ppapi::HostResource /* flash_message_loop */)
#endif  // !defined(OS_NACL) && !defined(NACL_WIN64)

// PPB_TCPSocket_Private.
IPC_SYNC_MESSAGE_CONTROL2_1(PpapiHostMsg_PPBTCPSocket_Create,
                            int32 /* routing_id */,
                            uint32 /* plugin_dispatcher_id */,
                            uint32 /* socket_id */)
IPC_MESSAGE_CONTROL4(PpapiHostMsg_PPBTCPSocket_Connect,
                     int32 /* routing_id */,
                     uint32 /* socket_id */,
                     std::string /* host */,
                     uint16_t /* port */)
IPC_MESSAGE_CONTROL3(PpapiHostMsg_PPBTCPSocket_ConnectWithNetAddress,
                     int32 /* routing_id */,
                     uint32 /* socket_id */,
                     PP_NetAddress_Private /* net_addr */)
IPC_MESSAGE_CONTROL5(PpapiHostMsg_PPBTCPSocket_SSLHandshake,
                     uint32 /* socket_id */,
                     std::string /* server_name */,
                     uint16_t /* server_port */,
                     std::vector<std::vector<char> > /* trusted_certs */,
                     std::vector<std::vector<char> > /* untrusted_certs */)
IPC_MESSAGE_CONTROL2(PpapiHostMsg_PPBTCPSocket_Read,
                     uint32 /* socket_id */,
                     int32_t /* bytes_to_read */)
IPC_MESSAGE_CONTROL2(PpapiHostMsg_PPBTCPSocket_Write,
                     uint32 /* socket_id */,
                     std::string /* data */)
IPC_MESSAGE_CONTROL1(PpapiHostMsg_PPBTCPSocket_Disconnect,
                     uint32 /* socket_id */)
IPC_MESSAGE_CONTROL3(PpapiHostMsg_PPBTCPSocket_SetBoolOption,
                     uint32 /* socket_id */,
                     uint32 /* name */,
                     bool /* value */)

// UDPSocketPrivate.
IPC_MESSAGE_CONTROL0(PpapiHostMsg_UDPSocketPrivate_Create)
IPC_MESSAGE_CONTROL2(PpapiHostMsg_UDPSocketPrivate_SetBoolSocketFeature,
                     int32_t /* name */,
                     bool /* value */)
IPC_MESSAGE_CONTROL1(PpapiHostMsg_UDPSocketPrivate_Bind,
                     PP_NetAddress_Private /* net_addr */)
IPC_MESSAGE_CONTROL1(PpapiHostMsg_UDPSocketPrivate_RecvFrom,
                     int32_t /* num_bytes */)
IPC_MESSAGE_CONTROL2(PpapiHostMsg_UDPSocketPrivate_SendTo,
                     std::string /* data */,
                     PP_NetAddress_Private /* net_addr */)
IPC_MESSAGE_CONTROL0(PpapiHostMsg_UDPSocketPrivate_Close)
IPC_MESSAGE_CONTROL1(PpapiPluginMsg_UDPSocketPrivate_BindReply,
                     PP_NetAddress_Private /* bound_addr */)
IPC_MESSAGE_CONTROL2(PpapiPluginMsg_UDPSocketPrivate_RecvFromReply,
                     std::string /* data */,
                     PP_NetAddress_Private /* remote_addr */)
IPC_MESSAGE_CONTROL1(PpapiPluginMsg_UDPSocketPrivate_SendToReply,
                     int32_t /* bytes_written */)

// PPB_TCPServerSocket_Private.
IPC_MESSAGE_CONTROL5(PpapiHostMsg_PPBTCPServerSocket_Listen,
                     int32 /* routing_id */,
                     uint32 /* plugin_dispatcher_id */,
                     PP_Resource /* socket_resource */,
                     PP_NetAddress_Private /* addr */,
                     int32_t /* backlog */)
IPC_MESSAGE_CONTROL2(PpapiHostMsg_PPBTCPServerSocket_Accept,
                     int32 /* tcp_client_socket_routing_id */,
                     uint32 /* server_socket_id */)
IPC_MESSAGE_CONTROL1(PpapiHostMsg_PPBTCPServerSocket_Destroy,
                     uint32 /* socket_id */)

// PPB_X509Certificate_Private
IPC_SYNC_MESSAGE_CONTROL1_2(PpapiHostMsg_PPBX509Certificate_ParseDER,
                            std::vector<char> /* der */,
                            bool /* succeeded */,
                            ppapi::PPB_X509Certificate_Fields /* result */)

//-----------------------------------------------------------------------------
// Resource call/reply messages.
//
// These are the new-style resource implementations where the resource is only
// implemented in the proxy and "resource messages" are sent between this and a
// host object. Resource messages are a wrapper around some general routing
// information and a separate message of a type defined by the specific resource
// sending/receiving it. The extra paremeters allow the nested message to be
// routed automatically to the correct resource.

// Notification that a resource has been created in the plugin. The nested
// message will be resource-type-specific.
IPC_MESSAGE_CONTROL3(PpapiHostMsg_ResourceCreated,
                     ppapi::proxy::ResourceMessageCallParams /* call_params */,
                     PP_Instance  /* instance */,
                     IPC::Message /* nested_msg */)

// Notification that a resource has been destroyed in the plugin.
IPC_MESSAGE_CONTROL1(PpapiHostMsg_ResourceDestroyed,
                     PP_Resource /* resource */)

// Most resources are created by the plugin, which then sends a ResourceCreated
// message to create a corresponding ResourceHost in the renderer or browser
// host process. However, some resources are first created in the host and
// "pushed" or returned to the plugin.
//
// In this case, the host will create a "pending" ResourceHost object which
// is identified by an ID. The ID is sent to the plugin process and the
// PluginResource object is created. This message is sent from the plugin to
// the host process to connect the PluginResource and the pending ResourceHost
// (at which point, it's no longer pending).
IPC_MESSAGE_CONTROL2(PpapiHostMsg_AttachToPendingHost,
                     PP_Resource /* resource */,
                     int /* pending_host_id */)

// A resource call is a request from the plugin to the host. It may or may not
// require a reply, depending on the params. The nested message will be
// resource-type-specific.
IPC_MESSAGE_CONTROL2(PpapiHostMsg_ResourceCall,
                     ppapi::proxy::ResourceMessageCallParams /* call_params */,
                     IPC::Message /* nested_msg */)

// A resource reply is a response to a ResourceCall from a host to the
// plugin. The resource ID + sequence number in the params will correspond to
// that of the previous ResourceCall.
IPC_MESSAGE_CONTROL2(
    PpapiPluginMsg_ResourceReply,
    ppapi::proxy::ResourceMessageReplyParams /* reply_params */,
    IPC::Message /* nested_msg */)

IPC_SYNC_MESSAGE_CONTROL2_2(PpapiHostMsg_ResourceSyncCall,
    ppapi::proxy::ResourceMessageCallParams /* call_params */,
    IPC::Message /* nested_msg */,
    ppapi::proxy::ResourceMessageReplyParams /* reply_params */,
    IPC::Message /* reply_msg */)

//-----------------------------------------------------------------------------
// Messages for resources using call/reply above.

// Broker ----------------------------------------------------------------------
IPC_MESSAGE_CONTROL0(PpapiHostMsg_Broker_Create)

// Queries whether the plugin has permission to connect to the Pepper broker.
// The response is contained in the error value of the
// ResourceMessageReplyParams in the reply message.
IPC_MESSAGE_CONTROL0(PpapiHostMsg_Broker_IsAllowed)

// Directory reader.
IPC_MESSAGE_CONTROL0(PpapiHostMsg_DirectoryReader_Create)
IPC_MESSAGE_CONTROL1(PpapiHostMsg_DirectoryReader_GetEntries,
                     ppapi::HostResource /* file_ref_resource */)
IPC_MESSAGE_CONTROL2(PpapiPluginMsg_DirectoryReader_GetEntriesReply,
                     std::vector<ppapi::PPB_FileRef_CreateInfo> /* files */,
                     std::vector<PP_FileType> /* file_types */)

// File chooser.
IPC_MESSAGE_CONTROL0(PpapiHostMsg_FileChooser_Create)
IPC_MESSAGE_CONTROL4(PpapiHostMsg_FileChooser_Show,
                     bool /* save_as */,
                     bool /* open_multiple */,
                     std::string /* suggested_file_name */,
                     std::vector<std::string> /* accept_mime_types */)
IPC_MESSAGE_CONTROL1(PpapiPluginMsg_FileChooser_ShowReply,
                     std::vector<ppapi::PPB_FileRef_CreateInfo> /* files */)

// FileIO
IPC_MESSAGE_CONTROL0(PpapiHostMsg_FileIO_Create)
IPC_MESSAGE_CONTROL2(PpapiHostMsg_FileIO_Open,
                     PP_Resource /* file_ref_resource */,
                     int32_t /* open_flags */)
IPC_MESSAGE_CONTROL0(PpapiPluginMsg_FileIO_OpenReply)
IPC_MESSAGE_CONTROL0(PpapiHostMsg_FileIO_Close)
IPC_MESSAGE_CONTROL0(PpapiHostMsg_FileIO_Query)
IPC_MESSAGE_CONTROL1(PpapiPluginMsg_FileIO_QueryReply, PP_FileInfo /* info */)
IPC_MESSAGE_CONTROL2(PpapiHostMsg_FileIO_Touch,
                     PP_Time /* last_access_time */,
                     PP_Time /* last_modified_time */)
IPC_MESSAGE_CONTROL2(PpapiHostMsg_FileIO_Read,
                     int64_t /* offset */,
                     int32_t /* bytes_to_read */)
IPC_MESSAGE_CONTROL1(PpapiPluginMsg_FileIO_ReadReply, std::string /* data */)
IPC_MESSAGE_CONTROL2(PpapiHostMsg_FileIO_Write,
                     int64_t /* offset */,
                     std::string /* data */)
IPC_MESSAGE_CONTROL1(PpapiHostMsg_FileIO_SetLength,
                     int64_t /* length */)
IPC_MESSAGE_CONTROL0(PpapiHostMsg_FileIO_Flush)
IPC_MESSAGE_CONTROL2(PpapiHostMsg_FileIO_WillWrite,
                     int64_t /* offset */,
                     int32_t /* bytes_to_write */)
IPC_MESSAGE_CONTROL1(PpapiHostMsg_FileIO_WillSetLength,
                     int64_t /* length */)
IPC_MESSAGE_CONTROL0(PpapiHostMsg_FileIO_GetOSFileDescriptor)
IPC_MESSAGE_CONTROL1(PpapiPluginMsg_FileIO_GetOSFileDescriptorReply,
                     int32_t /* file descriptor */)
IPC_MESSAGE_CONTROL0(PpapiPluginMsg_FileIO_GeneralReply)

// Flash device ID.
IPC_MESSAGE_CONTROL0(PpapiHostMsg_FlashDeviceID_Create)
IPC_MESSAGE_CONTROL0(PpapiHostMsg_FlashDeviceID_GetDeviceID)
IPC_MESSAGE_CONTROL1(PpapiPluginMsg_FlashDeviceID_GetDeviceIDReply,
                     std::string /* id */)

// Gamepad.
IPC_MESSAGE_CONTROL0(PpapiHostMsg_Gamepad_Create)

// Requests that the gamepad host send the shared memory handle to the plugin
// process.
IPC_MESSAGE_CONTROL0(PpapiHostMsg_Gamepad_RequestMemory)

// Reply to a RequestMemory call. This supplies the shared memory handle. The
// actual handle is passed in the ReplyParams struct.
IPC_MESSAGE_CONTROL0(PpapiPluginMsg_Gamepad_SendMemory)


// Graphics2D, plugin -> host
IPC_MESSAGE_CONTROL2(PpapiHostMsg_Graphics2D_Create,
                     PP_Size /* size */,
                     PP_Bool /* is_always_opaque */)
IPC_MESSAGE_CONTROL4(PpapiHostMsg_Graphics2D_PaintImageData,
                     ppapi::HostResource /* image_data */,
                     PP_Point /* top_left */,
                     bool /* src_rect_specified */,
                     PP_Rect /* src_rect */)
IPC_MESSAGE_CONTROL3(PpapiHostMsg_Graphics2D_Scroll,
                     bool /* clip_specified */,
                     PP_Rect /* clip */,
                     PP_Point /* amount */)
IPC_MESSAGE_CONTROL1(PpapiHostMsg_Graphics2D_ReplaceContents,
                     ppapi::HostResource /* image_data */)
IPC_MESSAGE_CONTROL1(PpapiHostMsg_Graphics2D_Dev_SetScale,
                     float /* scale */)

// Graphics2D, plugin -> host -> plugin
IPC_MESSAGE_CONTROL0(PpapiHostMsg_Graphics2D_Flush)
IPC_MESSAGE_CONTROL0(PpapiPluginMsg_Graphics2D_FlushAck)

IPC_MESSAGE_CONTROL2(PpapiHostMsg_Graphics2D_ReadImageData,
                     PP_Resource /* image */,
                     PP_Point /* top_left */)
IPC_MESSAGE_CONTROL0(PpapiPluginMsg_Graphics2D_ReadImageDataAck)

// HostResolverPrivate, plugin -> host -> plugin
IPC_MESSAGE_CONTROL0(PpapiHostMsg_HostResolverPrivate_Create)
IPC_MESSAGE_CONTROL2(PpapiHostMsg_HostResolverPrivate_Resolve,
                     ppapi::HostPortPair /* host_port */,
                     PP_HostResolver_Private_Hint /* hint */)
IPC_MESSAGE_CONTROL2(PpapiPluginMsg_HostResolverPrivate_ResolveReply,
                     std::string /* canonical_name */,
                     std::vector<PP_NetAddress_Private> /* net_address_list */)

// Printing.
IPC_MESSAGE_CONTROL0(PpapiHostMsg_Printing_Create)
IPC_MESSAGE_CONTROL0(PpapiHostMsg_Printing_GetDefaultPrintSettings)
IPC_MESSAGE_CONTROL1(PpapiPluginMsg_Printing_GetDefaultPrintSettingsReply,
                     PP_PrintSettings_Dev /* print_settings */)

// WebSocket ------------------------------------------------------------------
IPC_MESSAGE_CONTROL0(PpapiHostMsg_WebSocket_Create)

// Establishes the connection to a server. This message requires
// WebSocket_ConnectReply as a reply message.
IPC_MESSAGE_CONTROL2(PpapiHostMsg_WebSocket_Connect,
                     std::string /* url */,
                     std::vector<std::string> /* protocols */)

// Closes established connection with graceful closing handshake. This message
// requires WebSocket_CloseReply as a reply message.
IPC_MESSAGE_CONTROL2(PpapiHostMsg_WebSocket_Close,
                     int32_t /* code */,
                     std::string /* reason */)

// Sends a text frame to the server. No reply is defined.
IPC_MESSAGE_CONTROL1(PpapiHostMsg_WebSocket_SendText,
                     std::string /* message */)

// Sends a binary frame to the server. No reply is defined.
IPC_MESSAGE_CONTROL1(PpapiHostMsg_WebSocket_SendBinary,
                     std::vector<uint8_t> /* message */)

// Fails the connection. This message invokes RFC6455 defined
// _Fail the WebSocket Connection_ operation. No reply is defined.
IPC_MESSAGE_CONTROL1(PpapiHostMsg_WebSocket_Fail,
                     std::string /* message */)

// This message is a reply to WebSocket_Connect. If the |url| and |protocols|
// are invalid, WebSocket_ConnectReply is issued immediately and it contains
// proper error code in its result. Otherwise, WebSocket_ConnectReply is sent
// with valid |url|, |protocol|, and result PP_OK. |protocol| is not a passed
// |protocols|, but a result of opening handshake negotiation. If the
// connection can not be established successfully, WebSocket_ConnectReply is
// not issued, but WebSocket_ClosedReply is sent instead.
IPC_MESSAGE_CONTROL2(PpapiPluginMsg_WebSocket_ConnectReply,
                     std::string /* url */,
                     std::string /* protocol */)

// This message is a reply to WebSocket_Close. If the operation fails,
// WebSocket_CloseReply is issued immediately and it contains PP_ERROR_FAILED.
// Otherwise, CloseReply will be issued after the closing handshake is
// finished. All arguments will be valid iff the result is PP_OK and it means
// that the client initiated closing handshake is finished gracefully.
IPC_MESSAGE_CONTROL4(PpapiPluginMsg_WebSocket_CloseReply,
                     uint64_t /* buffered_amount */,
                     bool /* was_clean */,
                     uint16_t /* code */,
                     std::string /* reason */)

// Unsolicited reply message to transmit a receiving text frame.
IPC_MESSAGE_CONTROL1(PpapiPluginMsg_WebSocket_ReceiveTextReply,
                     std::string /* message */)

// Unsolicited reply message to transmit a receiving binary frame.
IPC_MESSAGE_CONTROL1(PpapiPluginMsg_WebSocket_ReceiveBinaryReply,
                     std::vector<uint8_t> /* message */)

// Unsolicited reply message to notify a error on underlying network connetion.
IPC_MESSAGE_CONTROL0(PpapiPluginMsg_WebSocket_ErrorReply)

// Unsolicited reply message to update the buffered amount value.
IPC_MESSAGE_CONTROL1(PpapiPluginMsg_WebSocket_BufferedAmountReply,
                     uint64_t /* buffered_amount */)

// Unsolicited reply message to update |state| because of incoming external
// events, e.g., protocol error, or unexpected network closure.
IPC_MESSAGE_CONTROL1(PpapiPluginMsg_WebSocket_StateReply,
                     int32_t /* state */)

// Unsolicited reply message to notify that the connection is closed without
// any WebSocket_Close request. Server initiated closing handshake or
// unexpected network errors will invoke this message.
IPC_MESSAGE_CONTROL4(PpapiPluginMsg_WebSocket_ClosedReply,
                     uint64_t /* buffered_amount */,
                     bool /* was_clean */,
                     uint16_t /* code */,
                     std::string /* reason */)

#if !defined(OS_NACL) && !defined(NACL_WIN64)

// Audio input.
IPC_MESSAGE_CONTROL0(PpapiHostMsg_AudioInput_Create)
IPC_MESSAGE_CONTROL3(PpapiHostMsg_AudioInput_Open,
                     std::string /* device_id */,
                     PP_AudioSampleRate /* sample_rate */,
                     uint32_t /* sample_frame_count */)
// Reply to an Open call. This supplies a socket handle and a shared memory
// handle. Both handles are passed in the ReplyParams struct.
IPC_MESSAGE_CONTROL0(PpapiPluginMsg_AudioInput_OpenReply)
IPC_MESSAGE_CONTROL1(PpapiHostMsg_AudioInput_StartOrStop, bool /* capture */)
IPC_MESSAGE_CONTROL0(PpapiHostMsg_AudioInput_Close)

// BrowserFont -----------------------------------------------------------------

IPC_MESSAGE_CONTROL0(PpapiHostMsg_BrowserFontSingleton_Create)

// Requests that the browser reply with the list of font families via
// PpapiPluginMsg_BrowserFontSingleton_GetFontFamiliesReply.
IPC_MESSAGE_CONTROL0(PpapiHostMsg_BrowserFontSingleton_GetFontFamilies)

// Reply to PpapiHostMsg_BrowserFontSingleton_GetFontFamilies with the font
// family list. The |families| result is encoded by separating each family name
// by a null character.
IPC_MESSAGE_CONTROL1(PpapiPluginMsg_BrowserFontSingleton_GetFontFamiliesReply,
                     std::string /* families */)

// Flash -----------------------------------------------------------------------

IPC_MESSAGE_CONTROL0(PpapiHostMsg_Flash_Create)

// Message to notify the browser to register an update in system activity.
IPC_MESSAGE_CONTROL0(PpapiHostMsg_Flash_UpdateActivity)

// Query the browser for the proxy server to use for the given URL.
IPC_MESSAGE_CONTROL1(PpapiHostMsg_Flash_GetProxyForURL, std::string /* url */)
// Reply message for GetProxyForURL containing the proxy server.
IPC_MESSAGE_CONTROL1(PpapiPluginMsg_Flash_GetProxyForURLReply,
                     std::string /* proxy */)

// Queries the browser for the local time zone offset for a given time.
IPC_MESSAGE_CONTROL1(PpapiHostMsg_Flash_GetLocalTimeZoneOffset,
                     base::Time /* time */)
// Reply to GetLocalTimeZoneOffset containing the time zone offset as a double.
IPC_MESSAGE_CONTROL1(PpapiPluginMsg_Flash_GetLocalTimeZoneOffsetReply,
                     double /* offset */)

// Query the browser for the restrictions on storing Flash LSOs.
IPC_MESSAGE_CONTROL0(PpapiHostMsg_Flash_GetLocalDataRestrictions)
// Reply message for GetLocalDataRestrictions containing the restrictions to
// use. These are PP_FlashLSORestrictions cast to an int32_t.
IPC_MESSAGE_CONTROL1(PpapiPluginMsg_Flash_GetLocalDataRestrictionsReply,
                     int32_t /* restrictions */)

// Notifies the renderer whether the Flash instance is in windowed mode. No
// reply is sent.
IPC_MESSAGE_CONTROL1(PpapiHostMsg_Flash_SetInstanceAlwaysOnTop,
                     bool /* on_top */)

// Notifies the renderer to draw text to the given PP_ImageData resource. All
// parmeters for drawing (including the resource to draw to) are contianed in
// the PPBFlash_DrawGlyphs_Params structure. An error code is sent in a reply
// message indicating success.
IPC_MESSAGE_CONTROL1(PpapiHostMsg_Flash_DrawGlyphs,
                     ppapi::proxy::PPBFlash_DrawGlyphs_Params /* params */)

// Notifies the renderer to navigate to the given URL contained in the
// URLRequestInfoData. An error code is sent in a reply message indicating
// success.
IPC_MESSAGE_CONTROL3(PpapiHostMsg_Flash_Navigate,
                     ppapi::URLRequestInfoData /* data */,
                     std::string /* target */,
                     bool /* from_user_action */)

// Queries the renderer on whether the plugin instance is the topmost element
// in the area of the instance specified by the given PP_Rect. PP_OK is sent as
// the error code in a reply message if the rect is topmost otherwise
// PP_ERROR_FAILED is sent.
IPC_MESSAGE_CONTROL1(PpapiHostMsg_Flash_IsRectTopmost,
                     PP_Rect /* rect */)

// Notifies the renderer to invoke printing for the given plugin instance. No
// reply is sent.
IPC_MESSAGE_CONTROL0(PpapiHostMsg_Flash_InvokePrinting)

// DeviceEnumeration -----------------------------------------------------------
// Device enumeration messages used by audio input and video capture.
IPC_MESSAGE_CONTROL0(PpapiHostMsg_DeviceEnumeration_EnumerateDevices)
IPC_MESSAGE_CONTROL1(PpapiPluginMsg_DeviceEnumeration_EnumerateDevicesReply,
                     std::vector<ppapi::DeviceRefData> /* devices */)
IPC_MESSAGE_CONTROL1(PpapiHostMsg_DeviceEnumeration_MonitorDeviceChange,
                     uint32_t /* callback_id */)
IPC_MESSAGE_CONTROL0(PpapiHostMsg_DeviceEnumeration_StopMonitoringDeviceChange)
IPC_MESSAGE_CONTROL2(PpapiPluginMsg_DeviceEnumeration_NotifyDeviceChange,
                     uint32_t /* callback_id */,
                     std::vector<ppapi::DeviceRefData> /* devices */)

// Flash clipboard.
IPC_MESSAGE_CONTROL0(PpapiHostMsg_FlashClipboard_Create)
IPC_MESSAGE_CONTROL1(PpapiHostMsg_FlashClipboard_RegisterCustomFormat,
                     std::string /* format_name */)
IPC_MESSAGE_CONTROL1(PpapiPluginMsg_FlashClipboard_RegisterCustomFormatReply,
                     uint32_t /* format */)
IPC_MESSAGE_CONTROL2(PpapiHostMsg_FlashClipboard_IsFormatAvailable,
                     uint32_t /* clipboard_type */,
                     uint32_t /* format */)
IPC_MESSAGE_CONTROL2(PpapiHostMsg_FlashClipboard_ReadData,
                     uint32_t /* clipboard_type */,
                     uint32_t /* format */)
IPC_MESSAGE_CONTROL1(PpapiPluginMsg_FlashClipboard_ReadDataReply,
                     std::string /* result */)
IPC_MESSAGE_CONTROL3(PpapiHostMsg_FlashClipboard_WriteData,
                     uint32_t /* clipboard_type */,
                     std::vector<uint32_t> /* formats */,
                     std::vector<std::string> /* data */)

// Flash file.
IPC_MESSAGE_CONTROL0(PpapiHostMsg_FlashFile_Create)
IPC_MESSAGE_CONTROL2(PpapiHostMsg_FlashFile_OpenFile,
                     ppapi::PepperFilePath /* path */,
                     int /* flags */)
IPC_MESSAGE_CONTROL2(PpapiHostMsg_FlashFile_RenameFile,
                     ppapi::PepperFilePath /* from_path */,
                     ppapi::PepperFilePath /* to_path */)
IPC_MESSAGE_CONTROL2(PpapiHostMsg_FlashFile_DeleteFileOrDir,
                     ppapi::PepperFilePath /* path */,
                     bool /* recursive */)
IPC_MESSAGE_CONTROL1(PpapiHostMsg_FlashFile_CreateDir,
                     ppapi::PepperFilePath /* path */)
IPC_MESSAGE_CONTROL1(PpapiHostMsg_FlashFile_QueryFile,
                     ppapi::PepperFilePath /* path */)
IPC_MESSAGE_CONTROL1(PpapiPluginMsg_FlashFile_QueryFileReply,
                     base::PlatformFileInfo /* file_info */)
IPC_MESSAGE_CONTROL1(PpapiHostMsg_FlashFile_GetDirContents,
                     ppapi::PepperFilePath /* path */)
IPC_MESSAGE_CONTROL1(PpapiPluginMsg_FlashFile_GetDirContentsReply,
                     ppapi::DirContents /* entries */)
IPC_MESSAGE_CONTROL0(PpapiHostMsg_FlashFile_CreateTemporaryFile)

// Flash font file.
IPC_MESSAGE_CONTROL2(PpapiHostMsg_FlashFontFile_Create,
                     ppapi::proxy::SerializedFontDescription /* description */,
                     PP_PrivateFontCharset /* charset */)
IPC_MESSAGE_CONTROL1(PpapiHostMsg_FlashFontFile_GetFontTable,
                     uint32_t /* table */)
IPC_MESSAGE_CONTROL1(PpapiPluginMsg_FlashFontFile_GetFontTableReply,
                     std::string /* output */)

// Flash fullscreen.
IPC_MESSAGE_CONTROL0(PpapiHostMsg_FlashFullscreen_Create)
IPC_MESSAGE_CONTROL1(PpapiHostMsg_FlashFullscreen_SetFullscreen,
                     bool /* fullscreen */)

// FlashMenu ------------------------------------------------------------------

// Creates the flash menu with the given data.
IPC_MESSAGE_CONTROL1(PpapiHostMsg_FlashMenu_Create,
                     ppapi::proxy::SerializedFlashMenu /* menu_data */)

// Shows the menu at the given location relative to the plugin instance.
IPC_MESSAGE_CONTROL1(PpapiHostMsg_FlashMenu_Show,
                     PP_Point /* location */)

// Reply to a show command. If the resource reply is PP_OK, the selected_id
// will be the menu item ID chosen by the user.
IPC_MESSAGE_CONTROL1(PpapiPluginMsg_FlashMenu_ShowReply,
                     int32_t /* selected_id */)

// VideoCapture_Dev, plugin -> host
IPC_MESSAGE_CONTROL0(PpapiHostMsg_VideoCapture_Create)
IPC_MESSAGE_CONTROL0(PpapiHostMsg_VideoCapture_StartCapture)
IPC_MESSAGE_CONTROL1(PpapiHostMsg_VideoCapture_ReuseBuffer,
                     uint32_t /* buffer */)
IPC_MESSAGE_CONTROL0(PpapiHostMsg_VideoCapture_StopCapture)
IPC_MESSAGE_CONTROL0(PpapiHostMsg_VideoCapture_Close)

// VideoCapture_Dev, plugin -> host -> plugin
IPC_MESSAGE_CONTROL3(PpapiHostMsg_VideoCapture_Open,
                     std::string /* device_id */,
                     PP_VideoCaptureDeviceInfo_Dev /* requested_info */,
                     uint32_t /* buffer_count */)
IPC_MESSAGE_CONTROL0(PpapiPluginMsg_VideoCapture_OpenReply)

// Talk ------------------------------------------------------------------------

IPC_MESSAGE_CONTROL0(PpapiHostMsg_Talk_Create)

// Requests talk permissions. The host will respond with GetPermissionReply.
IPC_MESSAGE_CONTROL0(PpapiHostMsg_Talk_GetPermission)

// Response to GetPermission.
//
// The result of this message is the general Pepper "result" in the ReplyParams.
IPC_MESSAGE_CONTROL0(PpapiPluginMsg_Talk_GetPermissionReply)

// VideoCapture_Dev, host -> plugin
IPC_MESSAGE_CONTROL3(PpapiPluginMsg_VideoCapture_OnDeviceInfo,
                     PP_VideoCaptureDeviceInfo_Dev /* info */,
                     std::vector<ppapi::HostResource> /* buffers */,
                     uint32_t /* buffer_size */)
IPC_MESSAGE_CONTROL1(PpapiPluginMsg_VideoCapture_OnStatus,
                     uint32_t /* status */)
IPC_MESSAGE_CONTROL1(PpapiPluginMsg_VideoCapture_OnError,
                     uint32_t /* error */)
IPC_MESSAGE_CONTROL1(PpapiPluginMsg_VideoCapture_OnBufferReady,
                     uint32_t /* buffer */)

#endif  // !defined(OS_NACL) && !defined(NACL_WIN64)
