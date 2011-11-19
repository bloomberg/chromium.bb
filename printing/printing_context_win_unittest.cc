// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <ocidl.h>
#include <commdlg.h>

#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/memory/scoped_ptr.h"
#include "printing/printing_test.h"
#include "printing/printing_context.h"
#include "printing/printing_context_win.h"
#include "printing/print_settings.h"
#include "testing/gtest/include/gtest/gtest.h"

// This test is automatically disabled if no printer is available.
class PrintingContextTest : public PrintingTest<testing::Test> {
 public:
  void PrintSettingsCallback(printing::PrintingContext::Result result) {
    result_ = result;
  }

 protected:
  printing::PrintingContext::Result result() const { return result_; }

 private:
  printing::PrintingContext::Result result_;
};

// This is a fake PrintDlgEx implementation that sets the right fields in
// |lppd| to trigger a bug in older revisions of PrintingContext.
HRESULT WINAPI PrintDlgExMock(LPPRINTDLGEX lppd) {
  // The interesting bits:
  // Pretend the user hit print
  lppd->dwResultAction = PD_RESULT_PRINT;

  // Pretend the page range is 1-5, but since lppd->Flags does not have
  // PD_SELECTION set, this really shouldn't matter.
  lppd->nPageRanges = 1;
  lppd->lpPageRanges[0].nFromPage = 1;
  lppd->lpPageRanges[0].nToPage = 5;

  // Painful paperwork.
  std::wstring printer_name = PrintingContextTest::GetDefaultPrinter();
  HANDLE printer;
  if (!OpenPrinter(const_cast<wchar_t*>(printer_name.c_str()), &printer, NULL))
    return E_FAIL;

  scoped_array<uint8> buffer;
  DEVMODE* dev_mode = NULL;
  PRINTER_INFO_2* info_2 = NULL;
  HRESULT result = S_OK;
  lppd->hDC = NULL;
  lppd->hDevMode = NULL;
  lppd->hDevNames = NULL;

  printing::PrintingContextWin::GetPrinterHelper(printer, 2, &buffer);
  if (buffer.get()) {
    info_2 = reinterpret_cast<PRINTER_INFO_2*>(buffer.get());
    if (info_2->pDevMode != NULL)
      dev_mode = info_2->pDevMode;
  }
  if (!dev_mode) {
    result = E_FAIL;
    goto Cleanup;
  }

  if (!printing::PrintingContextWin::AllocateContext(printer_name, dev_mode,
      &lppd->hDC)) {
    result = E_FAIL;
    goto Cleanup;
  }

  size_t dev_mode_size = dev_mode->dmSize + dev_mode->dmDriverExtra;
  lppd->hDevMode = GlobalAlloc(GMEM_MOVEABLE, dev_mode_size);
  if (!lppd->hDevMode) {
    result = E_FAIL;
    goto Cleanup;
  }
  void* dev_mode_ptr = GlobalLock(lppd->hDevMode);
  if (!dev_mode_ptr) {
    result = E_FAIL;
    goto Cleanup;
  }
  memcpy(dev_mode_ptr, dev_mode, dev_mode_size);
  GlobalUnlock(lppd->hDevMode);
  dev_mode_ptr = NULL;

  size_t driver_size = 2 + sizeof(wchar_t) * lstrlen(info_2->pDriverName);
  size_t printer_size = 2 + sizeof(wchar_t) * lstrlen(info_2->pPrinterName);
  size_t port_size = 2 + sizeof(wchar_t) * lstrlen(info_2->pPortName);
  size_t dev_names_size = sizeof(DEVNAMES) + driver_size + printer_size +
      port_size;
  lppd->hDevNames = GlobalAlloc(GHND, dev_names_size);
  if (!lppd->hDevNames) {
    result = E_FAIL;
    goto Cleanup;
  }
  void* dev_names_ptr = GlobalLock(lppd->hDevNames);
  if (!dev_names_ptr) {
    result = E_FAIL;
    goto Cleanup;
  }
  DEVNAMES* dev_names = reinterpret_cast<DEVNAMES*>(dev_names_ptr);
  dev_names->wDefault = 1;
  dev_names->wDriverOffset = sizeof(DEVNAMES);
  memcpy(reinterpret_cast<uint8*>(dev_names_ptr) + dev_names->wDriverOffset,
      info_2->pDriverName, driver_size);
  dev_names->wDeviceOffset = dev_names->wDriverOffset + driver_size;
  memcpy(reinterpret_cast<uint8*>(dev_names_ptr) + dev_names->wDeviceOffset,
      info_2->pPrinterName, printer_size);
  dev_names->wOutputOffset = dev_names->wDeviceOffset + printer_size;
  memcpy(reinterpret_cast<uint8*>(dev_names_ptr) + dev_names->wOutputOffset,
      info_2->pPortName, port_size);
  GlobalUnlock(lppd->hDevNames);
  dev_names_ptr = NULL;

Cleanup:
  // Note: This section does proper deallocation/free of DC/global handles.  We
  //       did not use ScopedHGlobal or ScopedHandle because they did not
  //       perform what we need.  Goto's are used based on Windows programming
  //       idiom, to avoid deeply nested if's, and try-catch-finally is not
  //       allowed in Chromium.
  if (FAILED(result)) {
    if (lppd->hDC) {
      DeleteDC(lppd->hDC);
    }
    if (lppd->hDevMode) {
      GlobalFree(lppd->hDevMode);
    }
    if (lppd->hDevNames) {
      GlobalFree(lppd->hDevNames);
    }
  }
  ClosePrinter(printer);
  return result;
}

TEST_F(PrintingContextTest, Base) {
  // Sometimes ::GetDefaultPrinter() fails? bug 61509.
  if (IsTestCaseDisabled())
    return;

  printing::PrintSettings settings;
  settings.set_device_name(GetDefaultPrinter());
  // Initialize it.
  scoped_ptr<printing::PrintingContext> context(
      printing::PrintingContext::Create(std::string()));
  EXPECT_EQ(printing::PrintingContext::OK, context->InitWithSettings(settings));

  // The print may lie to use and may not support world transformation.
  // Verify right now.
  XFORM random_matrix = { 1, 0.1f, 0, 1.5f, 0, 1 };
  EXPECT_TRUE(SetWorldTransform(context->context(), &random_matrix));
  EXPECT_TRUE(ModifyWorldTransform(context->context(), NULL, MWT_IDENTITY));
}

TEST_F(PrintingContextTest, PrintAll) {
  // Sometimes ::GetDefaultPrinter() fails? bug 61509.
  if (IsTestCaseDisabled())
    return;

  std::string dummy_locale;
  printing::PrintingContextWin context(dummy_locale);
  context.SetPrintDialog(&PrintDlgExMock);
  context.AskUserForSettings(
      NULL, 123, false, base::Bind(&PrintingContextTest::PrintSettingsCallback,
                                   base::Unretained(this)));
  EXPECT_EQ(printing::PrintingContext::OK, result());
  printing::PrintSettings settings = context.settings();
  EXPECT_EQ(settings.ranges.size(), 0);
}
