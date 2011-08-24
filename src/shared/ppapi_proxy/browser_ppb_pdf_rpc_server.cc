// Copyright (c) 2011 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// SRPC-abstraction wrappers around PPB_PDF functions.

#include <stdlib.h>
#include <string.h>
#include <algorithm>

#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/include/nacl_scoped_ptr.h"
#include "native_client/src/include/portability.h"
#include "native_client/src/shared/ppapi_proxy/browser_globals.h"
#include "native_client/src/shared/ppapi_proxy/object_serialize.h"
#include "native_client/src/shared/ppapi_proxy/utility.h"
#include "native_client/src/trusted/desc/nacl_desc_wrapper.h"
#include "ppapi/c/dev/ppb_memory_dev.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/pp_rect.h"
#include "ppapi/c/ppb_image_data.h"
#include "ppapi/c/private/ppb_pdf.h"
#include "srpcgen/ppb_rpc.h"

using ppapi_proxy::DebugPrintf;
using ppapi_proxy::PPBCoreInterface;
using ppapi_proxy::PPBPDFInterface;
using ppapi_proxy::SerializeTo;
using ppapi_proxy::DeserializeTo;

const nacl_abi_size_t kPpbPrivateFindResultBytes =
    static_cast<nacl_abi_size_t>(sizeof(PP_PrivateFindResult));
// Limit the maximum result buffer size.
const nacl_abi_size_t kMaxFindResultsBytes = 65536;
const int32_t kMaxFindResults =
    kMaxFindResultsBytes / kPpbPrivateFindResultBytes;

void PpbPdfRpcServer::PPB_PDF_GetLocalizedString(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    PP_Instance instance,
    int32_t string_id,
    nacl_abi_size_t* string_size, char* string) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;

  PP_Var pp_string =
      PPBPDFInterface()->GetLocalizedString(
          instance,
          static_cast<PP_ResourceString>(string_id));

  DebugPrintf("PPB_PDF::GetLocalizedString: "
              "string_id=%"NACL_PRId32"\n", string_id);
  if (!SerializeTo(&pp_string, string, string_size))
    return;
  rpc->result = NACL_SRPC_RESULT_OK;
}

void PpbPdfRpcServer::PPB_PDF_GetResourceImage(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    PP_Instance instance,
    int32_t image_id,
    PP_Resource* resource) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;

  *resource = PPBPDFInterface()->GetResourceImage(
      instance,
      static_cast<PP_ResourceImage>(image_id));

  DebugPrintf("PPB_PDF::GetResourceImage: image_id=%"NACL_PRId32"\n", image_id);
  rpc->result = NACL_SRPC_RESULT_OK;
}

void PpbPdfRpcServer::PPB_PDF_GetFontFileWithFallback(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    PP_Instance instance,
    nacl_abi_size_t description_size, char* description,
    nacl_abi_size_t face_size, char* face,
    int32_t charset,
    PP_Resource* resource) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;

  UNREFERENCED_PARAMETER(instance);
  UNREFERENCED_PARAMETER(description_size);
  UNREFERENCED_PARAMETER(description);
  UNREFERENCED_PARAMETER(face_size);
  UNREFERENCED_PARAMETER(face);
  UNREFERENCED_PARAMETER(charset);

  *resource = ppapi_proxy::kInvalidResourceId;
  DebugPrintf("PPB_PDF::GetFontFileWithFallback: not implemented.\n");
  rpc->result = NACL_SRPC_RESULT_OK;
}

void PpbPdfRpcServer::PPB_PDF_GetFontTableForPrivateFontFile(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    PP_Resource font_file,
    int32_t table,
    nacl_abi_size_t* output_size, char* output,
    int32_t* success) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;

  UNREFERENCED_PARAMETER(font_file);
  UNREFERENCED_PARAMETER(table);
  UNREFERENCED_PARAMETER(output);

  *output_size = 0;
  *success = 0;

  DebugPrintf("PPB_PDF::GetFontTableForPrivateFontFile: not implemented.\n");
  rpc->result = NACL_SRPC_RESULT_OK;
}

void PpbPdfRpcServer::PPB_PDF_SearchString(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    PP_Instance instance,
    nacl_abi_size_t string_size, char* string,
    nacl_abi_size_t term_size, char* term,
    int32_t case_sensitive,
    nacl_abi_size_t* results_size, char* results,
    int32_t* count) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;

  UNREFERENCED_PARAMETER(string_size);
  UNREFERENCED_PARAMETER(term_size);
  struct PP_PrivateFindResult* pp_results = NULL;
  int pp_result_count = 0;
  PPBPDFInterface()->SearchString(
      instance,
      reinterpret_cast<unsigned short*>(string),
      reinterpret_cast<unsigned short*>(term),
      case_sensitive ? PP_TRUE : PP_FALSE,
      &pp_results,
      &pp_result_count);
  pp_result_count = std::min(pp_result_count, kMaxFindResults);
  *results_size = std::min(*results_size,
                           pp_result_count * kPpbPrivateFindResultBytes);
  if (pp_results) {
    memcpy(results, pp_results, *results_size);
    // On Windows, the call to |free| must be made in the renderer's .exe,
    // not in the .dll code. That can cause a crash in some configurations.
    ppapi_proxy::PPBMemoryInterface()->MemFree(pp_results);
  }
  *count = static_cast<int32_t>(pp_result_count);

  DebugPrintf("PPB_PDF::SearchString: count=%"NACL_PRId32"\n", *count);
  rpc->result = NACL_SRPC_RESULT_OK;
}

void PpbPdfRpcServer::PPB_PDF_DidStartLoading(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    PP_Instance instance) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;

  PPBPDFInterface()->DidStartLoading(instance);

  DebugPrintf("PPB_PDF::DidStartLoading: instance=%"NACL_PRId32"\n", instance);
  rpc->result = NACL_SRPC_RESULT_OK;
}

void PpbPdfRpcServer::PPB_PDF_DidStopLoading(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    PP_Instance instance) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;

  PPBPDFInterface()->DidStopLoading(instance);

  DebugPrintf("PPB_PDF::DidStopLoading: instance=%"NACL_PRId32"\n", instance);
  rpc->result = NACL_SRPC_RESULT_OK;
}

void PpbPdfRpcServer::PPB_PDF_SetContentRestriction(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    PP_Instance instance,
    int32_t restrictions) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;

  PPBPDFInterface()->SetContentRestriction(instance, restrictions);

  DebugPrintf("PPB_PDF::SetContentRestriction: instance=%"NACL_PRId32"\n",
              instance);
  rpc->result = NACL_SRPC_RESULT_OK;
}

void PpbPdfRpcServer::PPB_PDF_HistogramPDFPageCount(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    int32_t count) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;

  PPBPDFInterface()->HistogramPDFPageCount(count);

  DebugPrintf("PPB_PDF::HistogramPDFPageCount\n");
  rpc->result = NACL_SRPC_RESULT_OK;
}

void PpbPdfRpcServer::PPB_PDF_UserMetricsRecordAction(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    nacl_abi_size_t action_size, char* action) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;

  PP_Var pp_action = PP_MakeUndefined();
  if (!DeserializeTo(rpc->channel, action, action_size, 1, &pp_action))
    return;
  PPBPDFInterface()->UserMetricsRecordAction(pp_action);

  DebugPrintf("PPB_PDF::UserMetricsRecordAction\n");
  rpc->result = NACL_SRPC_RESULT_OK;
}

void PpbPdfRpcServer::PPB_PDF_HasUnsupportedFeature(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    PP_Instance instance) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;

  PPBPDFInterface()->HasUnsupportedFeature(instance);

  DebugPrintf("PPB_PDF::HasUnsupportedFeature: instance=%"NACL_PRId32"\n",
              instance);
  rpc->result = NACL_SRPC_RESULT_OK;
}

void PpbPdfRpcServer::PPB_PDF_SaveAs(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    PP_Instance instance) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;

  PPBPDFInterface()->SaveAs(instance);

  DebugPrintf("PPB_PDF::SaveAs: instance=%"NACL_PRId32"\n", instance);
  rpc->result = NACL_SRPC_RESULT_OK;
}

