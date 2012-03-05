// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/backend/print_backend.h"

#include <objidl.h>
#include <winspool.h>

#include "base/file_path.h"
#include "base/file_version_info.h"
#include "base/memory/scoped_ptr.h"
#include "base/string_piece.h"
#include "base/utf_string_conversions.h"
#include "base/win/scoped_bstr.h"
#include "base/win/scoped_comptr.h"
#include "base/win/scoped_hglobal.h"
#include "printing/backend/print_backend_consts.h"
#include "printing/backend/win_helper.h"

namespace {

HRESULT StreamOnHGlobalToString(IStream* stream, std::string* out) {
  DCHECK(stream);
  DCHECK(out);
  HGLOBAL hdata = NULL;
  HRESULT hr = GetHGlobalFromStream(stream, &hdata);
  if (SUCCEEDED(hr)) {
    DCHECK(hdata);
    base::win::ScopedHGlobal<char> locked_data(hdata);
    out->assign(locked_data.release(), locked_data.Size());
  }
  return hr;
}

}  // namespace

namespace printing {

class PrintBackendWin : public PrintBackend {
 public:
  PrintBackendWin() {}
  virtual ~PrintBackendWin() {}

  virtual bool EnumeratePrinters(PrinterList* printer_list);

  virtual std::string GetDefaultPrinterName();

  virtual bool GetPrinterCapsAndDefaults(const std::string& printer_name,
                                         PrinterCapsAndDefaults* printer_info);

  virtual bool GetPrinterDriverInfo(const std::string& printer_name,
                                    PrinterDriverInfo* driver_info);

  virtual bool IsValidPrinter(const std::string& printer_name);
};

bool PrintBackendWin::EnumeratePrinters(PrinterList* printer_list) {
  DCHECK(printer_list);
  DWORD bytes_needed = 0;
  DWORD count_returned = 0;
  BOOL ret = EnumPrinters(PRINTER_ENUM_LOCAL|PRINTER_ENUM_CONNECTIONS, NULL, 2,
                          NULL, 0, &bytes_needed, &count_returned);
  if (!bytes_needed)
    return false;
  scoped_array<BYTE> printer_info_buffer(new BYTE[bytes_needed]);
  ret = EnumPrinters(PRINTER_ENUM_LOCAL|PRINTER_ENUM_CONNECTIONS, NULL, 2,
                     printer_info_buffer.get(), bytes_needed, &bytes_needed,
                     &count_returned);
  DCHECK(ret);
  if (!ret)
    return false;

  std::string default_printer = GetDefaultPrinterName();
  PRINTER_INFO_2* printer_info =
      reinterpret_cast<PRINTER_INFO_2*>(printer_info_buffer.get());
  for (DWORD index = 0; index < count_returned; index++) {
    PrinterBasicInfo info;
    info.printer_name = WideToUTF8(printer_info[index].pPrinterName);
    info.is_default = (info.printer_name == default_printer);
    if (printer_info[index].pComment)
      info.printer_description = WideToUTF8(printer_info[index].pComment);
    info.printer_status = printer_info[index].Status;
    if (printer_info[index].pLocation)
      info.options[kLocationTagName] =
          WideToUTF8(printer_info[index].pLocation);
    if (printer_info[index].pDriverName)
      info.options[kDriverNameTagName] =
          WideToUTF8(printer_info[index].pDriverName);
    printer_list->push_back(info);
  }
  return true;
}

std::string PrintBackendWin::GetDefaultPrinterName() {
  DWORD size = MAX_PATH;
  TCHAR default_printer_name[MAX_PATH];
  if (!::GetDefaultPrinter(default_printer_name, &size))
    return std::string();
  return WideToUTF8(default_printer_name);
}

bool PrintBackendWin::GetPrinterCapsAndDefaults(
    const std::string& printer_name,
    PrinterCapsAndDefaults* printer_info) {
  ScopedXPSInitializer xps_initializer;
  if (!xps_initializer.initialized()) {
    // TODO(sanjeevr): Handle legacy proxy case (with no prntvpt.dll)
    return false;
  }
  if (!IsValidPrinter(printer_name)) {
    return false;
  }
  DCHECK(printer_info);
  HPTPROVIDER provider = NULL;
  std::wstring printer_name_wide = UTF8ToWide(printer_name);
  HRESULT hr = XPSModule::OpenProvider(printer_name_wide, 1, &provider);
  DCHECK(SUCCEEDED(hr));
  if (provider) {
    base::win::ScopedComPtr<IStream> print_capabilities_stream;
    hr = CreateStreamOnHGlobal(NULL, TRUE,
                               print_capabilities_stream.Receive());
    DCHECK(SUCCEEDED(hr));
    if (print_capabilities_stream) {
      base::win::ScopedBstr error;
      hr = XPSModule::GetPrintCapabilities(provider,
                                           NULL,
                                           print_capabilities_stream,
                                           error.Receive());
      DCHECK(SUCCEEDED(hr));
      if (FAILED(hr)) {
        return false;
      }
      hr = StreamOnHGlobalToString(print_capabilities_stream.get(),
                                   &printer_info->printer_capabilities);
      DCHECK(SUCCEEDED(hr));
      printer_info->caps_mime_type = "text/xml";
    }
    ScopedPrinterHandle printer_handle;
    OpenPrinter(const_cast<LPTSTR>(printer_name_wide.c_str()),
                printer_handle.Receive(), NULL);
    DCHECK(printer_handle);
    if (printer_handle.IsValid()) {
      LONG devmode_size = DocumentProperties(
          NULL, printer_handle, const_cast<LPTSTR>(printer_name_wide.c_str()),
          NULL, NULL, 0);
      if (devmode_size <= 0)
        return false;
      scoped_array<BYTE> devmode_out_buffer(new BYTE[devmode_size]);
      DEVMODE* devmode_out =
          reinterpret_cast<DEVMODE*>(devmode_out_buffer.get());
      DocumentProperties(
          NULL, printer_handle, const_cast<LPTSTR>(printer_name_wide.c_str()),
          devmode_out, NULL, DM_OUT_BUFFER);
      base::win::ScopedComPtr<IStream> printer_defaults_stream;
      hr = CreateStreamOnHGlobal(NULL, TRUE,
                                 printer_defaults_stream.Receive());
      DCHECK(SUCCEEDED(hr));
      if (printer_defaults_stream) {
        hr = XPSModule::ConvertDevModeToPrintTicket(provider,
                                                    devmode_size,
                                                    devmode_out,
                                                    kPTJobScope,
                                                    printer_defaults_stream);
        DCHECK(SUCCEEDED(hr));
        if (SUCCEEDED(hr)) {
          hr = StreamOnHGlobalToString(printer_defaults_stream.get(),
                                       &printer_info->printer_defaults);
          DCHECK(SUCCEEDED(hr));
          printer_info->defaults_mime_type = "text/xml";
        }
      }
    }
    XPSModule::CloseProvider(provider);
  }
  return true;
}

// Gets the information about driver for a specific printer.
bool PrintBackendWin::GetPrinterDriverInfo(const std::string& printer_name,
                                           PrinterDriverInfo* driver_info) {
  DCHECK(driver_info);
  ScopedPrinterHandle printer_handle;
  if (!::OpenPrinter(const_cast<LPTSTR>(UTF8ToWide(printer_name).c_str()),
                     printer_handle.Receive(), NULL)) {
    return false;
  }
  DCHECK(printer_handle.IsValid());
  DWORD bytes_needed = 0;
  ::GetPrinterDriver(printer_handle, NULL, 6, NULL, 0, &bytes_needed);
  scoped_array<BYTE> driver_info_buffer(new BYTE[bytes_needed]);
  if (!bytes_needed || !driver_info_buffer.get())
    return false;
  if (!::GetPrinterDriver(printer_handle, NULL, 6, driver_info_buffer.get(),
                          bytes_needed, &bytes_needed)) {
    return false;
  }
  if (!bytes_needed)
    return false;
  const DRIVER_INFO_6* driver_info_6 =
      reinterpret_cast<DRIVER_INFO_6*>(driver_info_buffer.get());

  if (driver_info_6->pName)
    driver_info->driver_name = WideToUTF8(driver_info_6->pName);

  if (driver_info_6->pDriverPath) {
    scoped_ptr<FileVersionInfo> version_info(
        FileVersionInfo::CreateFileVersionInfo(
            FilePath(driver_info_6->pDriverPath)));
    driver_info->driver_version = WideToUTF8(version_info->file_version());
    driver_info->product_name = WideToUTF8(version_info->product_name());
    driver_info->product_version = WideToUTF8(version_info->product_version());
  }

  return true;
}

bool PrintBackendWin::IsValidPrinter(const std::string& printer_name) {
  ScopedPrinterHandle printer_handle;
  OpenPrinter(const_cast<LPTSTR>(UTF8ToWide(printer_name).c_str()),
              printer_handle.Receive(), NULL);
  return printer_handle.IsValid();
}

scoped_refptr<PrintBackend> PrintBackend::CreateInstance(
    const base::DictionaryValue* print_backend_settings) {
  return new PrintBackendWin;
}

}  // namespace printing
