// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Multiply-included message header, no traditional include guard.
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/file_path.h"
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
#include "ppapi/c/dev/ppb_text_input_dev.h"
#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_file_info.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_module.h"
#include "ppapi/c/pp_point.h"
#include "ppapi/c/pp_rect.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/pp_size.h"
#include "ppapi/c/dev/pp_video_dev.h"
#include "ppapi/c/private/ppb_tcp_socket_private.h"
#include "ppapi/proxy/ppapi_param_traits.h"
#include "ppapi/proxy/ppapi_proxy_export.h"
#include "ppapi/proxy/serialized_flash_menu.h"
#include "ppapi/proxy/serialized_structs.h"
#include "ppapi/shared_impl/input_event_impl.h"
#include "ppapi/shared_impl/ppapi_preferences.h"
#include "ppapi/shared_impl/url_request_info_impl.h"

#undef IPC_MESSAGE_EXPORT
#define IPC_MESSAGE_EXPORT PPAPI_PROXY_EXPORT

#define IPC_MESSAGE_START PpapiMsgStart

IPC_ENUM_TRAITS(PP_InputEvent_Type)
IPC_ENUM_TRAITS(PP_InputEvent_MouseButton)
IPC_ENUM_TRAITS(PP_TextInput_Type)
IPC_ENUM_TRAITS(PP_VideoDecoder_Profile)
IPC_ENUM_TRAITS(PP_VideoDecodeError_Dev)

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

IPC_STRUCT_TRAITS_BEGIN(PP_VideoCaptureDeviceInfo_Dev)
  IPC_STRUCT_TRAITS_MEMBER(width)
  IPC_STRUCT_TRAITS_MEMBER(height)
  IPC_STRUCT_TRAITS_MEMBER(frames_per_second)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(ppapi::proxy::PPPVideoCapture_Buffer)
  IPC_STRUCT_TRAITS_MEMBER(resource)
  IPC_STRUCT_TRAITS_MEMBER(handle)
  IPC_STRUCT_TRAITS_MEMBER(size)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(ppapi::Preferences)
  IPC_STRUCT_TRAITS_MEMBER(standard_font_family)
  IPC_STRUCT_TRAITS_MEMBER(fixed_font_family)
  IPC_STRUCT_TRAITS_MEMBER(serif_font_family)
  IPC_STRUCT_TRAITS_MEMBER(sans_serif_font_family)
  IPC_STRUCT_TRAITS_MEMBER(default_font_size)
  IPC_STRUCT_TRAITS_MEMBER(default_fixed_font_size)
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
  IPC_STRUCT_TRAITS_MEMBER(character_text)
  IPC_STRUCT_TRAITS_MEMBER(composition_segment_offsets)
  IPC_STRUCT_TRAITS_MEMBER(composition_target_segment)
  IPC_STRUCT_TRAITS_MEMBER(composition_selection_start)
  IPC_STRUCT_TRAITS_MEMBER(composition_selection_end)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(ppapi::PPB_URLRequestInfo_Data)
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
  IPC_STRUCT_TRAITS_MEMBER(body)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(ppapi::PPB_URLRequestInfo_Data::BodyItem)
  IPC_STRUCT_TRAITS_MEMBER(is_file)
  IPC_STRUCT_TRAITS_MEMBER(data)
  // Note: we don't serialize file_ref.
  IPC_STRUCT_TRAITS_MEMBER(file_ref_host_resource)
  IPC_STRUCT_TRAITS_MEMBER(start_offset)
  IPC_STRUCT_TRAITS_MEMBER(number_of_bytes)
  IPC_STRUCT_TRAITS_MEMBER(expected_last_modified_time)
IPC_STRUCT_TRAITS_END()

// These are from the browser to the plugin.
// Loads the given plugin.
IPC_MESSAGE_CONTROL1(PpapiMsg_LoadPlugin, FilePath /* path */)

// Creates a channel to talk to a renderer. The plugin will respond with
// PpapiHostMsg_ChannelCreated.
IPC_MESSAGE_CONTROL2(PpapiMsg_CreateChannel,
                     base::ProcessHandle /* host_process_handle */,
                     int /* renderer_id */)

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

// Network state notification from the browser for implementing
// PPP_NetworkState_Dev.
IPC_MESSAGE_CONTROL1(PpapiMsg_SetNetworkState,
                     bool /* online */)

// Sent in both directions to see if the other side supports the given
// interface.
IPC_SYNC_MESSAGE_CONTROL1_1(PpapiMsg_SupportsInterface,
                            std::string /* interface_name */,
                            bool /* result */)

// Broker Process.

IPC_SYNC_MESSAGE_CONTROL2_1(PpapiMsg_ConnectToPlugin,
                            PP_Instance /* instance */,
                            IPC::PlatformFileForTransit /* handle */,
                            int32_t /* result */)

// PPB_Audio.

// Notifies the result of the audio stream create call. This is called in
// both error cases and in the normal success case. These cases are
// differentiated by the result code, which is one of the standard PPAPI
// result codes.
//
// The handler of this message should always close all of the handles passed
// in, since some could be valid even in the error case.
IPC_MESSAGE_ROUTED5(PpapiMsg_PPBAudio_NotifyAudioStreamCreated,
                    ppapi::HostResource /* audio_id */,
                    int32_t /* result_code (will be != PP_OK on failure) */,
                    IPC::PlatformFileForTransit /* socket_handle */,
                    base::SharedMemoryHandle /* handle */,
                    int32_t /* length */)

// PPB_Broker.
IPC_MESSAGE_ROUTED3(
    PpapiMsg_PPBBroker_ConnectComplete,
    ppapi::HostResource /* broker */,
    IPC::PlatformFileForTransit /* handle */,
    int32_t /* result */)

// PPB_FileChooser.
IPC_MESSAGE_ROUTED3(
    PpapiMsg_PPBFileChooser_ChooseComplete,
    ppapi::HostResource /* chooser */,
    int32_t /* result_code (will be != PP_OK on failure */,
    std::vector<ppapi::PPB_FileRef_CreateInfo> /* chosen_files */)

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

// PPB_Flash_NetConnector.
IPC_MESSAGE_ROUTED5(PpapiMsg_PPBFlashNetConnector_ConnectACK,
                    ppapi::HostResource /* net_connector */,
                    int32_t /* result */,
                    IPC::PlatformFileForTransit /* handle */,
                    std::string /* local_addr_as_string */,
                    std::string /* remote_addr_as_string */)

// PPB_TCPSocket_Private.
IPC_MESSAGE_ROUTED5(PpapiMsg_PPBTCPSocket_ConnectACK,
                    uint32 /* plugin_dispatcher_id */,
                    uint32 /* socket_id */,
                    bool /* succeeded */,
                    PP_NetAddress_Private /* local_addr */,
                    PP_NetAddress_Private /* remote_addr */)
IPC_MESSAGE_ROUTED3(PpapiMsg_PPBTCPSocket_SSLHandshakeACK,
                    uint32 /* plugin_dispatcher_id */,
                    uint32 /* socket_id */,
                    bool /* succeeded */)
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

// PPB_UDPSocket_Private
IPC_MESSAGE_ROUTED3(PpapiMsg_PPBUDPSocket_BindACK,
                    uint32 /* plugin_dispatcher_id */,
                    uint32 /* socket_id */,
                    bool /* succeeded */)
IPC_MESSAGE_ROUTED5(PpapiMsg_PPBUDPSocket_RecvFromACK,
                    uint32 /* plugin_dispatcher_id */,
                    uint32 /* socket_id */,
                    bool /* succeeded */,
                    std::string /* data */,
                    PP_NetAddress_Private /* remote_addr */)
IPC_MESSAGE_ROUTED4(PpapiMsg_PPBUDPSocket_SendToACK,
                    uint32 /* plugin_dispatcher_id */,
                    uint32 /* socket_id */,
                    bool /* succeeded */,
                    int32_t /* bytes_written */)

// PPB_Graphics2D.
IPC_MESSAGE_ROUTED2(PpapiMsg_PPBGraphics2D_FlushACK,
                    ppapi::HostResource /* graphics_2d */,
                    int32_t /* pp_error */)

// PPB_Graphics3D.
IPC_MESSAGE_ROUTED2(PpapiMsg_PPBGraphics3D_SwapBuffersACK,
                    ppapi::HostResource /* graphics_3d */,
                    int32_t /* pp_error */)

// PPB_Instance.
IPC_MESSAGE_ROUTED2(PpapiMsg_PPBInstance_MouseLockComplete,
                    PP_Instance /* instance */,
                    int32_t /* result */)

// PPB_Surface3D.
IPC_MESSAGE_ROUTED2(PpapiMsg_PPBSurface3D_SwapBuffersACK,
                    ppapi::HostResource /* surface_3d */,
                    int32_t /* pp_error */)

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
IPC_MESSAGE_ROUTED5(PpapiMsg_PPPInstance_DidChangeView,
                    PP_Instance /* instance */,
                    PP_Rect /* position */,
                    PP_Rect /* clip */,
                    PP_Bool /* fullscreen */,
                    PP_Bool /* flash_fullscreen */)
IPC_MESSAGE_ROUTED2(PpapiMsg_PPPInstance_DidChangeFocus,
                    PP_Instance /* instance */,
                    PP_Bool /* has_focus */)
IPC_SYNC_MESSAGE_ROUTED2_1(PpapiMsg_PPPInstance_HandleDocumentLoad,
                           PP_Instance /* instance */,
                           ppapi::HostResource /* url_loader */,
                           PP_Bool /* result */)

// PPP_Instance_Private.
IPC_SYNC_MESSAGE_ROUTED1_1(PpapiMsg_PPPInstancePrivate_GetInstanceObject,
                           PP_Instance /* instance */,
                           ppapi::proxy::SerializedVar /* result */)

// PPP_Messaging.
IPC_MESSAGE_ROUTED2(PpapiMsg_PPPMessaging_HandleMessage,
                    PP_Instance /* instance */,
                    ppapi::proxy::SerializedVar /* message */)

// PPP_MouseLock.
IPC_MESSAGE_ROUTED1(PpapiMsg_PPPMouseLock_MouseLockLost,
                    PP_Instance /* instance */)

// PPB_URLLoader
// (Messages from browser to plugin to notify it of changes in state.)
IPC_MESSAGE_ROUTED1(
    PpapiMsg_PPBURLLoader_UpdateProgress,
    ppapi::proxy::PPBURLLoader_UpdateProgress_Params /* params */)
IPC_MESSAGE_ROUTED3(PpapiMsg_PPBURLLoader_ReadResponseBody_Ack,
                    ppapi::HostResource /* loader */,
                    int32 /* result */,
                    std::string /* data */)
IPC_MESSAGE_ROUTED2(PpapiMsg_PPBURLLoader_CallbackComplete,
                    ppapi::HostResource /* loader */,
                    int32_t /* result */)

// PPP_VideoCapture_Dev
IPC_MESSAGE_ROUTED3(
    PpapiMsg_PPPVideoCapture_OnDeviceInfo,
    ppapi::HostResource /* video_capture */,
    PP_VideoCaptureDeviceInfo_Dev /* info */,
    std::vector<ppapi::proxy::PPPVideoCapture_Buffer> /* buffers */)
IPC_MESSAGE_ROUTED2(PpapiMsg_PPPVideoCapture_OnStatus,
                    ppapi::HostResource /* video_capture */,
                    uint32_t /* status */)
IPC_MESSAGE_ROUTED2(PpapiMsg_PPPVideoCapture_OnError,
                    ppapi::HostResource /* video_capture */,
                    uint32_t /* error_code */)
IPC_MESSAGE_ROUTED2(PpapiMsg_PPPVideoCapture_OnBufferReady,
                    ppapi::HostResource /* video_capture */,
                    uint32_t /* buffer */)

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
IPC_MESSAGE_ROUTED3(PpapiMsg_PPPVideoDecoder_ProvidePictureBuffers,
                    ppapi::HostResource /* video_decoder */,
                    uint32_t /* requested number of buffers */,
                    PP_Size /* dimensions of buffers */)
IPC_MESSAGE_ROUTED2(PpapiMsg_PPPVideoDecoder_DismissPictureBuffer,
                    ppapi::HostResource /* video_decoder */,
                    int32_t /* picture buffer id */)
IPC_MESSAGE_ROUTED2(PpapiMsg_PPPVideoDecoder_PictureReady,
                    ppapi::HostResource /* video_decoder */,
                    PP_Picture_Dev /* output picture */)
IPC_MESSAGE_ROUTED1(PpapiMsg_PPPVideoDecoder_NotifyEndOfStream,
                    ppapi::HostResource /* video_decoder */)
IPC_MESSAGE_ROUTED2(PpapiMsg_PPPVideoDecoder_NotifyError,
                    ppapi::HostResource /* video_decoder */,
                    PP_VideoDecodeError_Dev /* error */)

// -----------------------------------------------------------------------------
// These are from the plugin to the renderer.

// Reply to PpapiMsg_CreateChannel. The handle will be NULL if the channel
// could not be established. This could be because the IPC could not be created
// for some weird reason, but more likely that the plugin failed to load or
// initialize properly.
IPC_MESSAGE_CONTROL1(PpapiHostMsg_ChannelCreated,
                     IPC::ChannelHandle /* handle */)

// PPB_Audio.
IPC_SYNC_MESSAGE_ROUTED3_1(PpapiHostMsg_PPBAudio_Create,
                           PP_Instance /* instance_id */,
                           int32_t /* sample_rate */,
                           uint32_t /* sample_frame_count */,
                           ppapi::HostResource /* result */)
IPC_MESSAGE_ROUTED2(PpapiHostMsg_PPBAudio_StartOrStop,
                    ppapi::HostResource /* audio_id */,
                    bool /* play */)

// PPB_Broker.
IPC_SYNC_MESSAGE_ROUTED1_1(PpapiHostMsg_PPBBroker_Create,
                           PP_Instance /* instance */,
                           ppapi::HostResource /* result_resource */)
IPC_MESSAGE_ROUTED1(PpapiHostMsg_PPBBroker_Connect,
                    ppapi::HostResource /* broker */)

// PPB_Buffer.
IPC_SYNC_MESSAGE_ROUTED2_2(PpapiHostMsg_PPBBuffer_Create,
                           PP_Instance /* instance */,
                           uint32_t /* size */,
                           ppapi::HostResource /* result_resource */,
                           base::SharedMemoryHandle /* result_shm_handle */)

// PPB_Context3D.
IPC_SYNC_MESSAGE_ROUTED3_1(PpapiHostMsg_PPBContext3D_Create,
                           PP_Instance /* instance */,
                           int32_t /* config */,
                           std::vector<int32_t> /* attrib_list */,
                           ppapi::HostResource /* result */)

IPC_SYNC_MESSAGE_ROUTED3_1(PpapiHostMsg_PPBContext3D_BindSurfaces,
                           ppapi::HostResource /* context */,
                           ppapi::HostResource /* draw */,
                           ppapi::HostResource /* read */,
                           int32_t /* result */)

IPC_SYNC_MESSAGE_ROUTED2_1(PpapiHostMsg_PPBContext3D_Initialize,
                           ppapi::HostResource /* context */,
                           int32 /* size */,
                           base::SharedMemoryHandle /* ring_buffer */)

IPC_SYNC_MESSAGE_ROUTED1_1(PpapiHostMsg_PPBContext3D_GetState,
                           ppapi::HostResource /* context */,
                           gpu::CommandBuffer::State /* state */)

IPC_SYNC_MESSAGE_ROUTED3_1(PpapiHostMsg_PPBContext3D_Flush,
                           ppapi::HostResource /* context */,
                           int32 /* put_offset */,
                           int32 /* last_known_get */,
                           gpu::CommandBuffer::State /* state */)

IPC_MESSAGE_ROUTED2(PpapiHostMsg_PPBContext3D_AsyncFlush,
                    ppapi::HostResource /* context */,
                    int32 /* put_offset */)

IPC_SYNC_MESSAGE_ROUTED2_1(PpapiHostMsg_PPBContext3D_CreateTransferBuffer,
                           ppapi::HostResource /* context */,
                           int32 /* size */,
                           int32 /* id */)

IPC_SYNC_MESSAGE_ROUTED2_0(PpapiHostMsg_PPBContext3D_DestroyTransferBuffer,
                           ppapi::HostResource /* context */,
                           int32 /* id */)

IPC_SYNC_MESSAGE_ROUTED2_2(PpapiHostMsg_PPBContext3D_GetTransferBuffer,
                           ppapi::HostResource /* context */,
                           int32 /* id */,
                           base::SharedMemoryHandle /* transfer_buffer */,
                           uint32 /* size */)

// PPB_Core.
IPC_MESSAGE_ROUTED1(PpapiHostMsg_PPBCore_AddRefResource,
                    ppapi::HostResource)
IPC_MESSAGE_ROUTED1(PpapiHostMsg_PPBCore_ReleaseResource,
                    ppapi::HostResource)

// PPB_CursorControl.
IPC_SYNC_MESSAGE_ROUTED4_1(PpapiHostMsg_PPBCursorControl_SetCursor,
                           PP_Instance /* instance */,
                           int32_t /* type */,
                           ppapi::HostResource /* custom_image */,
                           PP_Point /* hot_spot */,
                           PP_Bool /* result */)
IPC_SYNC_MESSAGE_ROUTED1_1(PpapiHostMsg_PPBCursorControl_LockCursor,
                           PP_Instance /* instance */,
                           PP_Bool /* result */)
IPC_SYNC_MESSAGE_ROUTED1_1(PpapiHostMsg_PPBCursorControl_UnlockCursor,
                           PP_Instance /* instance */,
                           PP_Bool /* result */)
IPC_SYNC_MESSAGE_ROUTED1_1(PpapiHostMsg_PPBCursorControl_HasCursorLock,
                           PP_Instance /* instance */,
                           PP_Bool /* result */)
IPC_SYNC_MESSAGE_ROUTED1_1(PpapiHostMsg_PPBCursorControl_CanLockCursor,
                           PP_Instance /* instance */,
                           PP_Bool /* result */)

// PPB_FileChooser.
IPC_SYNC_MESSAGE_ROUTED3_1(PpapiHostMsg_PPBFileChooser_Create,
                           PP_Instance /* instance */,
                           int /* mode */,
                           std::string /* accept_mime_types */,
                           ppapi::HostResource /* result */)
IPC_MESSAGE_ROUTED4(PpapiHostMsg_PPBFileChooser_Show,
                    ppapi::HostResource /* file_chooser */,
                    bool /* save_as */,
                    std::string /* suggested_file_name */,
                    bool /* require_user_gesture */)

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

// PPB_FileSystem.
IPC_SYNC_MESSAGE_ROUTED2_1(PpapiHostMsg_PPBFileSystem_Create,
                           PP_Instance /* instance */,
                           int /* type */,
                           ppapi::HostResource /* result */)
IPC_MESSAGE_ROUTED2(PpapiHostMsg_PPBFileSystem_Open,
                    ppapi::HostResource /* result */,
                    int64_t /* expected_size */)

// PPB_Flash.
IPC_MESSAGE_ROUTED2(PpapiHostMsg_PPBFlash_SetInstanceAlwaysOnTop,
                    PP_Instance /* instance */,
                    PP_Bool /* on_top */)
// This has to be synchronous becuase the caller may want to composite on
// top of the resulting text after the call is complete.
IPC_SYNC_MESSAGE_ROUTED1_1(
    PpapiHostMsg_PPBFlash_DrawGlyphs,
    ppapi::proxy::PPBFlash_DrawGlyphs_Params /* params */,
    PP_Bool /* result */)
IPC_SYNC_MESSAGE_ROUTED2_1(PpapiHostMsg_PPBFlash_GetProxyForURL,
                           PP_Instance /* instance */,
                           std::string /* url */,
                           ppapi::proxy::SerializedVar /* result */)
IPC_SYNC_MESSAGE_ROUTED4_1(PpapiHostMsg_PPBFlash_Navigate,
                           PP_Instance /* instance */,
                           ppapi::PPB_URLRequestInfo_Data /* request_data */,
                           std::string /* target */,
                           bool /* from_user_action */,
                           int32_t /* result */)
IPC_SYNC_MESSAGE_ROUTED1_0(PpapiHostMsg_PPBFlash_RunMessageLoop,
                           PP_Instance /* instance */)
IPC_SYNC_MESSAGE_ROUTED1_0(PpapiHostMsg_PPBFlash_QuitMessageLoop,
                           PP_Instance /* instance */)
IPC_SYNC_MESSAGE_ROUTED2_1(PpapiHostMsg_PPBFlash_GetLocalTimeZoneOffset,
                           PP_Instance /* instance */,
                           PP_Time /* t */,
                           double /* offset */)

// PPB_Flash_Clipboard.
IPC_SYNC_MESSAGE_ROUTED3_1(PpapiHostMsg_PPBFlashClipboard_IsFormatAvailable,
                           PP_Instance /* instance */,
                           int /* clipboard_type */,
                           int /* format */,
                           bool /* result */)
IPC_SYNC_MESSAGE_ROUTED2_1(PpapiHostMsg_PPBFlashClipboard_ReadPlainText,
                           PP_Instance /* instance */,
                           int /* clipboard_type */,
                           ppapi::proxy::SerializedVar /* result */)
IPC_MESSAGE_ROUTED3(PpapiHostMsg_PPBFlashClipboard_WritePlainText,
                    PP_Instance /* instance */,
                    int /* clipboard_type */,
                    ppapi::proxy::SerializedVar /* text */)

// PPB_Flash_File_FileRef.
IPC_SYNC_MESSAGE_ROUTED2_2(PpapiHostMsg_PPBFlashFile_FileRef_OpenFile,
                           ppapi::HostResource /* file_ref */,
                           int32_t /* mode */,
                           IPC::PlatformFileForTransit /* file_handle */,
                           int32_t /* result */)
IPC_SYNC_MESSAGE_ROUTED1_2(PpapiHostMsg_PPBFlashFile_FileRef_QueryFile,
                           ppapi::HostResource /* file_ref */,
                           PP_FileInfo /* info */,
                           int32_t /* result */)

// PPB_Flash_File_ModuleLocal.
IPC_SYNC_MESSAGE_ROUTED3_2(PpapiHostMsg_PPBFlashFile_ModuleLocal_OpenFile,
                           PP_Instance /* instance */,
                           std::string /* path */,
                           int32_t /* mode */,
                           IPC::PlatformFileForTransit /* file_handle */,
                           int32_t /* result */)
IPC_SYNC_MESSAGE_ROUTED3_1(PpapiHostMsg_PPBFlashFile_ModuleLocal_RenameFile,
                           PP_Instance /* instance */,
                           std::string /* path_from */,
                           std::string /* path_to */,
                           int32_t /* result */)
IPC_SYNC_MESSAGE_ROUTED3_1(
    PpapiHostMsg_PPBFlashFile_ModuleLocal_DeleteFileOrDir,
    PP_Instance /* instance */,
    std::string /* path */,
    PP_Bool /* recursive */,
    int32_t /* result */)
IPC_SYNC_MESSAGE_ROUTED2_1(PpapiHostMsg_PPBFlashFile_ModuleLocal_CreateDir,
                           PP_Instance /* instance */,
                           std::string /* path */,
                           int32_t /* result */)
IPC_SYNC_MESSAGE_ROUTED2_2(PpapiHostMsg_PPBFlashFile_ModuleLocal_QueryFile,
                           PP_Instance /* instance */,
                           std::string /* path */,
                           PP_FileInfo /* info */,
                           int32_t /* result */)
IPC_SYNC_MESSAGE_ROUTED2_2(
    PpapiHostMsg_PPBFlashFile_ModuleLocal_GetDirContents,
    PP_Instance /* instance */,
    std::string /* path */,
    std::vector<ppapi::proxy::SerializedDirEntry> /* entries */,
    int32_t /* result */)

// PPB_Flash_Menu
IPC_SYNC_MESSAGE_ROUTED2_1(PpapiHostMsg_PPBFlashMenu_Create,
                           PP_Instance /* instance */,
                           ppapi::proxy::SerializedFlashMenu /* menu_data */,
                           ppapi::HostResource /* result */)
IPC_SYNC_MESSAGE_ROUTED2_0(PpapiHostMsg_PPBFlashMenu_Show,
                           ppapi::HostResource /* menu */,
                           PP_Point /* location */)
IPC_MESSAGE_ROUTED3(PpapiMsg_PPBFlashMenu_ShowACK,
                    ppapi::HostResource /* menu */,
                    int32_t /* selected_id */,
                    int32_t /* result */)

// PPB_Flash_NetConnector.
IPC_SYNC_MESSAGE_ROUTED1_1(PpapiHostMsg_PPBFlashNetConnector_Create,
                           PP_Instance /* instance_id */,
                           ppapi::HostResource /* result */)
IPC_MESSAGE_ROUTED3(PpapiHostMsg_PPBFlashNetConnector_ConnectTcp,
                    ppapi::HostResource /* connector */,
                    std::string /* host */,
                    uint16_t /* port */)
IPC_MESSAGE_ROUTED2(PpapiHostMsg_PPBFlashNetConnector_ConnectTcpAddress,
                    ppapi::HostResource /* connector */,
                    std::string /* net_address_as_string */)

// PPB_TCPSocket_Private.
IPC_SYNC_MESSAGE_CONTROL2_1(PpapiHostMsg_PPBTCPSocket_Create,
                            int32 /* routing_id */,
                            uint32 /* plugin_dispatcher_id */,
                            uint32 /* socket_id */)
IPC_MESSAGE_CONTROL3(PpapiHostMsg_PPBTCPSocket_Connect,
                     uint32 /* socket_id */,
                     std::string /* host */,
                     uint16_t /* port */)
IPC_MESSAGE_CONTROL2(PpapiHostMsg_PPBTCPSocket_ConnectWithNetAddress,
                     uint32 /* socket_id */,
                     PP_NetAddress_Private /* net_addr */)
IPC_MESSAGE_CONTROL3(PpapiHostMsg_PPBTCPSocket_SSLHandshake,
                     uint32 /* socket_id */,
                     std::string /* server_name */,
                     uint16_t /* server_port */)
IPC_MESSAGE_CONTROL2(PpapiHostMsg_PPBTCPSocket_Read,
                     uint32 /* socket_id */,
                     int32_t /* bytes_to_read */)
IPC_MESSAGE_CONTROL2(PpapiHostMsg_PPBTCPSocket_Write,
                     uint32 /* socket_id */,
                     std::string /* data */)
IPC_MESSAGE_CONTROL1(PpapiHostMsg_PPBTCPSocket_Disconnect,
                     uint32 /* socket_id */)

// PPB_UDPSocket_Private.
IPC_SYNC_MESSAGE_CONTROL2_1(PpapiHostMsg_PPBUDPSocket_Create,
                            int32 /* routing_id */,
                            uint32 /* plugin_dispatcher_id */,
                            uint32 /* socket_id */)
IPC_MESSAGE_CONTROL2(PpapiHostMsg_PPBUDPSocket_Bind,
                     uint32 /* socket_id */,
                     PP_NetAddress_Private /* net_addr */)
IPC_MESSAGE_CONTROL2(PpapiHostMsg_PPBUDPSocket_RecvFrom,
                     uint32 /* socket_id */,
                     int32_t /* num_bytes */)
IPC_MESSAGE_CONTROL3(PpapiHostMsg_PPBUDPSocket_SendTo,
                     uint32 /* socket_id */,
                     std::string /* data */,
                     PP_NetAddress_Private /* net_addr */)
IPC_MESSAGE_CONTROL1(PpapiHostMsg_PPBUDPSocket_Close,
                     uint32 /* socket_id */)

// PPB_Font.
IPC_SYNC_MESSAGE_CONTROL0_1(PpapiHostMsg_PPBFont_GetFontFamilies,
                            std::string /* result */)

// PPB_Graphics2D.
IPC_MESSAGE_ROUTED5(PpapiHostMsg_PPBGraphics2D_PaintImageData,
                    ppapi::HostResource /* graphics_2d */,
                    ppapi::HostResource /* image_data */,
                    PP_Point /* top_left */,
                    bool /* src_rect_specified */,
                    PP_Rect /* src_rect */)
IPC_MESSAGE_ROUTED4(PpapiHostMsg_PPBGraphics2D_Scroll,
                    ppapi::HostResource /* graphics_2d */,
                    bool /* clip_specified */,
                    PP_Rect /* clip */,
                    PP_Point /* amount */)
IPC_MESSAGE_ROUTED2(PpapiHostMsg_PPBGraphics2D_ReplaceContents,
                    ppapi::HostResource /* graphics_2d */,
                    ppapi::HostResource /* image_data */)
IPC_MESSAGE_ROUTED1(PpapiHostMsg_PPBGraphics2D_Flush,
                    ppapi::HostResource /* graphics_2d */)

// PPB_Graphics3D.
IPC_SYNC_MESSAGE_ROUTED2_1(PpapiHostMsg_PPBGraphics3D_Create,
                           PP_Instance /* instance */,
                           std::vector<int32_t> /* attrib_list */,
                           ppapi::HostResource /* result */)

IPC_SYNC_MESSAGE_ROUTED2_1(PpapiHostMsg_PPBGraphics3D_InitCommandBuffer,
                           ppapi::HostResource /* context */,
                           int32 /* size */,
                           base::SharedMemoryHandle /* ring_buffer */)

IPC_SYNC_MESSAGE_ROUTED1_1(PpapiHostMsg_PPBGraphics3D_GetState,
                           ppapi::HostResource /* context */,
                           gpu::CommandBuffer::State /* state */)

IPC_SYNC_MESSAGE_ROUTED3_1(PpapiHostMsg_PPBGraphics3D_Flush,
                           ppapi::HostResource /* context */,
                           int32 /* put_offset */,
                           int32 /* last_known_get */,
                           gpu::CommandBuffer::State /* state */)

IPC_MESSAGE_ROUTED2(PpapiHostMsg_PPBGraphics3D_AsyncFlush,
                    ppapi::HostResource /* context */,
                    int32 /* put_offset */)

IPC_SYNC_MESSAGE_ROUTED2_1(PpapiHostMsg_PPBGraphics3D_CreateTransferBuffer,
                           ppapi::HostResource /* context */,
                           int32 /* size */,
                           int32 /* id */)

IPC_SYNC_MESSAGE_ROUTED2_0(PpapiHostMsg_PPBGraphics3D_DestroyTransferBuffer,
                           ppapi::HostResource /* context */,
                           int32 /* id */)

IPC_SYNC_MESSAGE_ROUTED2_2(PpapiHostMsg_PPBGraphics3D_GetTransferBuffer,
                           ppapi::HostResource /* context */,
                           int32 /* id */,
                           base::SharedMemoryHandle /* transfer_buffer */,
                           uint32 /* size */)

IPC_MESSAGE_ROUTED1(PpapiHostMsg_PPBGraphics3D_SwapBuffers,
                    ppapi::HostResource /* graphics_3d */)

// PPB_Instance.
IPC_SYNC_MESSAGE_ROUTED1_1(PpapiHostMsg_PPBInstance_GetWindowObject,
                           PP_Instance /* instance */,
                           ppapi::proxy::SerializedVar /* result */)
IPC_SYNC_MESSAGE_ROUTED1_1(PpapiHostMsg_PPBInstance_GetOwnerElementObject,
                           PP_Instance /* instance */,
                           ppapi::proxy::SerializedVar /* result */)
IPC_SYNC_MESSAGE_ROUTED2_1(PpapiHostMsg_PPBInstance_BindGraphics,
                           PP_Instance /* instance */,
                           ppapi::HostResource /* device */,
                           PP_Bool /* result */)
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
IPC_MESSAGE_ROUTED3(PpapiHostMsg_PPBInstance_Log,
                    PP_Instance /* instance */,
                    int /* log_level */,
                    ppapi::proxy::SerializedVar /* value */)
IPC_MESSAGE_ROUTED4(PpapiHostMsg_PPBInstance_LogWithSource,
                    PP_Instance /* instance */,
                    int /* log_level */,
                    ppapi::proxy::SerializedVar /* source */,
                    ppapi::proxy::SerializedVar /* value */)
IPC_SYNC_MESSAGE_ROUTED2_1(PpapiHostMsg_PPBInstance_SetFullscreen,
                           PP_Instance /* instance */,
                           PP_Bool /* fullscreen */,
                           PP_Bool /* result */)
IPC_SYNC_MESSAGE_ROUTED1_2(PpapiHostMsg_PPBInstance_GetScreenSize,
                           PP_Instance /* instance */,
                           PP_Bool /* result */,
                           PP_Size /* size */)
IPC_SYNC_MESSAGE_ROUTED2_1(PpapiHostMsg_PPBInstance_FlashSetFullscreen,
                           PP_Instance /* instance */,
                           PP_Bool /* fullscreen */,
                           PP_Bool /* result */)
IPC_SYNC_MESSAGE_ROUTED1_2(PpapiHostMsg_PPBInstance_FlashGetScreenSize,
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
IPC_SYNC_MESSAGE_ROUTED1_1(PpapiHostMsg_PPBInstance_GetDocumentURL,
                           PP_Instance /* active */,
                           ppapi::proxy::SerializedVar /* result */)
IPC_SYNC_MESSAGE_ROUTED1_1(PpapiHostMsg_PPBInstance_GetPluginInstanceURL,
                           PP_Instance /* active */,
                           ppapi::proxy::SerializedVar /* result */)

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

// PPB_Surface3D.
IPC_SYNC_MESSAGE_ROUTED3_1(PpapiHostMsg_PPBSurface3D_Create,
                           PP_Instance /* instance */,
                           int32_t /* config */,
                           std::vector<int32_t> /* attrib_list */,
                           ppapi::HostResource /* result */)
IPC_MESSAGE_ROUTED1(PpapiHostMsg_PPBSurface3D_SwapBuffers,
                    ppapi::HostResource /* surface_3d */)

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

// PPB_TextInput.
IPC_MESSAGE_ROUTED2(PpapiHostMsg_PPBTextInput_SetTextInputType,
                    PP_Instance /* instance */,
                    PP_TextInput_Type /* type */)
IPC_MESSAGE_ROUTED3(PpapiHostMsg_PPBTextInput_UpdateCaretPosition,
                    PP_Instance /* instance */,
                    PP_Rect /* caret */,
                    PP_Rect /* bounding_box */)
IPC_MESSAGE_ROUTED1(PpapiHostMsg_PPBTextInput_CancelCompositionText,
                    PP_Instance /* instance */)

// PPB_URLLoader.
IPC_SYNC_MESSAGE_ROUTED1_1(PpapiHostMsg_PPBURLLoader_Create,
                           PP_Instance /* instance */,
                           ppapi::HostResource /* result */)
IPC_MESSAGE_ROUTED2(PpapiHostMsg_PPBURLLoader_Open,
                    ppapi::HostResource /* loader */,
                    ppapi::PPB_URLRequestInfo_Data /* request_data */)
IPC_MESSAGE_ROUTED1(PpapiHostMsg_PPBURLLoader_FollowRedirect,
                    ppapi::HostResource /* loader */)
IPC_SYNC_MESSAGE_ROUTED1_1(
    PpapiHostMsg_PPBURLLoader_GetResponseInfo,
    ppapi::HostResource /* loader */,
    ppapi::HostResource /* response_info_out */)
IPC_MESSAGE_ROUTED2(PpapiHostMsg_PPBURLLoader_ReadResponseBody,
                    ppapi::HostResource /* loader */,
                    int32_t /* bytes_to_read */)
IPC_MESSAGE_ROUTED1(PpapiHostMsg_PPBURLLoader_FinishStreamingToFile,
                    ppapi::HostResource /* loader */)
IPC_MESSAGE_ROUTED1(PpapiHostMsg_PPBURLLoader_Close,
                    ppapi::HostResource /* loader */)
IPC_MESSAGE_ROUTED1(PpapiHostMsg_PPBURLLoader_GrantUniversalAccess,
                    ppapi::HostResource /* loader */)

// PPB_URLResponseInfo.
IPC_SYNC_MESSAGE_ROUTED2_1(PpapiHostMsg_PPBURLResponseInfo_GetProperty,
                           ppapi::HostResource /* response */,
                           int32_t /* property */,
                           ppapi::proxy::SerializedVar /* result */)
IPC_SYNC_MESSAGE_ROUTED1_1(PpapiHostMsg_PPBURLResponseInfo_GetBodyAsFileRef,
                           ppapi::HostResource /* response */,
                           ppapi::PPB_FileRef_CreateInfo /* result */)

// PPB_Var.
IPC_SYNC_MESSAGE_ROUTED1_1(PpapiHostMsg_PPBVar_AddRefObject,
                           int64 /* object_id */,
                           int /* unused - need a return value for sync msgs */)
IPC_MESSAGE_ROUTED1(PpapiHostMsg_PPBVar_ReleaseObject,
                    int64 /* object_id */)
IPC_SYNC_MESSAGE_ROUTED3_2(PpapiHostMsg_PPBVar_ConvertType,
                           PP_Instance /* instance */,
                           ppapi::proxy::SerializedVar /* var */,
                           int /* new_type */,
                           ppapi::proxy::SerializedVar /* exception */,
                           ppapi::proxy::SerializedVar /* result */)
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

IPC_SYNC_MESSAGE_ROUTED3_1(PpapiHostMsg_ResourceCreation_Graphics2D,
                           PP_Instance /* instance */,
                           PP_Size /* size */,
                           PP_Bool /* is_always_opaque */,
                           ppapi::HostResource /* result */)
IPC_SYNC_MESSAGE_ROUTED4_3(PpapiHostMsg_ResourceCreation_ImageData,
                           PP_Instance /* instance */,
                           int32 /* format */,
                           PP_Size /* size */,
                           PP_Bool /* init_to_zero */,
                           ppapi::HostResource /* result_resource */,
                           std::string /* image_data_desc */,
                           ppapi::proxy::ImageHandle /* result */)

// PPB_VideoCapture_Dev.
IPC_SYNC_MESSAGE_ROUTED1_1(PpapiHostMsg_PPBVideoCapture_Create,
                           PP_Instance /* instance */,
                           ppapi::HostResource /* result */)
IPC_MESSAGE_ROUTED3(PpapiHostMsg_PPBVideoCapture_StartCapture,
                    ppapi::HostResource /* video_capture */,
                    PP_VideoCaptureDeviceInfo_Dev /* requested_info */,
                    uint32_t /* buffer_count */)
IPC_MESSAGE_ROUTED2(PpapiHostMsg_PPBVideoCapture_ReuseBuffer,
                    ppapi::HostResource /* video_capture */,
                    uint32_t /* buffer */)
IPC_MESSAGE_ROUTED1(PpapiHostMsg_PPBVideoCapture_StopCapture,
                    ppapi::HostResource /* video_capture */)

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
                    int32 /* size of buffer */)
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
