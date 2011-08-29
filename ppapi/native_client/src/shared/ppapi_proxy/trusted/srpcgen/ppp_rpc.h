// Copyright (c) 2011 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING
//
// Automatically generated code.  See srpcgen.py
//
// NaCl Simple Remote Procedure Call interface abstractions.

#ifndef GEN_PPAPI_PROXY_PPP_RPC_H_
#define GEN_PPAPI_PROXY_PPP_RPC_H_

#ifndef __native_client__
#include "native_client/src/include/portability.h"
#endif  // __native_client__
#include "native_client/src/shared/srpc/nacl_srpc.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_module.h"
#include "ppapi/c/pp_resource.h"

class CompletionCallbackRpcClient {
 public:
  static NaClSrpcError RunCompletionCallback(
      NaClSrpcChannel* channel,
      int32_t callback_id,
      int32_t result,
      nacl_abi_size_t read_buffer_bytes, char* read_buffer);

 private:
  CompletionCallbackRpcClient();
  CompletionCallbackRpcClient(const CompletionCallbackRpcClient&);
  void operator=(const CompletionCallbackRpcClient);
};  // class CompletionCallbackRpcClient

class PppRpcClient {
 public:
  static NaClSrpcError PPP_InitializeModule(
      NaClSrpcChannel* channel,
      int32_t pid,
      PP_Module module,
      NaClSrpcImcDescType upcall_channel_desc,
      char* service_description,
      int32_t* nacl_pid,
      int32_t* success);
  static NaClSrpcError PPP_ShutdownModule(
      NaClSrpcChannel* channel);
  static NaClSrpcError PPP_GetInterface(
      NaClSrpcChannel* channel,
      char* interface_name,
      int32_t* exports_interface_name);

 private:
  PppRpcClient();
  PppRpcClient(const PppRpcClient&);
  void operator=(const PppRpcClient);
};  // class PppRpcClient

class PppAudioRpcClient {
 public:
  static NaClSrpcError PPP_Audio_StreamCreated(
      NaClSrpcChannel* channel,
      PP_Instance instance,
      NaClSrpcImcDescType out_shm,
      int32_t out_shm_size,
      NaClSrpcImcDescType out_socket);

 private:
  PppAudioRpcClient();
  PppAudioRpcClient(const PppAudioRpcClient&);
  void operator=(const PppAudioRpcClient);
};  // class PppAudioRpcClient

class PppFindRpcClient {
 public:
  static NaClSrpcError PPP_Find_StartFind(
      NaClSrpcChannel* channel,
      PP_Instance instance,
      nacl_abi_size_t text_bytes, char* text,
      int32_t case_sensitive,
      int32_t* supports_find);
  static NaClSrpcError PPP_Find_SelectFindResult(
      NaClSrpcChannel* channel,
      PP_Instance instance,
      int32_t forward);
  static NaClSrpcError PPP_Find_StopFind(
      NaClSrpcChannel* channel,
      PP_Instance instance);

 private:
  PppFindRpcClient();
  PppFindRpcClient(const PppFindRpcClient&);
  void operator=(const PppFindRpcClient);
};  // class PppFindRpcClient

class PppInputEventRpcClient {
 public:
  static NaClSrpcError PPP_InputEvent_HandleInputEvent(
      NaClSrpcChannel* channel,
      PP_Instance instance,
      PP_Resource resource,
      nacl_abi_size_t event_data_bytes, char* event_data,
      nacl_abi_size_t character_text_bytes, char* character_text,
      int32_t* handled);

 private:
  PppInputEventRpcClient();
  PppInputEventRpcClient(const PppInputEventRpcClient&);
  void operator=(const PppInputEventRpcClient);
};  // class PppInputEventRpcClient

class PppInstanceRpcClient {
 public:
  static NaClSrpcError PPP_Instance_DidCreate(
      NaClSrpcChannel* channel,
      PP_Instance instance,
      int32_t argc,
      nacl_abi_size_t argn_bytes, char* argn,
      nacl_abi_size_t argv_bytes, char* argv,
      int32_t* success);
  static NaClSrpcError PPP_Instance_DidDestroy(
      NaClSrpcChannel* channel,
      PP_Instance instance);
  static NaClSrpcError PPP_Instance_DidChangeView(
      NaClSrpcChannel* channel,
      PP_Instance instance,
      nacl_abi_size_t position_bytes, int32_t* position,
      nacl_abi_size_t clip_bytes, int32_t* clip);
  static NaClSrpcError PPP_Instance_DidChangeFocus(
      NaClSrpcChannel* channel,
      PP_Instance instance,
      bool has_focus);
  static NaClSrpcError PPP_Instance_HandleDocumentLoad(
      NaClSrpcChannel* channel,
      PP_Instance instance,
      PP_Resource url_loader,
      int32_t* success);

 private:
  PppInstanceRpcClient();
  PppInstanceRpcClient(const PppInstanceRpcClient&);
  void operator=(const PppInstanceRpcClient);
};  // class PppInstanceRpcClient

class PppMessagingRpcClient {
 public:
  static NaClSrpcError PPP_Messaging_HandleMessage(
      NaClSrpcChannel* channel,
      PP_Instance instance,
      nacl_abi_size_t message_bytes, char* message);

 private:
  PppMessagingRpcClient();
  PppMessagingRpcClient(const PppMessagingRpcClient&);
  void operator=(const PppMessagingRpcClient);
};  // class PppMessagingRpcClient

class PppPrintingRpcClient {
 public:
  static NaClSrpcError PPP_Printing_QuerySupportedFormats(
      NaClSrpcChannel* channel,
      PP_Instance instance,
      int32_t* formats);
  static NaClSrpcError PPP_Printing_Begin(
      NaClSrpcChannel* channel,
      PP_Instance instance,
      nacl_abi_size_t print_settings_bytes, char* print_settings,
      int32_t* pages_required);
  static NaClSrpcError PPP_Printing_PrintPages(
      NaClSrpcChannel* channel,
      PP_Instance instance,
      nacl_abi_size_t page_ranges_bytes, char* page_ranges,
      int32_t page_range_count,
      PP_Resource* image_data);
  static NaClSrpcError PPP_Printing_End(
      NaClSrpcChannel* channel,
      PP_Instance instance);

 private:
  PppPrintingRpcClient();
  PppPrintingRpcClient(const PppPrintingRpcClient&);
  void operator=(const PppPrintingRpcClient);
};  // class PppPrintingRpcClient

class PppScrollbarRpcClient {
 public:
  static NaClSrpcError PPP_Scrollbar_ValueChanged(
      NaClSrpcChannel* channel,
      PP_Instance instance,
      PP_Resource scrollbar,
      int32_t value);
  static NaClSrpcError PPP_Scrollbar_OverlayChanged(
      NaClSrpcChannel* channel,
      PP_Instance instance,
      PP_Resource scrollbar,
      int32_t overlay);

 private:
  PppScrollbarRpcClient();
  PppScrollbarRpcClient(const PppScrollbarRpcClient&);
  void operator=(const PppScrollbarRpcClient);
};  // class PppScrollbarRpcClient

class PppSelectionRpcClient {
 public:
  static NaClSrpcError PPP_Selection_GetSelectedText(
      NaClSrpcChannel* channel,
      PP_Instance instance,
      int32_t html,
      nacl_abi_size_t* selected_text_bytes, char* selected_text);

 private:
  PppSelectionRpcClient();
  PppSelectionRpcClient(const PppSelectionRpcClient&);
  void operator=(const PppSelectionRpcClient);
};  // class PppSelectionRpcClient

class PppWidgetRpcClient {
 public:
  static NaClSrpcError PPP_Widget_Invalidate(
      NaClSrpcChannel* channel,
      PP_Instance instance,
      PP_Resource widget,
      nacl_abi_size_t dirty_rect_bytes, char* dirty_rect);

 private:
  PppWidgetRpcClient();
  PppWidgetRpcClient(const PppWidgetRpcClient&);
  void operator=(const PppWidgetRpcClient);
};  // class PppWidgetRpcClient

class PppZoomRpcClient {
 public:
  static NaClSrpcError PPP_Zoom_Zoom(
      NaClSrpcChannel* channel,
      PP_Instance instance,
      double factor,
      int32_t text_only);

 private:
  PppZoomRpcClient();
  PppZoomRpcClient(const PppZoomRpcClient&);
  void operator=(const PppZoomRpcClient);
};  // class PppZoomRpcClient




#endif  // GEN_PPAPI_PROXY_PPP_RPC_H_

