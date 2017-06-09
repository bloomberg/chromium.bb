// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/backend/win_helper.h"

#include <stddef.h>

#include <algorithm>
#include <memory>

#include "base/file_version_info.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/free_deleter.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/scoped_comptr.h"
#include "base/win/windows_version.h"
#include "printing/backend/print_backend.h"
#include "printing/backend/print_backend_consts.h"
#include "printing/backend/printing_info_win.h"

namespace {

typedef HRESULT (WINAPI* PTOpenProviderProc)(PCWSTR printer_name,
                                             DWORD version,
                                             HPTPROVIDER* provider);

typedef HRESULT (WINAPI* PTGetPrintCapabilitiesProc)(HPTPROVIDER provider,
                                                     IStream* print_ticket,
                                                     IStream* capabilities,
                                                     BSTR* error_message);

typedef HRESULT (WINAPI* PTConvertDevModeToPrintTicketProc)(
    HPTPROVIDER provider,
    ULONG devmode_size_in_bytes,
    PDEVMODE devmode,
    EPrintTicketScope scope,
    IStream* print_ticket);

typedef HRESULT (WINAPI* PTConvertPrintTicketToDevModeProc)(
    HPTPROVIDER provider,
    IStream* print_ticket,
    EDefaultDevmodeType base_devmode_type,
    EPrintTicketScope scope,
    ULONG* devmode_byte_count,
    PDEVMODE* devmode,
    BSTR* error_message);

typedef HRESULT (WINAPI* PTMergeAndValidatePrintTicketProc)(
    HPTPROVIDER provider,
    IStream* base_ticket,
    IStream* delta_ticket,
    EPrintTicketScope scope,
    IStream* result_ticket,
    BSTR* error_message);

typedef HRESULT (WINAPI* PTReleaseMemoryProc)(PVOID buffer);

typedef HRESULT (WINAPI* PTCloseProviderProc)(HPTPROVIDER provider);

typedef HRESULT (WINAPI* StartXpsPrintJobProc)(
    const LPCWSTR printer_name,
    const LPCWSTR job_name,
    const LPCWSTR output_file_name,
    HANDLE progress_event,
    HANDLE completion_event,
    UINT8* printable_pages_on,
    UINT32 printable_pages_on_count,
    IXpsPrintJob** xps_print_job,
    IXpsPrintJobStream** document_stream,
    IXpsPrintJobStream** print_ticket_stream);

PTOpenProviderProc g_open_provider_proc = NULL;
PTGetPrintCapabilitiesProc g_get_print_capabilities_proc = NULL;
PTConvertDevModeToPrintTicketProc g_convert_devmode_to_print_ticket_proc = NULL;
PTConvertPrintTicketToDevModeProc g_convert_print_ticket_to_devmode_proc = NULL;
PTMergeAndValidatePrintTicketProc g_merge_and_validate_print_ticket_proc = NULL;
PTReleaseMemoryProc g_release_memory_proc = NULL;
PTCloseProviderProc g_close_provider_proc = NULL;
StartXpsPrintJobProc g_start_xps_print_job_proc = NULL;

HRESULT StreamFromPrintTicket(const std::string& print_ticket,
                              IStream** stream) {
  DCHECK(stream);
  HRESULT hr = CreateStreamOnHGlobal(NULL, TRUE, stream);
  if (FAILED(hr)) {
    return hr;
  }
  ULONG bytes_written = 0;
  (*stream)->Write(print_ticket.c_str(),
                   base::checked_cast<ULONG>(print_ticket.length()),
                   &bytes_written);
  DCHECK(bytes_written == print_ticket.length());
  LARGE_INTEGER pos = {};
  ULARGE_INTEGER new_pos = {};
  (*stream)->Seek(pos, STREAM_SEEK_SET, &new_pos);
  return S_OK;
}

const char kXpsTicketTemplate[] =
  "<?xml version='1.0' encoding='UTF-8'?>"
  "<psf:PrintTicket "
  "xmlns:psf='"
  "http://schemas.microsoft.com/windows/2003/08/printing/printschemaframework' "
  "xmlns:psk="
  "'http://schemas.microsoft.com/windows/2003/08/printing/printschemakeywords' "
  "version='1'>"
  "<psf:Feature name='psk:PageOutputColor'>"
  "<psf:Option name='psk:%s'>"
  "</psf:Option>"
  "</psf:Feature>"
  "</psf:PrintTicket>";

const char kXpsTicketColor[] = "Color";
const char kXpsTicketMonochrome[] = "Monochrome";


}  // namespace


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

HRESULT XPSModule::OpenProvider(const base::string16& printer_name,
                                DWORD version,
                                HPTPROVIDER* provider) {
  return g_open_provider_proc(printer_name.c_str(), version, provider);
}

HRESULT XPSModule::GetPrintCapabilities(HPTPROVIDER provider,
                                        IStream* print_ticket,
                                        IStream* capabilities,
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
    PDEVMODE* devmode,
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
  if (!XPSModule::Init())
    return;
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
  HRESULT hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
  // If this succeeded we are done because the PTOpenProvider call will provide
  // the extra refcount on the apartment. If it failed because someone already
  // called CoInitializeEx with COINIT_APARTMENTTHREADED, we try the other model
  // to provide the additional refcount (since we don't know which model buggy
  // printer drivers will use).
  if (!SUCCEEDED(hr))
    hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
  DCHECK(SUCCEEDED(hr));
  initialized_ = true;
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
    UINT8* printable_pages_on,
    UINT32 printable_pages_on_count,
    IXpsPrintJob** xps_print_job,
    IXpsPrintJobStream** document_stream,
    IXpsPrintJobStream** print_ticket_stream) {
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

bool InitBasicPrinterInfo(HANDLE printer, PrinterBasicInfo* printer_info) {
  DCHECK(printer);
  DCHECK(printer_info);
  if (!printer)
    return false;

  PrinterInfo2 info_2;
  if (!info_2.Init(printer))
    return false;

  printer_info->printer_name = base::WideToUTF8(info_2.get()->pPrinterName);
  if (info_2.get()->pComment) {
    printer_info->printer_description =
        base::WideToUTF8(info_2.get()->pComment);
  }
  if (info_2.get()->pLocation) {
    printer_info->options[kLocationTagName] =
        base::WideToUTF8(info_2.get()->pLocation);
  }
  if (info_2.get()->pDriverName) {
    printer_info->options[kDriverNameTagName] =
        base::WideToUTF8(info_2.get()->pDriverName);
  }
  printer_info->printer_status = info_2.get()->Status;

  std::string driver_info = GetDriverInfo(printer);
  if (!driver_info.empty())
    printer_info->options[kDriverInfoTagName] = driver_info;
  return true;
}

std::string GetDriverInfo(HANDLE printer) {
  DCHECK(printer);
  std::string driver_info;

  if (!printer)
    return driver_info;

  DriverInfo6 info_6;
  if (!info_6.Init(printer))
    return driver_info;

  std::string info[4];
  if (info_6.get()->pName)
    info[0] = base::WideToUTF8(info_6.get()->pName);

  if (info_6.get()->pDriverPath) {
    std::unique_ptr<FileVersionInfo> version_info(
        FileVersionInfo::CreateFileVersionInfo(
            base::FilePath(info_6.get()->pDriverPath)));
    if (version_info.get()) {
      info[1] = base::WideToUTF8(version_info->file_version());
      info[2] = base::WideToUTF8(version_info->product_name());
      info[3] = base::WideToUTF8(version_info->product_version());
    }
  }

  for (size_t i = 0; i < arraysize(info); ++i) {
    std::replace(info[i].begin(), info[i].end(), ';', ',');
    driver_info.append(info[i]);
    if (i < arraysize(info) - 1)
      driver_info.append(";");
  }
  return driver_info;
}

std::unique_ptr<DEVMODE, base::FreeDeleter> XpsTicketToDevMode(
    const base::string16& printer_name,
    const std::string& print_ticket) {
  std::unique_ptr<DEVMODE, base::FreeDeleter> dev_mode;
  printing::ScopedXPSInitializer xps_initializer;
  if (!xps_initializer.initialized()) {
    // TODO(sanjeevr): Handle legacy proxy case (with no prntvpt.dll)
    return dev_mode;
  }

  printing::ScopedPrinterHandle printer;
  if (!printer.OpenPrinter(printer_name.c_str()))
    return dev_mode;

  base::win::ScopedComPtr<IStream> pt_stream;
  HRESULT hr = StreamFromPrintTicket(print_ticket, pt_stream.GetAddressOf());
  if (FAILED(hr))
    return dev_mode;

  HPTPROVIDER provider = NULL;
  hr = printing::XPSModule::OpenProvider(printer_name, 1, &provider);
  if (SUCCEEDED(hr)) {
    ULONG size = 0;
    DEVMODE* dm = NULL;
    // Use kPTJobScope, because kPTDocumentScope breaks duplex.
    hr = printing::XPSModule::ConvertPrintTicketToDevMode(
        provider, pt_stream.Get(), kUserDefaultDevmode, kPTJobScope, &size, &dm,
        NULL);
    if (SUCCEEDED(hr)) {
      // Correct DEVMODE using DocumentProperties. See documentation for
      // PTConvertPrintTicketToDevMode.
      dev_mode = CreateDevMode(printer.Get(), dm);
      printing::XPSModule::ReleaseMemory(dm);
    }
    printing::XPSModule::CloseProvider(provider);
  }
  return dev_mode;
}

std::unique_ptr<DEVMODE, base::FreeDeleter> CreateDevModeWithColor(
    HANDLE printer,
    const base::string16& printer_name,
    bool color) {
  std::unique_ptr<DEVMODE, base::FreeDeleter> default_ticket =
      CreateDevMode(printer, NULL);
  if (!default_ticket)
    return default_ticket;

  if ((default_ticket->dmFields & DM_COLOR) &&
      ((default_ticket->dmColor == DMCOLOR_COLOR) == color)) {
    return default_ticket;
  }

  default_ticket->dmFields |= DM_COLOR;
  default_ticket->dmColor = color ? DMCOLOR_COLOR : DMCOLOR_MONOCHROME;

  DriverInfo6 info_6;
  if (!info_6.Init(printer))
    return default_ticket;

  const DRIVER_INFO_6* p = info_6.get();

  // Only HP known to have issues.
  if (!p->pszMfgName || wcscmp(p->pszMfgName, L"HP") != 0)
    return default_ticket;

  // Need XPS for this workaround.
  printing::ScopedXPSInitializer xps_initializer;
  if (!xps_initializer.initialized())
    return default_ticket;

  const char* xps_color = color ? kXpsTicketColor : kXpsTicketMonochrome;
  std::string xps_ticket = base::StringPrintf(kXpsTicketTemplate, xps_color);
  std::unique_ptr<DEVMODE, base::FreeDeleter> ticket =
      printing::XpsTicketToDevMode(printer_name, xps_ticket);
  if (!ticket)
    return default_ticket;

  return ticket;
}

bool PrinterHasValidPaperSize(const wchar_t* name, const wchar_t* port) {
  return DeviceCapabilities(name, port, DC_PAPERSIZE, nullptr, nullptr) > 0;
}

std::unique_ptr<DEVMODE, base::FreeDeleter> CreateDevMode(HANDLE printer,
                                                          DEVMODE* in) {
  LONG buffer_size = DocumentProperties(
      NULL, printer, const_cast<wchar_t*>(L""), NULL, NULL, 0);
  if (buffer_size < static_cast<int>(sizeof(DEVMODE)))
    return nullptr;

  // Some drivers request buffers with size smaller than dmSize + dmDriverExtra.
  // crbug.com/421402
  buffer_size *= 2;

  std::unique_ptr<DEVMODE, base::FreeDeleter> out(
      reinterpret_cast<DEVMODE*>(calloc(buffer_size, 1)));
  DWORD flags = (in ? (DM_IN_BUFFER) : 0) | DM_OUT_BUFFER;

  PrinterInfo5 info_5;
  if (!info_5.Init(printer))
    return nullptr;
  const wchar_t* name = info_5.get()->pPrinterName;
  const wchar_t* port = info_5.get()->pPortName;

  // Check that valid paper sizes exist; some old drivers return no paper sizes
  // and crash in DocumentProperties if used with Win10. See crbug.com/679160,
  // crbug.com/724595
  if (!PrinterHasValidPaperSize(name, port)) {
    return nullptr;
  }

  if (DocumentProperties(
          NULL, printer, const_cast<wchar_t*>(L""), out.get(), in, flags) !=
      IDOK) {
    return nullptr;
  }
  int size = out->dmSize;
  int extra_size = out->dmDriverExtra;
  CHECK_GE(buffer_size, size + extra_size);
  return out;
}

std::unique_ptr<DEVMODE, base::FreeDeleter> PromptDevMode(
    HANDLE printer,
    const base::string16& printer_name,
    DEVMODE* in,
    HWND window,
    bool* canceled) {
  LONG buffer_size =
      DocumentProperties(window,
                         printer,
                         const_cast<wchar_t*>(printer_name.c_str()),
                         NULL,
                         NULL,
                         0);
  if (buffer_size < static_cast<int>(sizeof(DEVMODE)))
    return std::unique_ptr<DEVMODE, base::FreeDeleter>();

  // Some drivers request buffers with size smaller than dmSize + dmDriverExtra.
  // crbug.com/421402
  buffer_size *= 2;

  std::unique_ptr<DEVMODE, base::FreeDeleter> out(
      reinterpret_cast<DEVMODE*>(calloc(buffer_size, 1)));
  DWORD flags = (in ? (DM_IN_BUFFER) : 0) | DM_OUT_BUFFER | DM_IN_PROMPT;
  LONG result = DocumentProperties(window,
                                   printer,
                                   const_cast<wchar_t*>(printer_name.c_str()),
                                   out.get(),
                                   in,
                                   flags);
  if (canceled)
    *canceled = (result == IDCANCEL);
  if (result != IDOK)
    return std::unique_ptr<DEVMODE, base::FreeDeleter>();
  int size = out->dmSize;
  int extra_size = out->dmDriverExtra;
  CHECK_GE(buffer_size, size + extra_size);
  return out;
}

}  // namespace printing
