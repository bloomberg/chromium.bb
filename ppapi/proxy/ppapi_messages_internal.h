// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipc/ipc_message_macros.h"
#include "ppapi/c/dev/pp_file_info_dev.h"
#include "ppapi/c/ppb_var.h"

#define IPC_MESSAGE_START PpapiMsgStart

// These are from the plugin to the renderer
// Loads the given plugin.
IPC_MESSAGE_CONTROL3(PpapiMsg_LoadPlugin,
                     base::ProcessHandle /* host_process_handle */,
                     FilePath /* path */,
                     int /* renderer_id */)

IPC_SYNC_MESSAGE_CONTROL1_1(PpapiMsg_InitializeModule,
                            PP_Module /* module_id */,
                            bool /* result */)

IPC_MESSAGE_CONTROL0(PpapiMsg_Shutdown)

// Sent in both directions to see if the other side supports the given
// interface.
IPC_SYNC_MESSAGE_CONTROL1_1(PpapiMsg_SupportsInterface,
                            std::string /* interface_name */,
                            bool /* result */)

// Way for the renderer to list all interfaces that is supports in advance to
// avoid extra IPC traffic.
IPC_MESSAGE_CONTROL1(PpapiMsg_DeclareInterfaces,
                     std::vector<std::string> /* interfaces */)

IPC_MESSAGE_CONTROL2(PpapiMsg_ExecuteCallback,
                     uint32 /* serialized_callback */,
                     int32 /* param */)

// PPB_Audio.

// Notifies the result of the audio stream create call. This is called in
// both error cases and in the normal success case. These cases are
// differentiated by the result code, which is one of the standard PPAPI
// result codes.
//
// The handler of this message should always close all of the handles passed
// in, since some could be valid even in the error case.
IPC_MESSAGE_ROUTED5(PpapiMsg_PPBAudio_NotifyAudioStreamCreated,
                    PP_Resource /* audio_id */,
                    int32_t /* result_code (will be != PP_OK on failure) */,
                    IPC::PlatformFileForTransit /* socket_handle */,
                    base::SharedMemoryHandle /* handle */,
                    int32_t /* length */)

// PPB_Graphics2D.
IPC_MESSAGE_ROUTED2(PpapiMsg_PPBGraphics2D_FlushACK,
                    PP_Resource /* graphics_2d */,
                    int32_t /* pp_error */)

// PPP_Class.
IPC_SYNC_MESSAGE_ROUTED3_2(PpapiMsg_PPPClass_HasProperty,
                           int64 /* ppp_class */,
                           int64 /* object */,
                           pp::proxy::SerializedVar /* property */,
                           pp::proxy::SerializedVar /* out_exception */,
                           bool /* result */)
IPC_SYNC_MESSAGE_ROUTED3_2(PpapiMsg_PPPClass_HasMethod,
                           int64 /* ppp_class */,
                           int64 /* object */,
                           pp::proxy::SerializedVar /* method */,
                           pp::proxy::SerializedVar /* out_exception */,
                           bool /* result */)
IPC_SYNC_MESSAGE_ROUTED3_2(PpapiMsg_PPPClass_GetProperty,
                           int64 /* ppp_class */,
                           int64 /* object */,
                           pp::proxy::SerializedVar /* property */,
                           pp::proxy::SerializedVar /* out_exception */,
                           pp::proxy::SerializedVar /* result */)
IPC_SYNC_MESSAGE_ROUTED2_2(PpapiMsg_PPPClass_EnumerateProperties,
                           int64 /* ppp_class */,
                           int64 /* object */,
                           std::vector<pp::proxy::SerializedVar> /* props */,
                           pp::proxy::SerializedVar /* out_exception */)
IPC_SYNC_MESSAGE_ROUTED4_1(PpapiMsg_PPPClass_SetProperty,
                           int64 /* ppp_class */,
                           int64 /* object */,
                           pp::proxy::SerializedVar /* name */,
                           pp::proxy::SerializedVar /* value */,
                           pp::proxy::SerializedVar /* out_exception */)
IPC_SYNC_MESSAGE_ROUTED3_1(PpapiMsg_PPPClass_RemoveProperty,
                           int64 /* ppp_class */,
                           int64 /* object */,
                           pp::proxy::SerializedVar /* property */,
                           pp::proxy::SerializedVar /* out_exception */)
IPC_SYNC_MESSAGE_ROUTED4_2(PpapiMsg_PPPClass_Call,
                           int64 /* ppp_class */,
                           int64 /* object */,
                           pp::proxy::SerializedVar /* method_name */,
                           std::vector<pp::proxy::SerializedVar> /* args */,
                           pp::proxy::SerializedVar /* out_exception */,
                           pp::proxy::SerializedVar /* result */)
IPC_SYNC_MESSAGE_ROUTED3_2(PpapiMsg_PPPClass_Construct,
                           int64 /* ppp_class */,
                           int64 /* object */,
                           std::vector<pp::proxy::SerializedVar> /* args */,
                           pp::proxy::SerializedVar /* out_exception */,
                           pp::proxy::SerializedVar /* result */)
IPC_MESSAGE_ROUTED2(PpapiMsg_PPPClass_Deallocate,
                    int64 /* ppp_class */,
                    int64 /* object */)

// PPP_Instance.
IPC_SYNC_MESSAGE_ROUTED3_1(PpapiMsg_PPPInstance_DidCreate,
                           PP_Instance /* instance */,
                           std::vector<std::string> /* argn */,
                           std::vector<std::string> /* argv */,
                           PP_Bool /* result */)
IPC_MESSAGE_ROUTED1(PpapiMsg_PPPInstance_DidDestroy,
                    PP_Instance /* instance */)
IPC_MESSAGE_ROUTED3(PpapiMsg_PPPInstance_DidChangeView,
                    PP_Instance /* instance */,
                    PP_Rect /* position */,
                    PP_Rect /* clip */)
IPC_MESSAGE_ROUTED2(PpapiMsg_PPPInstance_DidChangeFocus,
                    PP_Instance /* instance */,
                    PP_Bool /* has_focus */)
IPC_SYNC_MESSAGE_ROUTED2_1(PpapiMsg_PPPInstance_HandleInputEvent,
                           PP_Instance /* instance */,
                           PP_InputEvent /* event */,
                           PP_Bool /* result */)
IPC_SYNC_MESSAGE_ROUTED2_1(PpapiMsg_PPPInstance_HandleDocumentLoad,
                           PP_Instance /* instance */,
                           PP_Resource /* url_loader */,
                           PP_Bool /* result */)
IPC_SYNC_MESSAGE_ROUTED1_1(PpapiMsg_PPPInstance_GetInstanceObject,
                           PP_Instance /* instance */,
                           pp::proxy::SerializedVar /* result */)


// PPB_URLLoader
// (Messages from browser to plugin to notify it of changes in state.)
IPC_MESSAGE_ROUTED5(PpapiMsg_PPBURLLoader_UpdateProgress,
                    PP_Resource /* resource */,
                    int64 /* bytes_sent */,
                    int64 /* total_bytes_to_be_sent */,
                    int64 /* bytes_received */,
                    int64 /* total_bytes_to_be_received */)
IPC_MESSAGE_ROUTED3(PpapiMsg_PPBURLLoader_ReadResponseBody_Ack,
                    PP_Resource /* loader */,
                    int32 /* result */,
                    std::string /* data */)

// -----------------------------------------------------------------------------
// These are from the plugin to the renderer.
// Reply to PpapiMsg_LoadPlugin.
IPC_MESSAGE_CONTROL1(PpapiHostMsg_PluginLoaded,
                     IPC::ChannelHandle /* handle */)

// PPB_Audio.
IPC_SYNC_MESSAGE_ROUTED2_1(PpapiHostMsg_PPBAudio_Create,
                           PP_Instance /* instance_id */,
                           PP_Resource /* config_id */,
                           PP_Resource /* result */)
IPC_MESSAGE_ROUTED2(PpapiHostMsg_PPBAudio_StartOrStop,
                    PP_Resource /* audio_id */,
                    bool /* play */)

// PPB_AudioConfig.
IPC_SYNC_MESSAGE_ROUTED3_1(PpapiHostMsg_PPBAudioConfig_Create,
                           PP_Instance /* instance */,
                           int32_t /* sample_rate */,
                           uint32_t /* sample_frame_count */,
                           PP_Resource /* result */)
IPC_SYNC_MESSAGE_ROUTED2_1(
    PpapiHostMsg_PPBAudioConfig_RecommendSampleFrameCount,
    int32_t /* sample_rate */,
    uint32_t /* requested */,
    uint32_t /* result */)

// PPB_Buffer.
IPC_SYNC_MESSAGE_ROUTED2_2(PpapiHostMsg_PPBBuffer_Create,
                           PP_Instance /* instance */,
                           uint32_t /* size */,
                           PP_Resource /* result_resource */,
                           int32_t /* result_shm_handle */)

// PPB_Core.
IPC_MESSAGE_ROUTED1(PpapiHostMsg_PPBCore_AddRefResource, PP_Resource)
IPC_MESSAGE_ROUTED1(PpapiHostMsg_PPBCore_ReleaseResource, PP_Resource)

// PPB_CharSet.
IPC_SYNC_MESSAGE_ROUTED4_2(PpapiHostMsg_PPBCharSet_UTF16ToCharSet,
                           PP_Instance /* instance */,
                           string16 /* utf16 */,
                           std::string /* char_set */,
                           int32_t /* on_error */,
                           std::string /* output */,
                           bool /* output_is_success */)
IPC_SYNC_MESSAGE_ROUTED4_2(PpapiHostMsg_PPBCharSet_CharSetToUTF16,
                           PP_Instance /* instance */,
                           std::string /* input */,
                           std::string /* char_set */,
                           int32_t /* on_error */,
                           string16 /* output */,
                           bool /* output_is_success */)
IPC_SYNC_MESSAGE_ROUTED1_1(PpapiHostMsg_PPBCharSet_GetDefaultCharSet,
                           PP_Instance /* instance */,
                           pp::proxy::SerializedVar /* result */)

// PPB_CursorControl.
IPC_SYNC_MESSAGE_ROUTED4_1(PpapiHostMsg_PPBCursorControl_SetCursor,
                           PP_Instance /* instance */,
                           int32_t /* type */,
                           PP_Resource /* custom_image */,
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

// PPB_Flash.
IPC_MESSAGE_ROUTED2(PpapiHostMsg_PPBFlash_SetInstanceAlwaysOnTop,
                    PP_Instance /* instance */,
                    PP_Bool /* on_top */)
// This has to be synchronous becuase the caller may want to composite on
// top of the resulting text after the call is complete.
IPC_SYNC_MESSAGE_ROUTED1_1(PpapiHostMsg_PPBFlash_DrawGlyphs,
                           pp::proxy::PPBFlash_DrawGlyphs_Params /* params */,
                           PP_Bool /* result */)
IPC_SYNC_MESSAGE_ROUTED2_1(PpapiHostMsg_PPBFlash_GetProxyForURL,
                           PP_Instance /* instance */,
                           std::string /* url */,
                           pp::proxy::SerializedVar /* result */)
IPC_SYNC_MESSAGE_ROUTED3_2(PpapiHostMsg_PPBFlash_OpenModuleLocalFile,
                           PP_Instance /* instance */,
                           std::string /* path */,
                           int32_t /* mode */,
                           IPC::PlatformFileForTransit /* file_handle */,
                           int32_t /* result */)
IPC_SYNC_MESSAGE_ROUTED3_1(PpapiHostMsg_PPBFlash_RenameModuleLocalFile,
                           PP_Instance /* instance */,
                           std::string /* path_from */,
                           std::string /* path_to */,
                           int32_t /* result */)
IPC_SYNC_MESSAGE_ROUTED3_1(PpapiHostMsg_PPBFlash_DeleteModuleLocalFileOrDir,
                           PP_Instance /* instance */,
                           std::string /* path */,
                           PP_Bool /* recursive */,
                           int32_t /* result */)
IPC_SYNC_MESSAGE_ROUTED2_1(PpapiHostMsg_PPBFlash_CreateModuleLocalDir,
                           PP_Instance /* instance */,
                           std::string /* path */,
                           int32_t /* result */)
IPC_SYNC_MESSAGE_ROUTED2_2(PpapiHostMsg_PPBFlash_QueryModuleLocalFile,
                           PP_Instance /* instance */,
                           std::string /* path */,
                           PP_FileInfo_Dev /* info */,
                           int32_t /* result */)
IPC_SYNC_MESSAGE_ROUTED2_2(
    PpapiHostMsg_PPBFlash_GetModuleLocalDirContents,
    PP_Instance /* instance */,
    std::string /* path */,
    std::vector<pp::proxy::SerializedDirEntry> /* entries */,
    int32_t /* result */)
IPC_SYNC_MESSAGE_ROUTED3_1(PpapiHostMsg_PPBFlash_NavigateToURL,
                           PP_Instance /* instance */,
                           std::string /* url */,
                           std::string /* target */,
                           PP_Bool /* result */)

// PPB_Font.
IPC_SYNC_MESSAGE_ROUTED2_3(
    PpapiHostMsg_PPBFont_Create,
    PP_Instance /* instance */,
    pp::proxy::SerializedFontDescription /* in_description */,
    PP_Resource /* result */,
    pp::proxy::SerializedFontDescription /* out_description */,
    std::string /* out_metrics */)
IPC_SYNC_MESSAGE_ROUTED2_1(PpapiHostMsg_PPBFont_DrawTextAt,
                           pp::proxy::SerializedVar /* text */,
                           pp::proxy::PPBFont_DrawTextAt_Params /* params */,
                           PP_Bool /* result */)
IPC_SYNC_MESSAGE_ROUTED4_1(PpapiHostMsg_PPBFont_MeasureText,
                           PP_Resource /* font */,
                           pp::proxy::SerializedVar /* text */,
                           PP_Bool /* text_is_rtl */,
                           PP_Bool /* override_direction */,
                           int32_t /* result */)
IPC_SYNC_MESSAGE_ROUTED5_1(PpapiHostMsg_PPBFont_CharacterOffsetForPixel,
                           PP_Resource /* font */,
                           pp::proxy::SerializedVar /* text */,
                           PP_Bool /* text_is_rtl */,
                           PP_Bool /* override_direction */,
                           int32_t /* pixel_pos */,
                           uint32_t /* result */)
IPC_SYNC_MESSAGE_ROUTED5_1(PpapiHostMsg_PPBFont_PixelOffsetForCharacter,
                           PP_Resource /* font */,
                           pp::proxy::SerializedVar /* text */,
                           PP_Bool /* text_is_rtl */,
                           PP_Bool /* override_direction */,
                           uint32_t /* char_offset */,
                           int32_t /* result */)

// PPB_Fullscreen.
IPC_SYNC_MESSAGE_ROUTED1_1(PpapiHostMsg_PPBFullscreen_IsFullscreen,
                           PP_Instance /* instance */,
                           PP_Bool /* result */)
IPC_SYNC_MESSAGE_ROUTED2_1(PpapiHostMsg_PPBFullscreen_SetFullscreen,
                           PP_Instance /* instance */,
                           PP_Bool /* fullscreen */,
                           PP_Bool /* result */)

// PPB_Graphics2D.
IPC_SYNC_MESSAGE_ROUTED3_1(PpapiHostMsg_PPBGraphics2D_Create,
                           PP_Instance /* instance */,
                           PP_Size /* size */,
                           PP_Bool /* is_always_opaque */,
                           PP_Resource /* result */)
IPC_MESSAGE_ROUTED5(PpapiHostMsg_PPBGraphics2D_PaintImageData,
                    PP_Resource /* graphics_2d */,
                    PP_Resource /* image_data */,
                    PP_Point /* top_left */,
                    bool /* src_rect_specified */,
                    PP_Rect /* src_rect */)
IPC_MESSAGE_ROUTED4(PpapiHostMsg_PPBGraphics2D_Scroll,
                    PP_Resource /* graphics_2d */,
                    bool /* clip_specified */,
                    PP_Rect /* clip */,
                    PP_Point /* amount */)
IPC_MESSAGE_ROUTED2(PpapiHostMsg_PPBGraphics2D_ReplaceContents,
                    PP_Resource /* graphics_2d */,
                    PP_Resource /* image_data */)
IPC_MESSAGE_ROUTED1(PpapiHostMsg_PPBGraphics2D_Flush,
                    PP_Resource /* graphics_2d */)

// PPB_ImageData.
IPC_SYNC_MESSAGE_ROUTED4_3(PpapiHostMsg_PPBImageData_Create,
                           PP_Instance /* instance */,
                           int32 /* format */,
                           PP_Size /* size */,
                           PP_Bool /* init_to_zero */,
                           PP_Resource /* result_resource */,
                           std::string /* image_data_desc */,
                           pp::proxy::ImageHandle /* result */)

// PPB_Instance.
IPC_SYNC_MESSAGE_ROUTED1_1(PpapiHostMsg_PPBInstance_GetWindowObject,
                           PP_Instance /* instance */,
                           pp::proxy::SerializedVar /* result */)
IPC_SYNC_MESSAGE_ROUTED1_1(PpapiHostMsg_PPBInstance_GetOwnerElementObject,
                           PP_Instance /* instance */,
                           pp::proxy::SerializedVar /* result */)
IPC_SYNC_MESSAGE_ROUTED2_1(PpapiHostMsg_PPBInstance_BindGraphics,
                           PP_Instance /* instance */,
                           PP_Resource /* device */,
                           PP_Bool /* result */)
IPC_SYNC_MESSAGE_ROUTED1_1(PpapiHostMsg_PPBInstance_IsFullFrame,
                           PP_Instance /* instance */,
                           PP_Bool /* result */)
IPC_SYNC_MESSAGE_ROUTED2_2(PpapiHostMsg_PPBInstance_ExecuteScript,
                           PP_Instance /* instance */,
                           pp::proxy::SerializedVar /* script */,
                           pp::proxy::SerializedVar /* out_exception */,
                           pp::proxy::SerializedVar /* result */)

IPC_SYNC_MESSAGE_ROUTED3_1(
    PpapiHostMsg_PPBPDF_GetFontFileWithFallback,
    PP_Instance /* instance */,
    pp::proxy::SerializedFontDescription /* description */,
    int32_t /* charset */,
    PP_Resource /* result */)
IPC_SYNC_MESSAGE_ROUTED2_1(
    PpapiHostMsg_PPBPDF_GetFontTableForPrivateFontFile,
    PP_Resource /* font_file */,
    uint32_t /* table */,
    std::string /* result */)

// PPB_Testing.
IPC_SYNC_MESSAGE_ROUTED3_1(PpapiHostMsg_PPBTesting_ReadImageData,
                           PP_Resource /* device_context_2d */,
                           PP_Resource /* image */,
                           PP_Point /* top_left */,
                           PP_Bool /* result */)
IPC_SYNC_MESSAGE_ROUTED0_1(PpapiHostMsg_PPBTesting_RunMessageLoop,
                           bool /* dummy since there's no 0_0 variant */)
IPC_MESSAGE_ROUTED0(PpapiHostMsg_PPBTesting_QuitMessageLoop)
IPC_SYNC_MESSAGE_ROUTED1_1(PpapiHostMsg_PPBTesting_GetLiveObjectsForInstance,
                           PP_Instance /* instance */,
                           uint32 /* result */)

// PPB_URLLoader.
IPC_SYNC_MESSAGE_ROUTED1_1(PpapiHostMsg_PPBURLLoader_Create,
                           PP_Instance /* instance */,
                           PP_Resource /* result */)
IPC_MESSAGE_ROUTED3(PpapiHostMsg_PPBURLLoader_Open,
                    PP_Resource /* loader */,
                    PP_Resource /*request_info */,
                    uint32_t /* serialized_callback */)
IPC_MESSAGE_ROUTED2(PpapiHostMsg_PPBURLLoader_FollowRedirect,
                    PP_Resource /* loader */,
                    uint32_t /* serialized_callback */)
IPC_SYNC_MESSAGE_ROUTED1_1(PpapiHostMsg_PPBURLLoader_GetResponseInfo,
                           PP_Resource /* loader */,
                           PP_Resource /* response_info_out */)
IPC_MESSAGE_ROUTED2(PpapiHostMsg_PPBURLLoader_ReadResponseBody,
                    PP_Resource /* loader */,
                    int32_t /* bytes_to_read */)
IPC_MESSAGE_ROUTED2(PpapiHostMsg_PPBURLLoader_FinishStreamingToFile,
                    PP_Resource /* loader */,
                    uint32_t /* serialized_callback */)
IPC_MESSAGE_ROUTED1(PpapiHostMsg_PPBURLLoader_Close,
                    PP_Resource /* loader */)

// PPB_URLLoaderTrusted.
IPC_MESSAGE_ROUTED1(PpapiHostMsg_PPBURLLoaderTrusted_GrantUniversalAccess,
                    PP_Resource /* loader */)

// PPB_URLRequestInfo.
IPC_SYNC_MESSAGE_ROUTED1_1(PpapiHostMsg_PPBURLRequestInfo_Create,
                           PP_Instance /* instance */,
                           PP_Resource /* result */)
IPC_MESSAGE_ROUTED3(PpapiHostMsg_PPBURLRequestInfo_SetProperty,
                    PP_Resource /* request */,
                    int32_t /* property */,
                    pp::proxy::SerializedVar /* value */)
IPC_MESSAGE_ROUTED2(PpapiHostMsg_PPBURLRequestInfo_AppendDataToBody,
                    PP_Resource /* request */,
                    std::string /* data */)
IPC_MESSAGE_ROUTED5(PpapiHostMsg_PPBURLRequestInfo_AppendFileToBody,
                    PP_Resource /* request */,
                    PP_Resource /* file_ref */,
                    int64_t /* start_offset */,
                    int64_t /* number_of_bytes */,
                    double /* expected_last_modified_time */)

// PPB_URLResponseInfo.
IPC_SYNC_MESSAGE_ROUTED2_1(PpapiHostMsg_PPBURLResponseInfo_GetProperty,
                           PP_Resource /* response */,
                           int32_t /* property */,
                           pp::proxy::SerializedVar /* result */)
IPC_SYNC_MESSAGE_ROUTED1_1(PpapiHostMsg_PPBURLResponseInfo_GetBodyAsFileRef,
                           PP_Resource /* response */,
                           PP_Resource /* file_ref_result */)

// PPB_Var.
IPC_MESSAGE_ROUTED1(PpapiHostMsg_PPBVar_AddRefObject,
                    int64 /* object_id */)
IPC_MESSAGE_ROUTED1(PpapiHostMsg_PPBVar_ReleaseObject,
                    int64 /* object_id */)
IPC_SYNC_MESSAGE_ROUTED3_2(PpapiHostMsg_PPBVar_ConvertType,
                           PP_Instance /* instance */,
                           pp::proxy::SerializedVar /* var */,
                           int /* new_type */,
                           pp::proxy::SerializedVar /* exception */,
                           pp::proxy::SerializedVar /* result */)
IPC_MESSAGE_ROUTED3(PpapiHostMsg_PPBVar_DefineProperty,
                    pp::proxy::SerializedVar /* object */,
                    PP_ObjectProperty /* property */,
                    pp::proxy::SerializedVar /* out_exception */)
IPC_SYNC_MESSAGE_ROUTED2_2(PpapiHostMsg_PPBVar_HasProperty,
                           pp::proxy::SerializedVar /* object */,
                           pp::proxy::SerializedVar /* property */,
                           pp::proxy::SerializedVar /* out_exception */,
                           PP_Bool /* result */)
IPC_SYNC_MESSAGE_ROUTED2_2(PpapiHostMsg_PPBVar_HasMethodDeprecated,
                           pp::proxy::SerializedVar /* object */,
                           pp::proxy::SerializedVar /* method */,
                           pp::proxy::SerializedVar /* out_exception */,
                           PP_Bool /* result */)
IPC_SYNC_MESSAGE_ROUTED2_2(PpapiHostMsg_PPBVar_GetProperty,
                           pp::proxy::SerializedVar /* object */,
                           pp::proxy::SerializedVar /* property */,
                           pp::proxy::SerializedVar /* out_exception */,
                           pp::proxy::SerializedVar /* result */)
IPC_SYNC_MESSAGE_ROUTED2_2(PpapiHostMsg_PPBVar_DeleteProperty,
                           pp::proxy::SerializedVar /* object */,
                           pp::proxy::SerializedVar /* property */,
                           pp::proxy::SerializedVar /* out_exception */,
                           PP_Bool /* result */)
IPC_SYNC_MESSAGE_ROUTED1_2(PpapiHostMsg_PPBVar_EnumerateProperties,
                           pp::proxy::SerializedVar /* object */,
                           std::vector<pp::proxy::SerializedVar> /* props */,
                           pp::proxy::SerializedVar /* out_exception */)
IPC_SYNC_MESSAGE_ROUTED3_1(PpapiHostMsg_PPBVar_SetPropertyDeprecated,
                           pp::proxy::SerializedVar /* object */,
                           pp::proxy::SerializedVar /* name */,
                           pp::proxy::SerializedVar /* value */,
                           pp::proxy::SerializedVar /* out_exception */)
IPC_SYNC_MESSAGE_ROUTED1_1(PpapiHostMsg_PPBVar_IsCallable,
                           pp::proxy::SerializedVar /* object */,
                           PP_Bool /* result */)
IPC_SYNC_MESSAGE_ROUTED4_2(PpapiHostMsg_PPBVar_Call,
                           pp::proxy::SerializedVar /* object */,
                           pp::proxy::SerializedVar /* this_object */,
                           pp::proxy::SerializedVar /* method_name */,
                           std::vector<pp::proxy::SerializedVar> /* args */,
                           pp::proxy::SerializedVar /* out_exception */,
                           pp::proxy::SerializedVar /* result */)
IPC_SYNC_MESSAGE_ROUTED3_2(PpapiHostMsg_PPBVar_CallDeprecated,
                           pp::proxy::SerializedVar /* object */,
                           pp::proxy::SerializedVar /* method_name */,
                           std::vector<pp::proxy::SerializedVar> /* args */,
                           pp::proxy::SerializedVar /* out_exception */,
                           pp::proxy::SerializedVar /* result */)
IPC_SYNC_MESSAGE_ROUTED2_2(PpapiHostMsg_PPBVar_Construct,
                           pp::proxy::SerializedVar /* object */,
                           std::vector<pp::proxy::SerializedVar> /* args */,
                           pp::proxy::SerializedVar /* out_exception */,
                           pp::proxy::SerializedVar /* result */)
IPC_SYNC_MESSAGE_ROUTED2_2(PpapiHostMsg_PPBVar_IsInstanceOfDeprecated,
                           pp::proxy::SerializedVar /* var */,
                           int64 /* object_class */,
                           int64 /* object-data */,
                           PP_Bool /* result */)
IPC_SYNC_MESSAGE_ROUTED3_1(PpapiHostMsg_PPBVar_CreateObjectDeprecated,
                           PP_Instance /* instance */,
                           int64 /* object_class */,
                           int64 /* object_data */,
                           pp::proxy::SerializedVar /* result */)
