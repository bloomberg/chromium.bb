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
typedef HRESULT (WINAPI *StartXpsPrintJobProc)(
    const LPCWSTR printer_name,
    const LPCWSTR job_name,
    const LPCWSTR output_file_name,
    HANDLE progress_event,
    HANDLE completion_event,
    UINT8 *printable_pages_on,
    UINT32 printable_pages_on_count,
    IXpsPrintJob **xps_print_job,
    IXpsPrintJobStream **document_stream,
    IXpsPrintJobStream **print_ticket_stream);

PTOpenProviderProc g_open_provider_proc = NULL;
PTGetPrintCapabilitiesProc g_get_print_capabilities_proc = NULL;
PTConvertDevModeToPrintTicketProc g_convert_devmode_to_print_ticket_proc = NULL;
PTConvertPrintTicketToDevModeProc g_convert_print_ticket_to_devmode_proc = NULL;
PTMergeAndValidatePrintTicketProc g_merge_and_validate_print_ticket_proc = NULL;
PTReleaseMemoryProc g_release_memory_proc = NULL;
PTCloseProviderProc g_close_provider_proc = NULL;
StartXpsPrintJobProc g_start_xps_print_job_proc = NULL;
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

ScopedXPSInitializer::ScopedXPSInitializer() : initialized_(false) {
  if (XPSModule::Init()) {
    // Calls to XPS APIs typically require the XPS provider to be opened with
    // PTOpenProvider. PTOpenProvider calls CoInitializeEx with
    // COINIT_MULTITHREADED. We have seen certain buggy HP printer driver DLLs
    // that call CoInitializeEx with COINIT_APARTMENTTHREADED in the context of
    // PTGetPrintCapabilities. This call fails but the printer driver calls
    // CoUninitialize anyway. This results in the apartment being torn down too
    // early and the msxml DLL being unloaded which in turn causes code in
    // unidrvui.dll to have a dangling pointer to an XML document which causes a
    // crash. To protect ourselves from such drivers we make sure we always have
    // an extra CoInitialize (calls to CoInitialize/CoUninitialize are
    // refcounted).
    HRESULT coinit_ret = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    // If this succeeded we are done because the PTOpenProvider call will
    // provide the extra refcount on the apartment. If it failed because someone
    // already called CoInitializeEx with COINIT_APARTMENTTHREADED, we try
    // the other model to provide the additional refcount (since we don't know
    // which model buggy printer drivers will use).
    if (coinit_ret == RPC_E_CHANGED_MODE)
      coinit_ret = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    DCHECK(SUCCEEDED(coinit_ret));
    initialized_ = true;
  }
}

ScopedXPSInitializer::~ScopedXPSInitializer() {
  if (initialized_)
    CoUninitialize();
  initialized_ = false;
}

bool XPSPrintModule::Init() {
  static bool initialized = InitImpl();
  return initialized;
}

bool XPSPrintModule::InitImpl() {
  HMODULE xpsprint_module = LoadLibrary(L"xpsprint.dll");
  if (xpsprint_module == NULL)
    return false;
  g_start_xps_print_job_proc = reinterpret_cast<StartXpsPrintJobProc>(
      GetProcAddress(xpsprint_module, "StartXpsPrintJob"));
  if (!g_start_xps_print_job_proc) {
    NOTREACHED();
    return false;
  }
  return true;
}

HRESULT XPSPrintModule::StartXpsPrintJob(
    const LPCWSTR printer_name,
    const LPCWSTR job_name,
    const LPCWSTR output_file_name,
    HANDLE progress_event,
    HANDLE completion_event,
    UINT8 *printable_pages_on,
    UINT32 printable_pages_on_count,
    IXpsPrintJob **xps_print_job,
    IXpsPrintJobStream **document_stream,
    IXpsPrintJobStream **print_ticket_stream) {
  return g_start_xps_print_job_proc(printer_name,
                                    job_name,
                                    output_file_name,
                                    progress_event,
                                    completion_event,
                                    printable_pages_on,
                                    printable_pages_on_count,
                                    xps_print_job,
                                    document_stream,
                                    print_ticket_stream);
}

}  // namespace printing
