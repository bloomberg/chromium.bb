// Copyright (c) 2011 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// SRPC-abstraction wrappers around PPP_Printing functions.

#include <string.h>

#include "native_client/src/include/portability.h"
#include "native_client/src/include/portability_process.h"
#include "native_client/src/shared/ppapi_proxy/browser_globals.h"
#include "native_client/src/shared/ppapi_proxy/plugin_globals.h"
#include "native_client/src/shared/ppapi_proxy/utility.h"
#include "ppapi/c/dev/ppp_printing_dev.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/ppp.h"
#include "srpcgen/ppp_rpc.h"

using ppapi_proxy::DebugPrintf;
using ppapi_proxy::PPPPrintingInterface;

namespace {

const nacl_abi_size_t kPPPrintOutputFormatBytes =
    static_cast<nacl_abi_size_t>(sizeof(PP_PrintOutputFormat_Dev));
const nacl_abi_size_t kPPPrintSettingsBytes =
    static_cast<nacl_abi_size_t>(sizeof(struct PP_PrintSettings_Dev));
const nacl_abi_size_t kPPPrintPageNumberRangeBytes =
    static_cast<nacl_abi_size_t>(sizeof(struct PP_PrintPageNumberRange_Dev));

}  // namespace

void PppPrintingRpcServer::PPP_Printing_QuerySupportedFormats(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    // inputs
    PP_Instance instance,
    // outputs
    nacl_abi_size_t* formats_bytes, char* formats,
    int32_t* format_count) {
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;
  NaClSrpcClosureRunner runner(done);

  PP_PrintOutputFormat_Dev* pp_formats =
      PPPPrintingInterface()->QuerySupportedFormats(
          instance,
          reinterpret_cast<uint32_t*>(format_count));
  if (pp_formats != NULL) {
    nacl_abi_size_t formats_bytes_needed =
        *format_count * kPPPrintOutputFormatBytes;
    if (*formats_bytes >= formats_bytes_needed) {
      *formats_bytes = formats_bytes_needed;
      memcpy(pp_formats, formats, formats_bytes_needed);
    } else {
      *format_count = 0;
    }
    ppapi_proxy::PPBMemoryInterface()->MemFree(pp_formats);
  }

  DebugPrintf("PPP_Printing::QuerySupportedFormats: "
              "format_count=%"NACL_PRId32"\n", *format_count);
  rpc->result = NACL_SRPC_RESULT_OK;
}

void PppPrintingRpcServer::PPP_Printing_Begin(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    // inputs
    PP_Instance instance,
    nacl_abi_size_t print_settings_bytes, char* print_settings,
    // outputs
    int32_t* pages_required) {
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;
  NaClSrpcClosureRunner runner(done);

  if (print_settings_bytes != sizeof(struct PP_PrintSettings_Dev))
    return;
  struct PP_PrintSettings_Dev* pp_print_settings =
      reinterpret_cast<struct PP_PrintSettings_Dev*>(print_settings);
  *pages_required = PPPPrintingInterface()->Begin(instance, pp_print_settings);

  DebugPrintf("PPP_Printing::Begin: pages_required=%"NACL_PRId32"\n",
              *pages_required);
  rpc->result = NACL_SRPC_RESULT_OK;
}

void PppPrintingRpcServer::PPP_Printing_PrintPages(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    // inputs
    PP_Instance instance,
    nacl_abi_size_t page_ranges_bytes, char* page_ranges,
    int32_t page_range_count,
    // outputs
    PP_Resource* image_data) {
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;
  NaClSrpcClosureRunner runner(done);

  if (page_ranges_bytes < kPPPrintPageNumberRangeBytes * page_range_count)
    return;
  struct PP_PrintPageNumberRange_Dev* pp_page_ranges =
      reinterpret_cast<struct PP_PrintPageNumberRange_Dev*>(page_ranges);
  *image_data = PPPPrintingInterface()->PrintPages(instance,
                                                   pp_page_ranges,
                                                   page_range_count);

  DebugPrintf("PPP_Printing::PrintPages: image_data=%"NACL_PRIu32"\n",
              *image_data);
  rpc->result = NACL_SRPC_RESULT_OK;
}

void PppPrintingRpcServer::PPP_Printing_End(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    // inputs
    PP_Instance instance) {
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;
  NaClSrpcClosureRunner runner(done);

  PPPPrintingInterface()->End(instance);

  DebugPrintf("PPP_Printing::End\n");
  rpc->result = NACL_SRPC_RESULT_OK;
}
