// Copyright (c) 2011 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "native_client/src/shared/ppapi_proxy/browser_ppp_printing.h"

// Include file order cannot be observed because ppp_instance declares a
// structure return type that causes an error on Windows.
// TODO(sehr, brettw): fix the return types and include order in PPAPI.
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_resource.h"
#include "srpcgen/ppp_rpc.h"
#include "native_client/src/include/portability.h"
#include "native_client/src/shared/ppapi_proxy/browser_globals.h"
#include "native_client/src/shared/ppapi_proxy/browser_ppp.h"
#include "native_client/src/shared/ppapi_proxy/utility.h"

namespace ppapi_proxy {

namespace {

const nacl_abi_size_t kPPPrintOutputFormatBytes =
    static_cast<nacl_abi_size_t>(sizeof(PP_PrintOutputFormat_Dev));
const nacl_abi_size_t kPPPrintSettingsBytes =
    static_cast<nacl_abi_size_t>(sizeof(struct PP_PrintSettings_Dev));
const nacl_abi_size_t kPPPrintPageNumberRangeBytes =
    static_cast<nacl_abi_size_t>(sizeof(struct PP_PrintPageNumberRange_Dev));

PP_PrintOutputFormat_Dev* QuerySupportedFormats(PP_Instance instance,
                                                uint32_t* format_count) {
  DebugPrintf("PPP_Printing_Dev::QuerySupportedFormats: "
              "instance=%"NACL_PRIu32"\n", instance);

  const PPB_Memory_Dev* ppb_memory = PPBMemoryInterface();
  const nacl_abi_size_t kMaxFormats = 8;
  nacl_abi_size_t formats_bytes = kMaxFormats * kPPPrintOutputFormatBytes;
  char* formats =
      reinterpret_cast<char*>(ppb_memory->MemAlloc(formats_bytes));
  NaClSrpcError srpc_result =
      PppPrintingRpcClient::PPP_Printing_QuerySupportedFormats(
          GetMainSrpcChannel(instance),
          instance,
          &formats_bytes, formats,
          reinterpret_cast<int32_t*>(format_count));

  DebugPrintf("PPP_Printing_Dev::QuerySupportedFormats: %s\n",
              NaClSrpcErrorString(srpc_result));

  if (*format_count > 0)
    return reinterpret_cast<PP_PrintOutputFormat_Dev*>(formats);

  ppb_memory->MemFree(formats);
  return NULL;
}

int32_t Begin(PP_Instance instance,
              const struct PP_PrintSettings_Dev* print_settings) {
  DebugPrintf("PPP_Printing_Dev::Begin: instance=%"NACL_PRIu32"\n", instance);

  int32_t pages_required = 0;
  NaClSrpcError srpc_result =
      PppPrintingRpcClient::PPP_Printing_Begin(
          GetMainSrpcChannel(instance),
          instance,
          kPPPrintSettingsBytes,
          reinterpret_cast<char*>(
              const_cast<PP_PrintSettings_Dev*>(print_settings)),
          &pages_required);

  DebugPrintf("PPP_Printing_Dev::QuerySupportedFormats: %s\n",
              NaClSrpcErrorString(srpc_result));
  return pages_required;
}

PP_Resource PrintPages(PP_Instance instance,
                       const struct PP_PrintPageNumberRange_Dev* page_ranges,
                       uint32_t page_range_count) {
  DebugPrintf("PPP_Printing_Dev::PrintPages: "
              "instance=%"NACL_PRIu32"\n", instance);

  PP_Resource image_data = kInvalidResourceId;
  NaClSrpcError srpc_result =
      PppPrintingRpcClient::PPP_Printing_PrintPages(
          GetMainSrpcChannel(instance),
          instance,
          page_range_count * kPPPrintPageNumberRangeBytes,
          reinterpret_cast<char*>(
              const_cast<PP_PrintPageNumberRange_Dev*>(page_ranges)),
          page_range_count,
          &image_data);

  DebugPrintf("PPP_Printing_Dev::QuerySupportedFormats: %s\n",
              NaClSrpcErrorString(srpc_result));
  return image_data;
}

void End(PP_Instance instance) {
  DebugPrintf("PPP_Printing_Dev::End: instance=%"NACL_PRIu32"\n", instance);

  NaClSrpcError srpc_result =
      PppPrintingRpcClient::PPP_Printing_End(GetMainSrpcChannel(instance),
                                             instance);

  DebugPrintf("PPP_Printing_Dev::End: %s\n", NaClSrpcErrorString(srpc_result));
}

}  // namespace

const PPP_Printing_Dev* BrowserPrinting::GetInterface() {
  static const PPP_Printing_Dev printing_interface = {
    QuerySupportedFormats,
    Begin,
    PrintPages,
    End
  };
  return &printing_interface;
}

}  // namespace ppapi_proxy

