// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/backend/print_backend.h"

#include <objidl.h>
#include <winspool.h>

#include "base/memory/scoped_ptr.h"
#include "base/string_piece.h"
#include "base/utf_string_conversions.h"
#include "base/win/scoped_bstr.h"
#include "base/win/scoped_comptr.h"
#include "base/win/scoped_hglobal.h"
#include "printing/backend/print_backend_consts.h"
#include "printing/backend/win_helper.h"

namespace {

// This class is designed to work with PRINTER_INFO_X structures
// and calls GetPrinter internally with correctly allocated buffer.
template <typename T>
class PrinterInfo {
 public:
  bool GetPrinterInfo(HANDLE printer, int level) {
    DWORD buf_size = 0;
    GetPrinter(printer, level, NULL, 0, &buf_size);
    if (buf_size == 0)
      return false;
    buffer_.reset(new uint8[buf_size]);
    memset(buffer_.get(), 0, buf_size);
    return !!GetPrinter(printer, level, buffer_.get(), buf_size, &buf_size);
  }

  const T* get() const {
    return reinterpret_cast<T*>(buffer_.get());
  }

 private:
  scoped_array<uint8> buffer_;
};

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

  // PrintBackend implementation.
  virtual bool EnumeratePrinters(PrinterList* printer_list) OVERRIDE;
  virtual std::string GetDefaultPrinterName() OVERRIDE;
  virtual bool GetPrinterSemanticCapsAndDefaults(
      const std::string& printer_name,
      PrinterSemanticCapsAndDefaults* printer_info) OVERRIDE;
  virtual bool GetPrinterCapsAndDefaults(
      const std::string& printer_name,
      PrinterCapsAndDefaults* printer_info) OVERRIDE;
  virtual std::string GetPrinterDriverInfo(
      const std::string& printer_name) OVERRIDE;
  virtual bool IsValidPrinter(const std::string& printer_name) OVERRIDE;

 protected:
  virtual ~PrintBackendWin() {}
};

bool PrintBackendWin::EnumeratePrinters(PrinterList* printer_list) {
  DCHECK(printer_list);
  DWORD bytes_needed = 0;
  DWORD count_returned = 0;
  const DWORD kLevel = 4;
  BOOL ret = EnumPrinters(PRINTER_ENUM_LOCAL|PRINTER_ENUM_CONNECTIONS, NULL,
                          kLevel, NULL, 0, &bytes_needed, &count_returned);
  if (!bytes_needed)
    return false;
  scoped_array<BYTE> printer_info_buffer(new BYTE[bytes_needed]);
  ret = EnumPrinters(PRINTER_ENUM_LOCAL|PRINTER_ENUM_CONNECTIONS, NULL, kLevel,
                     printer_info_buffer.get(), bytes_needed, &bytes_needed,
                     &count_returned);
  DCHECK(ret);
  if (!ret)
    return false;

  std::string default_printer = GetDefaultPrinterName();
  PRINTER_INFO_4* printer_info =
      reinterpret_cast<PRINTER_INFO_4*>(printer_info_buffer.get());
  for (DWORD index = 0; index < count_returned; index++) {
    ScopedPrinterHandle printer;
    OpenPrinter(printer_info[index].pPrinterName, printer.Receive(), NULL);
    PrinterBasicInfo info;
    if (InitBasicPrinterInfo(printer, &info)) {
      info.is_default = (info.printer_name == default_printer);
      printer_list->push_back(info);
    }
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

bool PrintBackendWin::GetPrinterSemanticCapsAndDefaults(
    const std::string& printer_name,
    PrinterSemanticCapsAndDefaults* printer_info) {
  ScopedPrinterHandle printer_handle;
  OpenPrinter(const_cast<LPTSTR>(UTF8ToWide(printer_name).c_str()),
              printer_handle.Receive(), NULL);
  DCHECK(printer_handle);
  if (!printer_handle.IsValid())
    return false;

  PrinterInfo<PRINTER_INFO_5> info_5;
  if (!info_5.GetPrinterInfo(printer_handle, 5))
    return false;

  // Get printer capabilities. For more info see here:
  // http://msdn.microsoft.com/en-us/library/windows/desktop/dd183552(v=vs.85).aspx
  bool color_supported = (DeviceCapabilities(info_5.get()->pPrinterName,
                                             info_5.get()->pPortName,
                                             DC_COLORDEVICE,
                                             NULL,
                                             NULL) == 1);

  bool duplex_supported = (DeviceCapabilities(info_5.get()->pPrinterName,
                                              info_5.get()->pPortName,
                                              DC_DUPLEX,
                                              NULL,
                                              NULL) == 1);

  // PRINTER_INFO_9 retrieves current user settings.
  PrinterInfo<PRINTER_INFO_9> info_9;
  if (!info_9.GetPrinterInfo(printer_handle, 9))
    return false;
  DEVMODE* devmode = info_9.get()->pDevMode;

  // Sometimes user settings are not available (have not been setted up yet).
  // Use printer default settings (PRINTER_INFO_8) in this case.
  PrinterInfo<PRINTER_INFO_8> info_8;
  if (!devmode) {
    if (info_8.GetPrinterInfo(printer_handle, 8))
      devmode = info_8.get()->pDevMode;
  }
  if (!devmode)
    return false;

  PrinterSemanticCapsAndDefaults caps;
  caps.color_capable = color_supported;
  if ((devmode->dmFields & DM_COLOR) == DM_COLOR)
    caps.color_default = (devmode->dmColor == DMCOLOR_COLOR);

  caps.duplex_capable = duplex_supported;
  if ((devmode->dmFields & DM_DUPLEX) == DM_DUPLEX) {
    switch (devmode->dmDuplex) {
      case DMDUP_SIMPLEX:
        caps.duplex_default = SIMPLEX;
        break;
      case DMDUP_VERTICAL:
        caps.duplex_default = LONG_EDGE;
        break;
      case DMDUP_HORIZONTAL:
        caps.duplex_default = SHORT_EDGE;
        break;
      default:
        NOTREACHED();
    }
  }

  *printer_info = caps;
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
std::string PrintBackendWin::GetPrinterDriverInfo(
    const std::string& printer_name) {
  ScopedPrinterHandle printer;
  if (!::OpenPrinter(const_cast<LPTSTR>(UTF8ToWide(printer_name).c_str()),
                     printer.Receive(), NULL)) {
    return std::string();
  }
  return GetDriverInfo(printer);
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
