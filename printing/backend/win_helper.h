// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PRINTING_BACKEND_WIN_HELPER_H_
#define PRINTING_BACKEND_WIN_HELPER_H_
#pragma once

#include <objidl.h>
#include <winspool.h>
#include <prntvpt.h>

#include "base/string16.h"

// These are helper functions for dealing with Windows Printing.
namespace printing {

// Wrapper class to wrap the XPS APIs (PTxxx APIs). This is needed because these
// APIs are not available by default on XP. We could delayload prntvpt.dll but
// this would mean having to add that to every binary that links with
// printing.lib (which is a LOT of binaries). So choosing the GetProcAddress
// route instead).
class XPSModule {
 public:
  // All the other methods can ONLY be called after a successful call to Init.
  // Init can be called many times and by multiple threads.
  static bool Init();
  static HRESULT OpenProvider(const string16& printer_name,
                              DWORD version,
                              HPTPROVIDER *provider);
  static HRESULT GetPrintCapabilities(HPTPROVIDER provider,
                                      IStream *print_ticket,
                                      IStream *capabilities,
                                      BSTR* error_message);
  static HRESULT ConvertDevModeToPrintTicket(HPTPROVIDER provider,
                                             ULONG devmode_size_in_bytes,
                                             PDEVMODE devmode,
                                             EPrintTicketScope scope,
                                             IStream* print_ticket);
  static HRESULT ConvertPrintTicketToDevMode(
      HPTPROVIDER provider,
      IStream* print_ticket,
      EDefaultDevmodeType base_devmode_type,
      EPrintTicketScope scope,
      ULONG* devmode_byte_count,
      PDEVMODE *devmode,
      BSTR* error_message);
  static HRESULT MergeAndValidatePrintTicket(HPTPROVIDER provider,
                                             IStream* base_ticket,
                                             IStream* delta_ticket,
                                             EPrintTicketScope scope,
                                             IStream* result_ticket,
                                             BSTR* error_message);
  static HRESULT ReleaseMemory(PVOID buffer);
  static HRESULT CloseProvider(HPTPROVIDER provider);
 private:
  XPSModule() { }
  static bool InitImpl();
};

}  // namespace printing

#endif  // PRINTING_BACKEND_WIN_HELPER_H_
