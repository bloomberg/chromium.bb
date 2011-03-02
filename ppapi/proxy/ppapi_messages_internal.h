// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/common/command_buffer.h"
#include "gpu/ipc/gpu_command_buffer_traits.h"
#include "ipc/ipc_message_macros.h"
#include "ppapi/c/dev/pp_file_info_dev.h"
#include "ppapi/c/ppb_var.h"

#define IPC_MESSAGE_START PpapiMsgStart

// These are from the plugin to the renderer
// Loads the given plugin.
IPC_MESSAGE_CONTROL1(PpapiMsg_LoadPlugin, FilePath /* path */)

// Creates a channel to talk to a renderer. The plugin will respond with
// PpapiHostMsg_ChannelCreated.
IPC_MESSAGE_CONTROL2(PpapiMsg_CreateChannel,
                     base::ProcessHandle /* host_process_handle */,
                     int /* renderer_id */);

// Sent in both directions to see if the other side supports the given
// interface.
IPC_SYNC_MESSAGE_CONTROL1_1(PpapiMsg_SupportsInterface,
                            std::string /* interface_name */,
                            bool /* result */)

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
                    pp::proxy::HostResource /* audio_id */,
                    int32_t /* result_code (will be != PP_OK on failure) */,
                    IPC::PlatformFileForTransit /* socket_handle */,
                    base::SharedMemoryHandle /* handle */,
                    int32_t /* length */)

// PPB_FileChooser.
IPC_MESSAGE_ROUTED3(
    PpapiMsg_PPBFileChooser_ChooseComplete,
    pp::proxy::HostResource /* chooser */,
    int32_t /* result_code (will be != PP_OK on failure */,
    std::vector<pp::proxy::PPBFileRef_CreateInfo> /* chosen_files */)

// PPB_FileSystem.
IPC_MESSAGE_ROUTED2(
    PpapiMsg_PPBFileSystem_OpenComplete,
    pp::proxy::HostResource /* filesystem */,
    int32_t /* result */)

// PPB_Graphics2D.
IPC_MESSAGE_ROUTED2(PpapiMsg_PPBGraphics2D_FlushACK,
                    pp::proxy::HostResource /* graphics_2d */,
                    int32_t /* pp_error */)

// PPB_Surface3D.
IPC_MESSAGE_ROUTED2(PpapiMsg_PPBSurface3D_SwapBuffersACK,
                    pp::proxy::HostResource /* surface_3d */,
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
// TODO(piman): DidChangeView should be async, but doing so causes an issue with
// webkit and accelerated compositing. Turn back to async once this is fixed.
IPC_SYNC_MESSAGE_ROUTED3_0(PpapiMsg_PPPInstance_DidChangeView,
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
                           pp::proxy::HostResource /* url_loader */,
                           PP_Bool /* result */)
IPC_SYNC_MESSAGE_ROUTED1_1(PpapiMsg_PPPInstance_GetInstanceObject,
                           PP_Instance /* instance */,
                           pp::proxy::SerializedVar /* result */)


// PPB_URLLoader
// (Messages from browser to plugin to notify it of changes in state.)
IPC_MESSAGE_ROUTED1(PpapiMsg_PPBURLLoader_UpdateProgress,
                    pp::proxy::PPBURLLoader_UpdateProgress_Params /* params */)
IPC_MESSAGE_ROUTED3(PpapiMsg_PPBURLLoader_ReadResponseBody_Ack,
                    pp::proxy::HostResource /* loader */,
                    int32 /* result */,
                    std::string /* data */)

// -----------------------------------------------------------------------------
// These are from the plugin to the renderer.

// Reply to PpapiMsg_CreateChannel. The handle will be NULL if the channel
// could not be established. This could be because the IPC could not be created
// for some weird reason, but more likely that the plugin failed to load or
// initialize properly.
IPC_MESSAGE_CONTROL1(PpapiHostMsg_ChannelCreated,
                     IPC::ChannelHandle /* handle */)

// PPB_Audio.
IPC_SYNC_MESSAGE_ROUTED2_1(PpapiHostMsg_PPBAudio_Create,
                           PP_Instance /* instance_id */,
                           pp::proxy::HostResource /* config_id */,
                           pp::proxy::HostResource /* result */)
IPC_MESSAGE_ROUTED2(PpapiHostMsg_PPBAudio_StartOrStop,
                    pp::proxy::HostResource /* audio_id */,
                    bool /* play */)

// PPB_AudioConfig.
IPC_SYNC_MESSAGE_ROUTED3_1(PpapiHostMsg_PPBAudioConfig_Create,
                           PP_Instance /* instance */,
                           int32_t /* sample_rate */,
                           uint32_t /* sample_frame_count */,
                           pp::proxy::HostResource /* result */)
IPC_SYNC_MESSAGE_ROUTED2_1(
    PpapiHostMsg_PPBAudioConfig_RecommendSampleFrameCount,
    int32_t /* sample_rate */,
    uint32_t /* requested */,
    uint32_t /* result */)

// PPB_Buffer.
IPC_SYNC_MESSAGE_ROUTED2_2(PpapiHostMsg_PPBBuffer_Create,
                           PP_Instance /* instance */,
                           uint32_t /* size */,
                           pp::proxy::HostResource /* result_resource */,
                           int32_t /* result_shm_handle */)

// PPB_Context3D.
IPC_SYNC_MESSAGE_ROUTED3_1(PpapiHostMsg_PPBContext3D_Create,
                           PP_Instance /* instance */,
                           int32_t /* config */,
                           std::vector<int32_t> /* attrib_list */,
                           pp::proxy::HostResource /* result */)

IPC_SYNC_MESSAGE_ROUTED3_1(PpapiHostMsg_PPBContext3D_BindSurfaces,
                           pp::proxy::HostResource /* context */,
                           pp::proxy::HostResource /* draw */,
                           pp::proxy::HostResource /* read */,
                           int32_t /* result */)

IPC_SYNC_MESSAGE_ROUTED2_1(PpapiHostMsg_PPBContext3D_Initialize,
                           pp::proxy::HostResource /* context */,
                           int32 /* size */,
                           base::SharedMemoryHandle /* ring_buffer */)

IPC_SYNC_MESSAGE_ROUTED1_1(PpapiHostMsg_PPBContext3D_GetState,
                           pp::proxy::HostResource /* context */,
                           gpu::CommandBuffer::State /* state */)

IPC_SYNC_MESSAGE_ROUTED2_1(PpapiHostMsg_PPBContext3D_Flush,
                           pp::proxy::HostResource /* context */,
                           int32 /* put_offset */,
                           gpu::CommandBuffer::State /* state */)

IPC_MESSAGE_ROUTED2(PpapiHostMsg_PPBContext3D_AsyncFlush,
                    pp::proxy::HostResource /* context */,
                    int32 /* put_offset */)

IPC_SYNC_MESSAGE_ROUTED2_1(PpapiHostMsg_PPBContext3D_CreateTransferBuffer,
                           pp::proxy::HostResource /* context */,
                           int32 /* size */,
                           int32 /* id */)

IPC_SYNC_MESSAGE_ROUTED2_0(PpapiHostMsg_PPBContext3D_DestroyTransferBuffer,
                           pp::proxy::HostResource /* context */,
                           int32 /* id */)

IPC_SYNC_MESSAGE_ROUTED2_2(PpapiHostMsg_PPBContext3D_GetTransferBuffer,
                           pp::proxy::HostResource /* context */,
                           int32 /* id */,
                           base::SharedMemoryHandle /* transfer_buffer */,
                           uint32 /* size */)

// PPB_Core.
IPC_MESSAGE_ROUTED1(PpapiHostMsg_PPBCore_AddRefResource,
                    pp::proxy::HostResource)
IPC_MESSAGE_ROUTED1(PpapiHostMsg_PPBCore_ReleaseResource,
                    pp::proxy::HostResource)

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
                           pp::proxy::HostResource /* custom_image */,
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
                           pp::proxy::HostResource /* result */)
IPC_MESSAGE_ROUTED1(PpapiHostMsg_PPBFileChooser_Show,
                    pp::proxy::HostResource /* file_chooser */)


// PPB_FileRef.
IPC_SYNC_MESSAGE_ROUTED2_1(PpapiHostMsg_PPBFileRef_Create,
                           pp::proxy::HostResource /* file_system */,
                           std::string /* path */,
                           pp::proxy::PPBFileRef_CreateInfo /* result */)
IPC_SYNC_MESSAGE_ROUTED1_1(PpapiHostMsg_PPBFileRef_GetParent,
                           pp::proxy::HostResource /* file_ref */,
                           pp::proxy::PPBFileRef_CreateInfo /* result */)
IPC_MESSAGE_ROUTED3(PpapiHostMsg_PPBFileRef_MakeDirectory,
                    pp::proxy::HostResource /* file_ref */,
                    PP_Bool /* make_ancestors */,
                    uint32_t /* serialized_callback */);
IPC_MESSAGE_ROUTED4(PpapiHostMsg_PPBFileRef_Touch,
                    pp::proxy::HostResource /* file_ref */,
                    PP_Time /* last_access */,
                    PP_Time /* last_modified */,
                    uint32_t /* serialized_callback */);
IPC_MESSAGE_ROUTED2(PpapiHostMsg_PPBFileRef_Delete,
                    pp::proxy::HostResource /* file_ref */,
                    uint32_t /* serialized_callback */);
IPC_MESSAGE_ROUTED3(PpapiHostMsg_PPBFileRef_Rename,
                    pp::proxy::HostResource /* file_ref */,
                    pp::proxy::HostResource /* new_file_ref */,
                    uint32_t /* serialized_callback */);

// PPB_FileSystem.
IPC_SYNC_MESSAGE_ROUTED2_1(PpapiHostMsg_PPBFileSystem_Create,
                           PP_Instance /* instance */,
                           int /* type */,
                           pp::proxy::HostResource /* result */)
IPC_MESSAGE_ROUTED2(PpapiHostMsg_PPBFileSystem_Open,
                    pp::proxy::HostResource /* result */,
                    int64_t /* expected_size */)

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
IPC_SYNC_MESSAGE_ROUTED3_1(PpapiHostMsg_PPBFlash_NavigateToURL,
                           PP_Instance /* instance */,
                           std::string /* url */,
                           std::string /* target */,
                           PP_Bool /* result */)
IPC_SYNC_MESSAGE_ROUTED1_0(PpapiHostMsg_PPBFlash_RunMessageLoop,
                           PP_Instance /* instance */)
IPC_SYNC_MESSAGE_ROUTED1_0(PpapiHostMsg_PPBFlash_QuitMessageLoop,
                           PP_Instance /* instance */)

// PPB_Flash_File_FileRef.
IPC_SYNC_MESSAGE_ROUTED2_2(PpapiHostMsg_PPBFlashFile_FileRef_OpenFile,
                           pp::proxy::HostResource /* file_ref */,
                           int32_t /* mode */,
                           IPC::PlatformFileForTransit /* file_handle */,
                           int32_t /* result */)
IPC_SYNC_MESSAGE_ROUTED1_2(PpapiHostMsg_PPBFlashFile_FileRef_QueryFile,
                           pp::proxy::HostResource /* file_ref */,
                           PP_FileInfo_Dev /* info */,
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
                           PP_FileInfo_Dev /* info */,
                           int32_t /* result */)
IPC_SYNC_MESSAGE_ROUTED2_2(
    PpapiHostMsg_PPBFlashFile_ModuleLocal_GetDirContents,
    PP_Instance /* instance */,
    std::string /* path */,
    std::vector<pp::proxy::SerializedDirEntry> /* entries */,
    int32_t /* result */)

// PPB_Flash_Menu
IPC_SYNC_MESSAGE_ROUTED2_1(PpapiHostMsg_PPBFlashMenu_Create,
                           PP_Instance /* instance */,
                           pp::proxy::SerializedFlashMenu /* menu_data */,
                           pp::proxy::HostResource /* result */)
IPC_MESSAGE_ROUTED2(PpapiHostMsg_PPBFlashMenu_Show,
                    pp::proxy::HostResource /* menu */,
                    PP_Point /* location */)
IPC_MESSAGE_ROUTED3(PpapiMsg_PPBFlashMenu_ShowACK,
                    pp::proxy::HostResource /* menu */,
                    int32_t /* selected_id */,
                    int32_t /* result */)


// PPB_Font.
IPC_SYNC_MESSAGE_ROUTED2_3(
    PpapiHostMsg_PPBFont_Create,
    PP_Instance /* instance */,
    pp::proxy::SerializedFontDescription /* in_description */,
    pp::proxy::HostResource /* result */,
    pp::proxy::SerializedFontDescription /* out_description */,
    std::string /* out_metrics */)
IPC_SYNC_MESSAGE_ROUTED2_1(PpapiHostMsg_PPBFont_DrawTextAt,
                           pp::proxy::SerializedVar /* text */,
                           pp::proxy::PPBFont_DrawTextAt_Params /* params */,
                           PP_Bool /* result */)
IPC_SYNC_MESSAGE_ROUTED4_1(PpapiHostMsg_PPBFont_MeasureText,
                           pp::proxy::HostResource /* font */,
                           pp::proxy::SerializedVar /* text */,
                           PP_Bool /* text_is_rtl */,
                           PP_Bool /* override_direction */,
                           int32_t /* result */)
IPC_SYNC_MESSAGE_ROUTED5_1(PpapiHostMsg_PPBFont_CharacterOffsetForPixel,
                           pp::proxy::HostResource /* font */,
                           pp::proxy::SerializedVar /* text */,
                           PP_Bool /* text_is_rtl */,
                           PP_Bool /* override_direction */,
                           int32_t /* pixel_pos */,
                           uint32_t /* result */)
IPC_SYNC_MESSAGE_ROUTED5_1(PpapiHostMsg_PPBFont_PixelOffsetForCharacter,
                           pp::proxy::HostResource /* font */,
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
                           pp::proxy::HostResource /* result */)
IPC_MESSAGE_ROUTED5(PpapiHostMsg_PPBGraphics2D_PaintImageData,
                    pp::proxy::HostResource /* graphics_2d */,
                    pp::proxy::HostResource /* image_data */,
                    PP_Point /* top_left */,
                    bool /* src_rect_specified */,
                    PP_Rect /* src_rect */)
IPC_MESSAGE_ROUTED4(PpapiHostMsg_PPBGraphics2D_Scroll,
                    pp::proxy::HostResource /* graphics_2d */,
                    bool /* clip_specified */,
                    PP_Rect /* clip */,
                    PP_Point /* amount */)
IPC_MESSAGE_ROUTED2(PpapiHostMsg_PPBGraphics2D_ReplaceContents,
                    pp::proxy::HostResource /* graphics_2d */,
                    pp::proxy::HostResource /* image_data */)
IPC_MESSAGE_ROUTED1(PpapiHostMsg_PPBGraphics2D_Flush,
                    pp::proxy::HostResource /* graphics_2d */)

// PPB_ImageData.
IPC_SYNC_MESSAGE_ROUTED4_3(PpapiHostMsg_PPBImageData_Create,
                           PP_Instance /* instance */,
                           int32 /* format */,
                           PP_Size /* size */,
                           PP_Bool /* init_to_zero */,
                           pp::proxy::HostResource /* result_resource */,
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
                           pp::proxy::HostResource /* device */,
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
    pp::proxy::HostResource /* result */)
IPC_SYNC_MESSAGE_ROUTED2_1(
    PpapiHostMsg_PPBPDF_GetFontTableForPrivateFontFile,
    pp::proxy::HostResource /* font_file */,
    uint32_t /* table */,
    std::string /* result */)

// PPB_Surface3D.
IPC_SYNC_MESSAGE_ROUTED3_1(PpapiHostMsg_PPBSurface3D_Create,
                           PP_Instance /* instance */,
                           int32_t /* config */,
                           std::vector<int32_t> /* attrib_list */,
                           pp::proxy::HostResource /* result */)
IPC_MESSAGE_ROUTED1(PpapiHostMsg_PPBSurface3D_SwapBuffers,
                    pp::proxy::HostResource /* surface_3d */)

// PPB_Testing.
IPC_SYNC_MESSAGE_ROUTED3_1(
    PpapiHostMsg_PPBTesting_ReadImageData,
    pp::proxy::HostResource /* device_context_2d */,
    pp::proxy::HostResource /* image */,
    PP_Point /* top_left */,
    PP_Bool /* result */)
IPC_SYNC_MESSAGE_ROUTED1_0(PpapiHostMsg_PPBTesting_RunMessageLoop,
                           PP_Instance /* instance */)
IPC_SYNC_MESSAGE_ROUTED1_0(PpapiHostMsg_PPBTesting_QuitMessageLoop,
                           PP_Instance /* instance */)
IPC_SYNC_MESSAGE_ROUTED1_1(PpapiHostMsg_PPBTesting_GetLiveObjectsForInstance,
                           PP_Instance /* instance */,
                           uint32 /* result */)

// PPB_URLLoader.
IPC_SYNC_MESSAGE_ROUTED1_1(PpapiHostMsg_PPBURLLoader_Create,
                           PP_Instance /* instance */,
                           pp::proxy::HostResource /* result */)
IPC_MESSAGE_ROUTED3(PpapiHostMsg_PPBURLLoader_Open,
                    pp::proxy::HostResource /* loader */,
                    pp::proxy::HostResource /*request_info */,
                    uint32_t /* serialized_callback */)
IPC_MESSAGE_ROUTED2(PpapiHostMsg_PPBURLLoader_FollowRedirect,
                    pp::proxy::HostResource /* loader */,
                    uint32_t /* serialized_callback */)
IPC_SYNC_MESSAGE_ROUTED1_1(
    PpapiHostMsg_PPBURLLoader_GetResponseInfo,
    pp::proxy::HostResource /* loader */,
    pp::proxy::HostResource /* response_info_out */)
IPC_MESSAGE_ROUTED2(PpapiHostMsg_PPBURLLoader_ReadResponseBody,
                    pp::proxy::HostResource /* loader */,
                    int32_t /* bytes_to_read */)
IPC_MESSAGE_ROUTED2(PpapiHostMsg_PPBURLLoader_FinishStreamingToFile,
                    pp::proxy::HostResource /* loader */,
                    uint32_t /* serialized_callback */)
IPC_MESSAGE_ROUTED1(PpapiHostMsg_PPBURLLoader_Close,
                    pp::proxy::HostResource /* loader */)

// PPB_URLLoaderTrusted.
IPC_MESSAGE_ROUTED1(PpapiHostMsg_PPBURLLoaderTrusted_GrantUniversalAccess,
                    pp::proxy::HostResource /* loader */)

// PPB_URLRequestInfo.
IPC_SYNC_MESSAGE_ROUTED1_1(PpapiHostMsg_PPBURLRequestInfo_Create,
                           PP_Instance /* instance */,
                           pp::proxy::HostResource /* result */)
IPC_MESSAGE_ROUTED3(PpapiHostMsg_PPBURLRequestInfo_SetProperty,
                    pp::proxy::HostResource /* request */,
                    int32_t /* property */,
                    pp::proxy::SerializedVar /* value */)
IPC_MESSAGE_ROUTED2(PpapiHostMsg_PPBURLRequestInfo_AppendDataToBody,
                    pp::proxy::HostResource /* request */,
                    std::string /* data */)
IPC_MESSAGE_ROUTED5(PpapiHostMsg_PPBURLRequestInfo_AppendFileToBody,
                    pp::proxy::HostResource /* request */,
                    pp::proxy::HostResource /* file_ref */,
                    int64_t /* start_offset */,
                    int64_t /* number_of_bytes */,
                    double /* expected_last_modified_time */)

// PPB_URLResponseInfo.
IPC_SYNC_MESSAGE_ROUTED2_1(PpapiHostMsg_PPBURLResponseInfo_GetProperty,
                           pp::proxy::HostResource /* response */,
                           int32_t /* property */,
                           pp::proxy::SerializedVar /* result */)
IPC_SYNC_MESSAGE_ROUTED1_1(PpapiHostMsg_PPBURLResponseInfo_GetBodyAsFileRef,
                           pp::proxy::HostResource /* response */,
                           pp::proxy::PPBFileRef_CreateInfo /* result */)

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
