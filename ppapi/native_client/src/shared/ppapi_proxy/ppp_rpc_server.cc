// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING
//
// Automatically generated code.  See srpcgen.py
//
// NaCl Simple Remote Procedure Call interface abstractions.

#include "untrusted/srpcgen/ppp_rpc.h"
#ifdef __native_client__
#ifndef UNREFERENCED_PARAMETER
#define UNREFERENCED_PARAMETER(P) do { (void) P; } while (0)
#endif  // UNREFERENCED_PARAMETER
#else
#include "native_client/src/include/portability.h"
#endif  // __native_client__
#include "native_client/src/shared/srpc/nacl_srpc.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_module.h"
#include "ppapi/c/pp_resource.h"

namespace {

static void RunCompletionCallbackDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  UNREFERENCED_PARAMETER(outputs);
  CompletionCallbackRpcServer::RunCompletionCallback(
      rpc,
      done,
      inputs[0]->u.ival,
      inputs[1]->u.ival,
      inputs[2]->u.count, inputs[2]->arrays.carr
  );
}

static void PPP_InitializeModuleDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  PppRpcServer::PPP_InitializeModule(
      rpc,
      done,
      inputs[0]->u.ival,
      inputs[1]->u.ival,
      inputs[2]->u.hval,
      inputs[3]->arrays.str,
      &(outputs[0]->u.ival),
      &(outputs[1]->u.ival)
  );
}

static void PPP_ShutdownModuleDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  UNREFERENCED_PARAMETER(inputs);
  UNREFERENCED_PARAMETER(outputs);
  PppRpcServer::PPP_ShutdownModule(
      rpc,
      done
  );
}

static void PPP_GetInterfaceDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  PppRpcServer::PPP_GetInterface(
      rpc,
      done,
      inputs[0]->arrays.str,
      &(outputs[0]->u.ival)
  );
}

static void PPP_Audio_StreamCreatedDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  UNREFERENCED_PARAMETER(outputs);
  PppAudioRpcServer::PPP_Audio_StreamCreated(
      rpc,
      done,
      inputs[0]->u.ival,
      inputs[1]->u.hval,
      inputs[2]->u.ival,
      inputs[3]->u.hval
  );
}

static void PPP_Find_StartFindDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  PppFindRpcServer::PPP_Find_StartFind(
      rpc,
      done,
      inputs[0]->u.ival,
      inputs[1]->u.count, inputs[1]->arrays.carr,
      inputs[2]->u.ival,
      &(outputs[0]->u.ival)
  );
}

static void PPP_Find_SelectFindResultDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  UNREFERENCED_PARAMETER(outputs);
  PppFindRpcServer::PPP_Find_SelectFindResult(
      rpc,
      done,
      inputs[0]->u.ival,
      inputs[1]->u.ival
  );
}

static void PPP_Find_StopFindDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  UNREFERENCED_PARAMETER(outputs);
  PppFindRpcServer::PPP_Find_StopFind(
      rpc,
      done,
      inputs[0]->u.ival
  );
}

static void PPP_InputEvent_HandleInputEventDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  PppInputEventRpcServer::PPP_InputEvent_HandleInputEvent(
      rpc,
      done,
      inputs[0]->u.ival,
      inputs[1]->u.ival,
      inputs[2]->u.count, inputs[2]->arrays.carr,
      inputs[3]->u.count, inputs[3]->arrays.carr,
      &(outputs[0]->u.ival)
  );
}

static void PPP_Instance_DidCreateDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  PppInstanceRpcServer::PPP_Instance_DidCreate(
      rpc,
      done,
      inputs[0]->u.ival,
      inputs[1]->u.ival,
      inputs[2]->u.count, inputs[2]->arrays.carr,
      inputs[3]->u.count, inputs[3]->arrays.carr,
      &(outputs[0]->u.ival)
  );
}

static void PPP_Instance_DidDestroyDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  UNREFERENCED_PARAMETER(outputs);
  PppInstanceRpcServer::PPP_Instance_DidDestroy(
      rpc,
      done,
      inputs[0]->u.ival
  );
}

static void PPP_Instance_DidChangeViewDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  UNREFERENCED_PARAMETER(outputs);
  PppInstanceRpcServer::PPP_Instance_DidChangeView(
      rpc,
      done,
      inputs[0]->u.ival,
      inputs[1]->u.count, inputs[1]->arrays.iarr,
      inputs[2]->u.count, inputs[2]->arrays.iarr
  );
}

static void PPP_Instance_DidChangeFocusDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  UNREFERENCED_PARAMETER(outputs);
  PppInstanceRpcServer::PPP_Instance_DidChangeFocus(
      rpc,
      done,
      inputs[0]->u.ival,
      inputs[1]->u.bval
  );
}

static void PPP_Instance_HandleDocumentLoadDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  PppInstanceRpcServer::PPP_Instance_HandleDocumentLoad(
      rpc,
      done,
      inputs[0]->u.ival,
      inputs[1]->u.ival,
      &(outputs[0]->u.ival)
  );
}

static void PPP_Messaging_HandleMessageDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  UNREFERENCED_PARAMETER(outputs);
  PppMessagingRpcServer::PPP_Messaging_HandleMessage(
      rpc,
      done,
      inputs[0]->u.ival,
      inputs[1]->u.count, inputs[1]->arrays.carr
  );
}

static void PPP_MouseLock_MouseLockLostDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  UNREFERENCED_PARAMETER(outputs);
  PppMouseLockRpcServer::PPP_MouseLock_MouseLockLost(
      rpc,
      done,
      inputs[0]->u.ival
  );
}

static void PPP_Printing_QuerySupportedFormatsDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  PppPrintingRpcServer::PPP_Printing_QuerySupportedFormats(
      rpc,
      done,
      inputs[0]->u.ival,
      &(outputs[0]->u.ival)
  );
}

static void PPP_Printing_BeginDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  PppPrintingRpcServer::PPP_Printing_Begin(
      rpc,
      done,
      inputs[0]->u.ival,
      inputs[1]->u.count, inputs[1]->arrays.carr,
      &(outputs[0]->u.ival)
  );
}

static void PPP_Printing_PrintPagesDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  PppPrintingRpcServer::PPP_Printing_PrintPages(
      rpc,
      done,
      inputs[0]->u.ival,
      inputs[1]->u.count, inputs[1]->arrays.carr,
      inputs[2]->u.ival,
      &(outputs[0]->u.ival)
  );
}

static void PPP_Printing_EndDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  UNREFERENCED_PARAMETER(outputs);
  PppPrintingRpcServer::PPP_Printing_End(
      rpc,
      done,
      inputs[0]->u.ival
  );
}

static void PPP_Scrollbar_ValueChangedDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  UNREFERENCED_PARAMETER(outputs);
  PppScrollbarRpcServer::PPP_Scrollbar_ValueChanged(
      rpc,
      done,
      inputs[0]->u.ival,
      inputs[1]->u.ival,
      inputs[2]->u.ival
  );
}

static void PPP_Scrollbar_OverlayChangedDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  UNREFERENCED_PARAMETER(outputs);
  PppScrollbarRpcServer::PPP_Scrollbar_OverlayChanged(
      rpc,
      done,
      inputs[0]->u.ival,
      inputs[1]->u.ival,
      inputs[2]->u.ival
  );
}

static void PPP_Selection_GetSelectedTextDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  PppSelectionRpcServer::PPP_Selection_GetSelectedText(
      rpc,
      done,
      inputs[0]->u.ival,
      inputs[1]->u.ival,
      &(outputs[0]->u.count), outputs[0]->arrays.carr
  );
}

static void PPP_Widget_InvalidateDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  UNREFERENCED_PARAMETER(outputs);
  PppWidgetRpcServer::PPP_Widget_Invalidate(
      rpc,
      done,
      inputs[0]->u.ival,
      inputs[1]->u.ival,
      inputs[2]->u.count, inputs[2]->arrays.carr
  );
}

static void PPP_Zoom_ZoomDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  UNREFERENCED_PARAMETER(outputs);
  PppZoomRpcServer::PPP_Zoom_Zoom(
      rpc,
      done,
      inputs[0]->u.ival,
      inputs[1]->u.dval,
      inputs[2]->u.ival
  );
}

}  // namespace

NaClSrpcHandlerDesc PppRpcs::srpc_methods[] = {
  { "RunCompletionCallback:iiC:", RunCompletionCallbackDispatcher },
  { "PPP_InitializeModule:iihs:ii", PPP_InitializeModuleDispatcher },
  { "PPP_ShutdownModule::", PPP_ShutdownModuleDispatcher },
  { "PPP_GetInterface:s:i", PPP_GetInterfaceDispatcher },
  { "PPP_Audio_StreamCreated:ihih:", PPP_Audio_StreamCreatedDispatcher },
  { "PPP_Find_StartFind:iCi:i", PPP_Find_StartFindDispatcher },
  { "PPP_Find_SelectFindResult:ii:", PPP_Find_SelectFindResultDispatcher },
  { "PPP_Find_StopFind:i:", PPP_Find_StopFindDispatcher },
  { "PPP_InputEvent_HandleInputEvent:iiCC:i", PPP_InputEvent_HandleInputEventDispatcher },
  { "PPP_Instance_DidCreate:iiCC:i", PPP_Instance_DidCreateDispatcher },
  { "PPP_Instance_DidDestroy:i:", PPP_Instance_DidDestroyDispatcher },
  { "PPP_Instance_DidChangeView:iII:", PPP_Instance_DidChangeViewDispatcher },
  { "PPP_Instance_DidChangeFocus:ib:", PPP_Instance_DidChangeFocusDispatcher },
  { "PPP_Instance_HandleDocumentLoad:ii:i", PPP_Instance_HandleDocumentLoadDispatcher },
  { "PPP_Messaging_HandleMessage:iC:", PPP_Messaging_HandleMessageDispatcher },
  { "PPP_MouseLock_MouseLockLost:i:", PPP_MouseLock_MouseLockLostDispatcher },
  { "PPP_Printing_QuerySupportedFormats:i:i", PPP_Printing_QuerySupportedFormatsDispatcher },
  { "PPP_Printing_Begin:iC:i", PPP_Printing_BeginDispatcher },
  { "PPP_Printing_PrintPages:iCi:i", PPP_Printing_PrintPagesDispatcher },
  { "PPP_Printing_End:i:", PPP_Printing_EndDispatcher },
  { "PPP_Scrollbar_ValueChanged:iii:", PPP_Scrollbar_ValueChangedDispatcher },
  { "PPP_Scrollbar_OverlayChanged:iii:", PPP_Scrollbar_OverlayChangedDispatcher },
  { "PPP_Selection_GetSelectedText:ii:C", PPP_Selection_GetSelectedTextDispatcher },
  { "PPP_Widget_Invalidate:iiC:", PPP_Widget_InvalidateDispatcher },
  { "PPP_Zoom_Zoom:idi:", PPP_Zoom_ZoomDispatcher },
  { NULL, NULL }
};

