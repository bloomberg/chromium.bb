// Copyright (c) 2011 The Chromium Authors. All rights reserved.
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

class CompletionCallbackRpcServer {
 public:
  static void RunCompletionCallback(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      int32_t callback_id,
      int32_t result,
      nacl_abi_size_t read_buffer_bytes, char* read_buffer);

 private:
  CompletionCallbackRpcServer();
  CompletionCallbackRpcServer(const CompletionCallbackRpcServer&);
  void operator=(const CompletionCallbackRpcServer);
};  // class CompletionCallbackRpcServer

class PppRpcServer {
 public:
  static void PPP_InitializeModule(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      int32_t pid,
      PP_Module module,
      NaClSrpcImcDescType upcall_channel_desc,
      char* service_description,
      int32_t* nacl_pid,
      int32_t* success);
  static void PPP_ShutdownModule(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done);
  static void PPP_GetInterface(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      char* interface_name,
      int32_t* exports_interface_name);

 private:
  PppRpcServer();
  PppRpcServer(const PppRpcServer&);
  void operator=(const PppRpcServer);
};  // class PppRpcServer

class PppAudioRpcServer {
 public:
  static void PPP_Audio_StreamCreated(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      PP_Instance instance,
      NaClSrpcImcDescType out_shm,
      int32_t out_shm_size,
      NaClSrpcImcDescType out_socket);

 private:
  PppAudioRpcServer();
  PppAudioRpcServer(const PppAudioRpcServer&);
  void operator=(const PppAudioRpcServer);
};  // class PppAudioRpcServer

class PppFindRpcServer {
 public:
  static void PPP_Find_StartFind(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      PP_Instance instance,
      nacl_abi_size_t text_bytes, char* text,
      int32_t case_sensitive,
      int32_t* supports_find);
  static void PPP_Find_SelectFindResult(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      PP_Instance instance,
      int32_t forward);
  static void PPP_Find_StopFind(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      PP_Instance instance);

 private:
  PppFindRpcServer();
  PppFindRpcServer(const PppFindRpcServer&);
  void operator=(const PppFindRpcServer);
};  // class PppFindRpcServer

class PppInputEventRpcServer {
 public:
  static void PPP_InputEvent_HandleInputEvent(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      PP_Instance instance,
      PP_Resource resource,
      nacl_abi_size_t event_data_bytes, char* event_data,
      nacl_abi_size_t character_text_bytes, char* character_text,
      int32_t* handled);

 private:
  PppInputEventRpcServer();
  PppInputEventRpcServer(const PppInputEventRpcServer&);
  void operator=(const PppInputEventRpcServer);
};  // class PppInputEventRpcServer

class PppInstanceRpcServer {
 public:
  static void PPP_Instance_DidCreate(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      PP_Instance instance,
      int32_t argc,
      nacl_abi_size_t argn_bytes, char* argn,
      nacl_abi_size_t argv_bytes, char* argv,
      int32_t* success);
  static void PPP_Instance_DidDestroy(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      PP_Instance instance);
  static void PPP_Instance_DidChangeView(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      PP_Instance instance,
      nacl_abi_size_t position_bytes, int32_t* position,
      nacl_abi_size_t clip_bytes, int32_t* clip);
  static void PPP_Instance_DidChangeFocus(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      PP_Instance instance,
      bool has_focus);
  static void PPP_Instance_HandleDocumentLoad(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      PP_Instance instance,
      PP_Resource url_loader,
      int32_t* success);

 private:
  PppInstanceRpcServer();
  PppInstanceRpcServer(const PppInstanceRpcServer&);
  void operator=(const PppInstanceRpcServer);
};  // class PppInstanceRpcServer

class PppMessagingRpcServer {
 public:
  static void PPP_Messaging_HandleMessage(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      PP_Instance instance,
      nacl_abi_size_t message_bytes, char* message);

 private:
  PppMessagingRpcServer();
  PppMessagingRpcServer(const PppMessagingRpcServer&);
  void operator=(const PppMessagingRpcServer);
};  // class PppMessagingRpcServer

class PppMouseLockRpcServer {
 public:
  static void PPP_MouseLock_MouseLockLost(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      PP_Instance instance);

 private:
  PppMouseLockRpcServer();
  PppMouseLockRpcServer(const PppMouseLockRpcServer&);
  void operator=(const PppMouseLockRpcServer);
};  // class PppMouseLockRpcServer

class PppPrintingRpcServer {
 public:
  static void PPP_Printing_QuerySupportedFormats(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      PP_Instance instance,
      int32_t* formats);
  static void PPP_Printing_Begin(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      PP_Instance instance,
      nacl_abi_size_t print_settings_bytes, char* print_settings,
      int32_t* pages_required);
  static void PPP_Printing_PrintPages(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      PP_Instance instance,
      nacl_abi_size_t page_ranges_bytes, char* page_ranges,
      int32_t page_range_count,
      PP_Resource* image_data);
  static void PPP_Printing_End(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      PP_Instance instance);

 private:
  PppPrintingRpcServer();
  PppPrintingRpcServer(const PppPrintingRpcServer&);
  void operator=(const PppPrintingRpcServer);
};  // class PppPrintingRpcServer

class PppScrollbarRpcServer {
 public:
  static void PPP_Scrollbar_ValueChanged(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      PP_Instance instance,
      PP_Resource scrollbar,
      int32_t value);
  static void PPP_Scrollbar_OverlayChanged(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      PP_Instance instance,
      PP_Resource scrollbar,
      int32_t overlay);

 private:
  PppScrollbarRpcServer();
  PppScrollbarRpcServer(const PppScrollbarRpcServer&);
  void operator=(const PppScrollbarRpcServer);
};  // class PppScrollbarRpcServer

class PppSelectionRpcServer {
 public:
  static void PPP_Selection_GetSelectedText(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      PP_Instance instance,
      int32_t html,
      nacl_abi_size_t* selected_text_bytes, char* selected_text);

 private:
  PppSelectionRpcServer();
  PppSelectionRpcServer(const PppSelectionRpcServer&);
  void operator=(const PppSelectionRpcServer);
};  // class PppSelectionRpcServer

class PppWidgetRpcServer {
 public:
  static void PPP_Widget_Invalidate(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      PP_Instance instance,
      PP_Resource widget,
      nacl_abi_size_t dirty_rect_bytes, char* dirty_rect);

 private:
  PppWidgetRpcServer();
  PppWidgetRpcServer(const PppWidgetRpcServer&);
  void operator=(const PppWidgetRpcServer);
};  // class PppWidgetRpcServer

class PppZoomRpcServer {
 public:
  static void PPP_Zoom_Zoom(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      PP_Instance instance,
      double factor,
      int32_t text_only);

 private:
  PppZoomRpcServer();
  PppZoomRpcServer(const PppZoomRpcServer&);
  void operator=(const PppZoomRpcServer);
};  // class PppZoomRpcServer

class PppRpcs {
 public:
  static NaClSrpcHandlerDesc srpc_methods[];
};  // class PppRpcs


#endif  // GEN_PPAPI_PROXY_PPP_RPC_H_

