// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This header is meant to be included in multiple passes, hence no traditional
// header guard. It is included by backing_store_messages_internal.h
// See ipc_message_macros.h for explanation of the macros and passes.

// This file needs to be included again, even though we're actually included
// from it via utility_messages.h.
#include "ipc/ipc_message_macros.h"

// These are from the plugin to the renderer
IPC_BEGIN_MESSAGES(Ppapi)
  // Loads the given plugin.
  IPC_MESSAGE_CONTROL2(PpapiMsg_LoadPlugin,
                       FilePath /* path */,
                       int /* renderer_id */)

  IPC_SYNC_MESSAGE_CONTROL1_1(PpapiMsg_InitializeModule,
                              PP_Module /* module_id */,
                              bool /* result */)

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
                             bool /* result */)
  IPC_MESSAGE_ROUTED1(PpapiMsg_PPPInstance_DidDestroy,
                      PP_Instance /* instance */)
  IPC_MESSAGE_ROUTED3(PpapiMsg_PPPInstance_DidChangeView,
                      PP_Instance /* instance */,
                      PP_Rect /* position */,
                      PP_Rect /* clip */)
  IPC_MESSAGE_ROUTED2(PpapiMsg_PPPInstance_DidChangeFocus,
                      PP_Instance /* instance */,
                      bool /* has_focus */)
  IPC_SYNC_MESSAGE_ROUTED2_1(PpapiMsg_PPPInstance_HandleInputEvent,
                             PP_Instance /* instance */,
                             PP_InputEvent /* event */,
                             bool /* result */)
  IPC_SYNC_MESSAGE_ROUTED2_1(PpapiMsg_PPPInstance_HandleDocumentLoad,
                             PP_Instance /* instance */,
                             PP_Resource /* url_loader */,
                             bool /* result */)
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
IPC_END_MESSAGES(Ppapi)

// -----------------------------------------------------------------------------
// These are from the plugin to the renderer.
IPC_BEGIN_MESSAGES(PpapiHost)
  // Reply to PpapiMsg_LoadPlugin.
  IPC_MESSAGE_CONTROL1(PpapiHostMsg_PluginLoaded,
                       IPC::ChannelHandle /* handle */)

  // PPB_Core.
  IPC_MESSAGE_ROUTED1(PpapiHostMsg_PPBCore_AddRefResource, PP_Resource)
  IPC_MESSAGE_ROUTED1(PpapiHostMsg_PPBCore_ReleaseResource, PP_Resource)
  IPC_SYNC_MESSAGE_ROUTED0_1(PpapiHostMsg_PPBCore_GetTime,
                             double /* return value -> time */)
  IPC_SYNC_MESSAGE_ROUTED0_1(PpapiHostMsg_PPBCore_GetTimeTicks,
                             double /* return value -> time */)
  IPC_MESSAGE_ROUTED3(PpapiHostMsg_PPBCore_CallOnMainThread,
                      int /* delay_in_msec */,
                      uint32_t /* serialized_callback */,
                      int32_t /* result */)

  // PPB_Graphics2D.
  IPC_SYNC_MESSAGE_ROUTED3_1(PpapiHostMsg_PPBGraphics2D_Create,
                             PP_Module /* module */,
                             PP_Size /* size */,
                             bool /* is_always_opaque */,
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
  IPC_SYNC_MESSAGE_ROUTED2_1(PpapiHostMsg_PPBGraphics2D_Flush,
                             PP_Resource /* graphics_2d */,
                             uint32_t /* serialized_callback */,
                             int32_t /* result */)

  // PPB_ImageData.
  IPC_SYNC_MESSAGE_ROUTED0_1(
      PpapiHostMsg_PPBImageData_GetNativeImageDataFormat,
      int32 /* result_format */)
  IPC_SYNC_MESSAGE_ROUTED1_1(
      PpapiHostMsg_PPBImageData_IsImageDataFormatSupported,
      int32 /* format */,
      bool /* result */)
  IPC_SYNC_MESSAGE_ROUTED4_3(PpapiHostMsg_PPBImageData_Create,
                             PP_Module /* module */,
                             int32 /* format */,
                             PP_Size /* size */,
                             bool /* init_to_zero */,
                             PP_Resource /* result_resource */,
                             std::string /* image_data_desc */,
                             uint64_t /* result_shm_handle */)

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
                             bool /* result */)
  IPC_SYNC_MESSAGE_ROUTED1_1(PpapiHostMsg_PPBInstance_IsFullFrame,
                             PP_Instance /* instance */,
                             bool /* result */)
  IPC_SYNC_MESSAGE_ROUTED2_2(PpapiHostMsg_PPBInstance_ExecuteScript,
                             PP_Instance /* instance */,
                             pp::proxy::SerializedVar /* script */,
                             pp::proxy::SerializedVar /* out_exception */,
                             pp::proxy::SerializedVar /* result */)

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
  IPC_SYNC_MESSAGE_ROUTED1_3(PpapiHostMsg_PPBURLLoader_GetUploadProgress,
                             PP_Resource /* loader */,
                             int64 /* bytes_sent */,
                             int64 /* total_bytes_to_be_sent */,
                             bool /* result */)
  IPC_SYNC_MESSAGE_ROUTED1_3(PpapiHostMsg_PPBURLLoader_GetDownloadProgress,
                             PP_Resource /* loader */,
                             int64 /* bytes_received */,
                             int64 /* total_bytes_to_be_received */,
                             bool /* result */)
  IPC_SYNC_MESSAGE_ROUTED1_1(PpapiHostMsg_PPBURLLoader_GetResponseInfo,
                             PP_Resource /* loader */,
                             PP_Resource /* response_info_out */)
  IPC_MESSAGE_ROUTED3(PpapiHostMsg_PPBURLLoader_ReadResponseBody,
                      PP_Resource /* loader */,
                      int32_t /* bytes_to_read */,
                      uint32_t /* serialized_callback */)
  IPC_MESSAGE_ROUTED2(PpapiHostMsg_PPBURLLoader_FinishStreamingToFile,
                      PP_Resource /* loader */,
                      uint32_t /* serialized_callback */)
  IPC_MESSAGE_ROUTED1(PpapiHostMsg_PPBURLLoader_Close,
                      PP_Resource /* loader */)

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
                             bool /* result */)
  IPC_SYNC_MESSAGE_ROUTED2_2(PpapiHostMsg_PPBVar_HasMethodDeprecated,
                             pp::proxy::SerializedVar /* object */,
                             pp::proxy::SerializedVar /* method */,
                             pp::proxy::SerializedVar /* out_exception */,
                             bool /* result */)
  IPC_SYNC_MESSAGE_ROUTED2_2(PpapiHostMsg_PPBVar_GetProperty,
                             pp::proxy::SerializedVar /* object */,
                             pp::proxy::SerializedVar /* property */,
                             pp::proxy::SerializedVar /* out_exception */,
                             pp::proxy::SerializedVar /* result */)
  IPC_SYNC_MESSAGE_ROUTED2_2(PpapiHostMsg_PPBVar_DeleteProperty,
                             pp::proxy::SerializedVar /* object */,
                             pp::proxy::SerializedVar /* property */,
                             pp::proxy::SerializedVar /* out_exception */,
                             bool /* result */)
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
                             bool /* result */)
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
                             bool /* result */)
  IPC_SYNC_MESSAGE_ROUTED3_1(PpapiHostMsg_PPBVar_CreateObjectDeprecated,
                             PP_Module /* module */,
                             int64 /* object_class */,
                             int64 /* object_data */,
                             pp::proxy::SerializedVar /* result */)

IPC_END_MESSAGES(PpapiHost)

