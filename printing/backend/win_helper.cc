// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/backend/win_helper.h"

#include "base/logging.h"

namespace {
typedef HRESULT (WINAPI *PTOpenProviderProc)(PCWSTR printer_name,
                                             DWORD version,
                                             HPTPROVIDER *provider);
typedef HRESULT (WINAPI *PTGetPrintCapabilitiesProc)(HPTPROVIDER provider,
                                                     IStream *print_ticket,
                                                     IStream *capabilities,
                                                     BSTR* error_message);
typedef HRESULT (WINAPI *PTConvertDevModeToPrintTicketProc)(
    HPTPROVIDER provider,
    ULONG devmode_size_in_bytes,
    PDEVMODE devmode,
    EPrintTicketScope scope,
    IStream* print_ticket);
typedef HRESULT (WINAPI *PTConvertPrintTicketToDevModeProc)(
    HPTPROVIDER provider,
    IStream* print_ticket,
    EDefaultDevmodeType base_devmode_type,
    EPrintTicketScope scope,
    ULONG* devmode_byte_count,
    PDEVMODE *devmode,
    BSTR* error_message);
typedef HRESULT (WINAPI *PTMergeAndValidatePrintTicketProc)(
    HPTPROVIDER provider,
    IStream* base_ticket,
    IStream* delta_ticket,
    EPrintTicketScope scope,
    IStream* result_ticket,
    BSTR* error_message);
typedef HRESULT (WINAPI *PTReleaseMemoryProc)(PVOID buffer);
typedef HRESULT (WINAPI *PTCloseProviderProc)(HPTPROVIDER provider);

PTOpenProviderProc g_open_provider_proc = NULL;
PTGetPrintCapabilitiesProc g_get_print_capabilities_proc = NULL;
PTConvertDevModeToPrintTicketProc g_convert_devmode_to_print_ticket_proc = NULL;
PTConvertPrintTicketToDevModeProc g_convert_print_ticket_to_devmode_proc = NULL;
PTMergeAndValidatePrintTicketProc g_merge_and_validate_print_ticket_proc = NULL;
PTReleaseMemoryProc g_release_memory_proc = NULL;
PTCloseProviderProc g_close_provider_proc = NULL;
}

namespace printing {

bool XPSModule::Init() {
  static bool initialized = InitImpl();
  return initialized;
}

bool XPSModule::InitImpl() {
  HMODULE prntvpt_module = LoadLibrary(L"prntvpt.dll");
  if (prntvpt_module == NULL)
    return false;
  g_open_provider_proc = reinterpret_cast<PTOpenProviderProc>(
      GetProcAddress(prntvpt_module, "PTOpenProvider"));
  if (!g_open_provider_proc) {
    NOTREACHED();
    return false;
  }
  g_get_print_capabilities_proc = reinterpret_cast<PTGetPrintCapabilitiesProc>(
      GetProcAddress(prntvpt_module, "PTGetPrintCapabilities"));
  if (!g_get_print_capabilities_proc) {
    NOTREACHED();
    return false;
  }
  g_convert_devmode_to_print_ticket_proc =
      reinterpret_cast<PTConvertDevModeToPrintTicketProc>(
          GetProcAddress(prntvpt_module, "PTConvertDevModeToPrintTicket"));
  if (!g_convert_devmode_to_print_ticket_proc) {
    NOTREACHED();
    return false;
  }
  g_convert_print_ticket_to_devmode_proc =
      reinterpret_cast<PTConvertPrintTicketToDevModeProc>(
          GetProcAddress(prntvpt_module, "PTConvertPrintTicketToDevMode"));
  if (!g_convert_print_ticket_to_devmode_proc) {
    NOTREACHED();
    return false;
  }
  g_merge_and_validate_print_ticket_proc =
      reinterpret_cast<PTMergeAndValidatePrintTicketProc>(
          GetProcAddress(prntvpt_module, "PTMergeAndValidatePrintTicket"));
  if (!g_merge_and_validate_print_ticket_proc) {
    NOTREACHED();
    return false;
  }
  g_release_memory_proc =
      reinterpret_cast<PTReleaseMemoryProc>(
          GetProcAddress(prntvpt_module, "PTReleaseMemory"));
  if (!g_release_memory_proc) {
    NOTREACHED();
    return false;
  }
  g_close_provider_proc =
      reinterpret_cast<PTCloseProviderProc>(
          GetProcAddress(prntvpt_module, "PTCloseProvider"));
  if (!g_close_provider_proc) {
    NOTREACHED();
    return false;
  }
  return true;
}

HRESULT XPSModule::OpenProvider(const string16& printer_name,
                                DWORD version,
                                HPTPROVIDER *provider) {
  return g_open_provider_proc(printer_name.c_str(), version, provider);
}

HRESULT XPSModule::GetPrintCapabilities(HPTPROVIDER provider,
                                        IStream *print_ticket,
                                        IStream *capabilities,
                                        BSTR* error_message) {
  return g_get_print_capabilities_proc(provider,
                                       print_ticket,
                                       capabilities,
                                       error_message);
}

HRESULT XPSModule::ConvertDevModeToPrintTicket(HPTPROVIDER provider,
                                               ULONG devmode_size_in_bytes,
                                               PDEVMODE devmode,
                                               EPrintTicketScope scope,
                                               IStream* print_ticket) {
  return g_convert_devmode_to_print_ticket_proc(provider,
                                                devmode_size_in_bytes,
                                                devmode,
                                                scope,
                                                print_ticket);
}

HRESULT XPSModule::ConvertPrintTicketToDevMode(
    HPTPROVIDER provider,
    IStream* print_ticket,
    EDefaultDevmodeType base_devmode_type,
    EPrintTicketScope scope,
    ULONG* devmode_byte_count,
    PDEVMODE *devmode,
    BSTR* error_message) {
  return g_convert_print_ticket_to_devmode_proc(provider,
                                                print_ticket,
                                                base_devmode_type,
                                                scope,
                                                devmode_byte_count,
                                                devmode,
                                                error_message);
}

HRESULT XPSModule::MergeAndValidatePrintTicket(HPTPROVIDER provider,
                                               IStream* base_ticket,
                                               IStream* delta_ticket,
                                               EPrintTicketScope scope,
                                               IStream* result_ticket,
                                               BSTR* error_message) {
  return g_merge_and_validate_print_ticket_proc(provider,
                                                base_ticket,
                                                delta_ticket,
                                                scope,
                                                result_ticket,
                                                error_message);
}

HRESULT XPSModule::ReleaseMemory(PVOID buffer) {
  return g_release_memory_proc(buffer);
}

HRESULT XPSModule::CloseProvider(HPTPROVIDER provider) {
  return g_close_provider_proc(provider);
}

}  // namespace printing
