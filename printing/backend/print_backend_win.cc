// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/backend/print_backend.h"

#include <objidl.h>
#include <winspool.h>

#include "base/scoped_ptr.h"
#include "base/string_piece.h"
#include "base/utf_string_conversions.h"
#include "base/win/scoped_bstr.h"
#include "base/win/scoped_comptr.h"
#include "base/win/scoped_hglobal.h"
#include "printing/backend/print_backend_consts.h"
#include "printing/backend/win_helper.h"

using base::win::ScopedBstr;
using base::win::ScopedComPtr;

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

  virtual bool GetPrinterCapsAndDefaults(const std::string& printer_name,
                                         PrinterCapsAndDefaults* printer_info);

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
  scoped_ptr<BYTE> printer_info_buffer(new BYTE[bytes_needed]);
  ret = EnumPrinters(PRINTER_ENUM_LOCAL|PRINTER_ENUM_CONNECTIONS, NULL, 2,
                     printer_info_buffer.get(), bytes_needed, &bytes_needed,
                     &count_returned);
  DCHECK(ret);
  if (!ret)
    return false;

  PRINTER_INFO_2* printer_info =
      reinterpret_cast<PRINTER_INFO_2*>(printer_info_buffer.get());
  for (DWORD index = 0; index < count_returned; index++) {
    PrinterBasicInfo info;
    info.printer_name = WideToUTF8(printer_info[index].pPrinterName);
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
    ScopedComPtr<IStream> print_capabilities_stream;
    hr = CreateStreamOnHGlobal(NULL, TRUE,
                               print_capabilities_stream.Receive());
    DCHECK(SUCCEEDED(hr));
    if (print_capabilities_stream) {
      ScopedBstr error;
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
    // TODO(sanjeevr): Add ScopedPrinterHandle
    HANDLE printer_handle = NULL;
    OpenPrinter(const_cast<LPTSTR>(printer_name_wide.c_str()), &printer_handle,
                NULL);
    DCHECK(printer_handle);
    if (printer_handle) {
      DWORD devmode_size = DocumentProperties(
          NULL, printer_handle, const_cast<LPTSTR>(printer_name_wide.c_str()),
          NULL, NULL, 0);
      DCHECK_NE(0U, devmode_size);
      scoped_ptr<BYTE> devmode_out_buffer(new BYTE[devmode_size]);
      DEVMODE* devmode_out =
          reinterpret_cast<DEVMODE*>(devmode_out_buffer.get());
      DocumentProperties(
          NULL, printer_handle, const_cast<LPTSTR>(printer_name_wide.c_str()),
          devmode_out, NULL, DM_OUT_BUFFER);
      ScopedComPtr<IStream> printer_defaults_stream;
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
      ClosePrinter(printer_handle);
    }
    XPSModule::CloseProvider(provider);
  }
  return true;
}

bool PrintBackendWin::IsValidPrinter(const std::string& printer_name) {
  std::wstring printer_name_wide = UTF8ToWide(printer_name);
  HANDLE printer_handle = NULL;
  OpenPrinter(const_cast<LPTSTR>(printer_name_wide.c_str()), &printer_handle,
              NULL);
  bool ret = false;
  if (printer_handle) {
    ret = true;
    ClosePrinter(printer_handle);
  }
  return ret;
}

scoped_refptr<PrintBackend> PrintBackend::CreateInstance(
    const DictionaryValue* print_backend_settings) {
  return new PrintBackendWin;
}

}  // namespace printing
