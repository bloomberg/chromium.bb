// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING
//
// Automatically generated code.  See srpcgen.py
//
// NaCl Simple Remote Procedure Call interface abstractions.

#include "trusted/srpcgen/ppp_rpc.h"
#ifdef __native_client__
#ifndef UNREFERENCED_PARAMETER
#define UNREFERENCED_PARAMETER(P) do { (void) P; } while (0)
#endif  // UNREFERENCED_PARAMETER
#else
#include "native_client/src/include/portability.h"
#endif  // __native_client__
#include "native_client/src/shared/ppapi_proxy/browser_globals.h"
#include "native_client/src/shared/srpc/nacl_srpc.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_module.h"
#include "ppapi/c/pp_resource.h"

NaClSrpcError CompletionCallbackRpcClient::RunCompletionCallback(
    NaClSrpcChannel* channel,
    int32_t callback_id,
    int32_t result,
    nacl_abi_size_t read_buffer_bytes, char* read_buffer)  {
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "RunCompletionCallback:iiC:",
      callback_id,
      result,
      read_buffer_bytes, read_buffer
  );
  return retval;
}

NaClSrpcError PppRpcClient::PPP_InitializeModule(
    NaClSrpcChannel* channel,
    int32_t pid,
    PP_Module module,
    NaClSrpcImcDescType upcall_channel_desc,
    char* service_description,
    int32_t* nacl_pid,
    int32_t* success)  {
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPP_InitializeModule:iihs:ii",
      pid,
      module,
      upcall_channel_desc,
      service_description,
      nacl_pid,
      success
  );
  return retval;
}

NaClSrpcError PppRpcClient::PPP_ShutdownModule(
    NaClSrpcChannel* channel)  {
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPP_ShutdownModule::"
  );
  return retval;
}

NaClSrpcError PppRpcClient::PPP_GetInterface(
    NaClSrpcChannel* channel,
    char* interface_name,
    int32_t* exports_interface_name)  {
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPP_GetInterface:s:i",
      interface_name,
      exports_interface_name
  );
  return retval;
}

NaClSrpcError PppAudioRpcClient::PPP_Audio_StreamCreated(
    NaClSrpcChannel* channel,
    PP_Instance instance,
    NaClSrpcImcDescType out_shm,
    int32_t out_shm_size,
    NaClSrpcImcDescType out_socket)  {
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPP_Audio_StreamCreated:ihih:",
      instance,
      out_shm,
      out_shm_size,
      out_socket
  );
  if (retval == NACL_SRPC_RESULT_INTERNAL)
    ppapi_proxy::CleanUpAfterDeadNexe(instance);
  return retval;
}

NaClSrpcError PppFindRpcClient::PPP_Find_StartFind(
    NaClSrpcChannel* channel,
    PP_Instance instance,
    nacl_abi_size_t text_bytes, char* text,
    int32_t case_sensitive,
    int32_t* supports_find)  {
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPP_Find_StartFind:iCi:i",
      instance,
      text_bytes, text,
      case_sensitive,
      supports_find
  );
  if (retval == NACL_SRPC_RESULT_INTERNAL)
    ppapi_proxy::CleanUpAfterDeadNexe(instance);
  return retval;
}

NaClSrpcError PppFindRpcClient::PPP_Find_SelectFindResult(
    NaClSrpcChannel* channel,
    PP_Instance instance,
    int32_t forward)  {
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPP_Find_SelectFindResult:ii:",
      instance,
      forward
  );
  if (retval == NACL_SRPC_RESULT_INTERNAL)
    ppapi_proxy::CleanUpAfterDeadNexe(instance);
  return retval;
}

NaClSrpcError PppFindRpcClient::PPP_Find_StopFind(
    NaClSrpcChannel* channel,
    PP_Instance instance)  {
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPP_Find_StopFind:i:",
      instance
  );
  if (retval == NACL_SRPC_RESULT_INTERNAL)
    ppapi_proxy::CleanUpAfterDeadNexe(instance);
  return retval;
}

NaClSrpcError PppInputEventRpcClient::PPP_InputEvent_HandleInputEvent(
    NaClSrpcChannel* channel,
    PP_Instance instance,
    PP_Resource resource,
    nacl_abi_size_t event_data_bytes, char* event_data,
    nacl_abi_size_t character_text_bytes, char* character_text,
    int32_t* handled)  {
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPP_InputEvent_HandleInputEvent:iiCC:i",
      instance,
      resource,
      event_data_bytes, event_data,
      character_text_bytes, character_text,
      handled
  );
  if (retval == NACL_SRPC_RESULT_INTERNAL)
    ppapi_proxy::CleanUpAfterDeadNexe(instance);
  return retval;
}

NaClSrpcError PppInstanceRpcClient::PPP_Instance_DidCreate(
    NaClSrpcChannel* channel,
    PP_Instance instance,
    int32_t argc,
    nacl_abi_size_t argn_bytes, char* argn,
    nacl_abi_size_t argv_bytes, char* argv,
    int32_t* success)  {
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPP_Instance_DidCreate:iiCC:i",
      instance,
      argc,
      argn_bytes, argn,
      argv_bytes, argv,
      success
  );
  if (retval == NACL_SRPC_RESULT_INTERNAL)
    ppapi_proxy::CleanUpAfterDeadNexe(instance);
  return retval;
}

NaClSrpcError PppInstanceRpcClient::PPP_Instance_DidDestroy(
    NaClSrpcChannel* channel,
    PP_Instance instance)  {
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPP_Instance_DidDestroy:i:",
      instance
  );
  if (retval == NACL_SRPC_RESULT_INTERNAL)
    ppapi_proxy::CleanUpAfterDeadNexe(instance);
  return retval;
}

NaClSrpcError PppInstanceRpcClient::PPP_Instance_DidChangeView(
    NaClSrpcChannel* channel,
    PP_Instance instance,
    nacl_abi_size_t position_bytes, int32_t* position,
    nacl_abi_size_t clip_bytes, int32_t* clip)  {
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPP_Instance_DidChangeView:iII:",
      instance,
      position_bytes, position,
      clip_bytes, clip
  );
  if (retval == NACL_SRPC_RESULT_INTERNAL)
    ppapi_proxy::CleanUpAfterDeadNexe(instance);
  return retval;
}

NaClSrpcError PppInstanceRpcClient::PPP_Instance_DidChangeFocus(
    NaClSrpcChannel* channel,
    PP_Instance instance,
    bool has_focus)  {
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPP_Instance_DidChangeFocus:ib:",
      instance,
      has_focus
  );
  if (retval == NACL_SRPC_RESULT_INTERNAL)
    ppapi_proxy::CleanUpAfterDeadNexe(instance);
  return retval;
}

NaClSrpcError PppInstanceRpcClient::PPP_Instance_HandleDocumentLoad(
    NaClSrpcChannel* channel,
    PP_Instance instance,
    PP_Resource url_loader,
    int32_t* success)  {
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPP_Instance_HandleDocumentLoad:ii:i",
      instance,
      url_loader,
      success
  );
  if (retval == NACL_SRPC_RESULT_INTERNAL)
    ppapi_proxy::CleanUpAfterDeadNexe(instance);
  return retval;
}

NaClSrpcError PppMessagingRpcClient::PPP_Messaging_HandleMessage(
    NaClSrpcChannel* channel,
    PP_Instance instance,
    nacl_abi_size_t message_bytes, char* message)  {
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPP_Messaging_HandleMessage:iC:",
      instance,
      message_bytes, message
  );
  if (retval == NACL_SRPC_RESULT_INTERNAL)
    ppapi_proxy::CleanUpAfterDeadNexe(instance);
  return retval;
}

NaClSrpcError PppPrintingRpcClient::PPP_Printing_QuerySupportedFormats(
    NaClSrpcChannel* channel,
    PP_Instance instance,
    int32_t* formats)  {
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPP_Printing_QuerySupportedFormats:i:i",
      instance,
      formats
  );
  if (retval == NACL_SRPC_RESULT_INTERNAL)
    ppapi_proxy::CleanUpAfterDeadNexe(instance);
  return retval;
}

NaClSrpcError PppPrintingRpcClient::PPP_Printing_Begin(
    NaClSrpcChannel* channel,
    PP_Instance instance,
    nacl_abi_size_t print_settings_bytes, char* print_settings,
    int32_t* pages_required)  {
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPP_Printing_Begin:iC:i",
      instance,
      print_settings_bytes, print_settings,
      pages_required
  );
  if (retval == NACL_SRPC_RESULT_INTERNAL)
    ppapi_proxy::CleanUpAfterDeadNexe(instance);
  return retval;
}

NaClSrpcError PppPrintingRpcClient::PPP_Printing_PrintPages(
    NaClSrpcChannel* channel,
    PP_Instance instance,
    nacl_abi_size_t page_ranges_bytes, char* page_ranges,
    int32_t page_range_count,
    PP_Resource* image_data)  {
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPP_Printing_PrintPages:iCi:i",
      instance,
      page_ranges_bytes, page_ranges,
      page_range_count,
      image_data
  );
  if (retval == NACL_SRPC_RESULT_INTERNAL)
    ppapi_proxy::CleanUpAfterDeadNexe(instance);
  return retval;
}

NaClSrpcError PppPrintingRpcClient::PPP_Printing_End(
    NaClSrpcChannel* channel,
    PP_Instance instance)  {
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPP_Printing_End:i:",
      instance
  );
  if (retval == NACL_SRPC_RESULT_INTERNAL)
    ppapi_proxy::CleanUpAfterDeadNexe(instance);
  return retval;
}

NaClSrpcError PppScrollbarRpcClient::PPP_Scrollbar_ValueChanged(
    NaClSrpcChannel* channel,
    PP_Instance instance,
    PP_Resource scrollbar,
    int32_t value)  {
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPP_Scrollbar_ValueChanged:iii:",
      instance,
      scrollbar,
      value
  );
  if (retval == NACL_SRPC_RESULT_INTERNAL)
    ppapi_proxy::CleanUpAfterDeadNexe(instance);
  return retval;
}

NaClSrpcError PppScrollbarRpcClient::PPP_Scrollbar_OverlayChanged(
    NaClSrpcChannel* channel,
    PP_Instance instance,
    PP_Resource scrollbar,
    int32_t overlay)  {
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPP_Scrollbar_OverlayChanged:iii:",
      instance,
      scrollbar,
      overlay
  );
  if (retval == NACL_SRPC_RESULT_INTERNAL)
    ppapi_proxy::CleanUpAfterDeadNexe(instance);
  return retval;
}

NaClSrpcError PppSelectionRpcClient::PPP_Selection_GetSelectedText(
    NaClSrpcChannel* channel,
    PP_Instance instance,
    int32_t html,
    nacl_abi_size_t* selected_text_bytes, char* selected_text)  {
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPP_Selection_GetSelectedText:ii:C",
      instance,
      html,
      selected_text_bytes, selected_text
  );
  if (retval == NACL_SRPC_RESULT_INTERNAL)
    ppapi_proxy::CleanUpAfterDeadNexe(instance);
  return retval;
}

NaClSrpcError PppWidgetRpcClient::PPP_Widget_Invalidate(
    NaClSrpcChannel* channel,
    PP_Instance instance,
    PP_Resource widget,
    nacl_abi_size_t dirty_rect_bytes, char* dirty_rect)  {
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPP_Widget_Invalidate:iiC:",
      instance,
      widget,
      dirty_rect_bytes, dirty_rect
  );
  if (retval == NACL_SRPC_RESULT_INTERNAL)
    ppapi_proxy::CleanUpAfterDeadNexe(instance);
  return retval;
}

NaClSrpcError PppZoomRpcClient::PPP_Zoom_Zoom(
    NaClSrpcChannel* channel,
    PP_Instance instance,
    double factor,
    int32_t text_only)  {
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPP_Zoom_Zoom:idi:",
      instance,
      factor,
      text_only
  );
  if (retval == NACL_SRPC_RESULT_INTERNAL)
    ppapi_proxy::CleanUpAfterDeadNexe(instance);
  return retval;
}


