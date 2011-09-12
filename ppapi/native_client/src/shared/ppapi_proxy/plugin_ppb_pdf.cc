// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "native_client/src/shared/ppapi_proxy/plugin_ppb_pdf.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>

#include "native_client/src/include/nacl_scoped_ptr.h"
#include "native_client/src/include/portability.h"
#include "native_client/src/shared/ppapi_proxy/object_serialize.h"
#include "native_client/src/shared/ppapi_proxy/plugin_globals.h"
#include "native_client/src/shared/ppapi_proxy/plugin_ppb_image_data.h"
#include "native_client/src/shared/ppapi_proxy/plugin_resource.h"
#include "native_client/src/shared/ppapi_proxy/utility.h"
#include "native_client/src/shared/srpc/nacl_srpc.h"
#include "ppapi/c/private/ppb_pdf.h"
#include "srpcgen/ppb_rpc.h"

namespace ppapi_proxy {

namespace {

const nacl_abi_size_t kPpbPrivateFindResultBytes =
    static_cast<nacl_abi_size_t>(sizeof(PP_PrivateFindResult));
// Limit the maximum result buffer size.
const nacl_abi_size_t kMaxFindResultsBytes = 65536;
const int32_t kMaxFindResults =
    kMaxFindResultsBytes / kPpbPrivateFindResultBytes;

PP_Var GetLocalizedString(
    PP_Instance instance,
    PP_ResourceString string_id) {
  DebugPrintf("PPB_PDF::GetLocalizedString: "
              "instance=%"NACL_PRIu32"\n", instance);

  NaClSrpcChannel* channel = GetMainSrpcChannel();
  nacl_abi_size_t string_size = kMaxVarSize;
  nacl::scoped_array<char> string_bytes(new char[kMaxVarSize]);
  NaClSrpcError srpc_result =
      PpbPdfRpcClient::PPB_PDF_GetLocalizedString(
          channel,
          instance,
          static_cast<int32_t>(string_id),
          &string_size, string_bytes.get());

  PP_Var string = PP_MakeUndefined();
  if (srpc_result == NACL_SRPC_RESULT_OK) {
    (void) DeserializeTo(
        channel, string_bytes.get(), string_size, 1, &string);
  }

  DebugPrintf("PPB_PDF::GetLocalizedString: %s\n",
              NaClSrpcErrorString(srpc_result));
  return string;
}

PP_Resource GetResourceImage(PP_Instance instance,
                             PP_ResourceImage image_id) {
  DebugPrintf("PPB_PDF::GetResourceImage: "
              "instance=%"NACL_PRIu32"\n", instance);

  PP_Resource image = kInvalidResourceId;
  NaClSrpcError srpc_result =
      PpbPdfRpcClient::PPB_PDF_GetResourceImage(
          GetMainSrpcChannel(),
          instance,
          static_cast<int32_t>(image_id),
          &image);

  DebugPrintf("PPB_PDF::GetResourceImage: %s\n",
              NaClSrpcErrorString(srpc_result));
  if (srpc_result == NACL_SRPC_RESULT_OK) {
    scoped_refptr<PluginImageData> image_data =
        PluginResource::AdoptAs<PluginImageData>(image);
    if (image_data.get()) {
      return image;
    }
  }
  return kInvalidResourceId;
}

PP_Resource GetFontFileWithFallback(
    PP_Instance instance,
    const struct PP_FontDescription_Dev* description,
    PP_PrivateFontCharset charset) {
  DebugPrintf("PPB_PDF::GetFontFileWithFallback: Not Implemented\n");
  return kInvalidResourceId;
}

bool GetFontTableForPrivateFontFile(PP_Resource font_file,
                                    uint32_t table,
                                    void* output,
                                    uint32_t* output_length) {
  DebugPrintf("PPB_PDF::GetFontTableForPrivateFontFile: Not Implemented\n");
  *output_length = 0;
  return false;
}

nacl_abi_size_t utf16_length(const unsigned short* str) {
  const unsigned short* s = str;
  while (*s)
    ++s;
  return (s - str);
}

void SearchString(PP_Instance instance,
                  const unsigned short* string,
                  const unsigned short* term,
                  bool case_sensitive,
                  struct PP_PrivateFindResult** results,
                  int* count) {
  DebugPrintf("PPB_PDF::SearchString: instance=%"NACL_PRIu32"\n", instance);

  CHECK(string != NULL);
  CHECK(term != NULL);
  CHECK(results != NULL);
  CHECK(count != NULL);

  nacl_abi_size_t find_results_size = kMaxFindResultsBytes;
  *results = reinterpret_cast<PP_PrivateFindResult*>(malloc(find_results_size));
  *count = 0;

  nacl_abi_size_t string_length = utf16_length(string);
  nacl_abi_size_t term_length = utf16_length(term);
  if (string_length > 0 && term_length > 0) {
    NaClSrpcError srpc_result =
        PpbPdfRpcClient::PPB_PDF_SearchString(
            GetMainSrpcChannel(),
            instance,
            (string_length + 1) * sizeof(unsigned short),  // include NULL
            reinterpret_cast<char*>(const_cast<unsigned short*>(string)),
            (term_length + 1) * sizeof(unsigned short),  // include NULL
            reinterpret_cast<char*>(const_cast<unsigned short*>(term)),
            static_cast<int32_t>(case_sensitive),
            &find_results_size,
            reinterpret_cast<char*>(*results),
            reinterpret_cast<int32_t*>(count));

    DebugPrintf("PPB_PDF::SearchString: %s\n",
        NaClSrpcErrorString(srpc_result));
  }
}

void DidStartLoading(PP_Instance instance) {
  DebugPrintf("PPB_PDF::DidStartLoading: instance=%"NACL_PRIu32"\n",
              instance);

  NaClSrpcError srpc_result =
      PpbPdfRpcClient::PPB_PDF_DidStartLoading(GetMainSrpcChannel(),
                                               instance);

  DebugPrintf("PPB_PDF::DidStartLoading: %s\n",
              NaClSrpcErrorString(srpc_result));
}

void DidStopLoading(PP_Instance instance) {
  DebugPrintf("PPB_PDF::DidStopLoading: instance=%"NACL_PRIu32"\n",
              instance);

  NaClSrpcError srpc_result =
      PpbPdfRpcClient::PPB_PDF_DidStopLoading(GetMainSrpcChannel(),
                                              instance);

  DebugPrintf("PPB_PDF::DidStopLoading: %s\n",
              NaClSrpcErrorString(srpc_result));
}

void SetContentRestriction(PP_Instance instance,
                           int restrictions) {
  DebugPrintf("PPB_PDF::SetContentRestriction: instance=%"NACL_PRIu32"\n",
              instance);

  NaClSrpcError srpc_result =
      PpbPdfRpcClient::PPB_PDF_SetContentRestriction(GetMainSrpcChannel(),
                                                     instance,
                                                     restrictions);

  DebugPrintf("PPB_PDF::SetContentRestriction: %s\n",
              NaClSrpcErrorString(srpc_result));
}

void HistogramPDFPageCount(int count) {
  DebugPrintf("PPB_PDF::HistogramPDFPageCount/n");

  NaClSrpcError srpc_result =
      PpbPdfRpcClient::PPB_PDF_HistogramPDFPageCount(GetMainSrpcChannel(),
                                                     count);

  DebugPrintf("PPB_PDF::HistogramPDFPageCount: %s\n",
              NaClSrpcErrorString(srpc_result));
}

void UserMetricsRecordAction(struct PP_Var action) {
  DebugPrintf("PPB_PDF::UserMetricsRecordAction\n");

  nacl_abi_size_t action_size = kMaxVarSize;
  nacl::scoped_array<char> action_bytes(
      Serialize(&action, 1, &action_size));
  NaClSrpcError srpc_result =
      PpbPdfRpcClient::PPB_PDF_UserMetricsRecordAction(GetMainSrpcChannel(),
                                                       action_size,
                                                       action_bytes.get());

  DebugPrintf("PPB_PDF::UserMetricsRecordAction: %s\n",
              NaClSrpcErrorString(srpc_result));
}

void HasUnsupportedFeature(PP_Instance instance) {
  DebugPrintf("PPB_PDF::HasUnsupportedFeature: instance=%"NACL_PRIu32"\n",
              instance);

  NaClSrpcError srpc_result =
      PpbPdfRpcClient::PPB_PDF_HasUnsupportedFeature(GetMainSrpcChannel(),
                                                     instance);

  DebugPrintf("PPB_PDF::HasUnsupportedFeature: %s\n",
              NaClSrpcErrorString(srpc_result));
}

void SaveAs(PP_Instance instance) {
  DebugPrintf("PPB_PDF::SaveAs: instance=%"NACL_PRIu32"\n", instance);

  NaClSrpcError srpc_result =
      PpbPdfRpcClient::PPB_PDF_SaveAs(GetMainSrpcChannel(),
                                      instance);

  DebugPrintf("PPB_PDF::SaveAs: %s\n", NaClSrpcErrorString(srpc_result));
}

}  // namespace

const PPB_PDF* PluginPDF::GetInterface() {
  static const PPB_PDF pdf_interface = {
    GetLocalizedString,
    GetResourceImage,
    GetFontFileWithFallback,
    GetFontTableForPrivateFontFile,
    SearchString,
    DidStartLoading,
    DidStopLoading,
    SetContentRestriction,
    HistogramPDFPageCount,
    UserMetricsRecordAction,
    HasUnsupportedFeature,
    SaveAs
  };
  return &pdf_interface;
}

}  // namespace ppapi_proxy

