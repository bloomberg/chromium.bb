// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/printing_context_win.h"

#include <winspool.h>

#include "base/i18n/file_util_icu.h"
#include "base/i18n/time_formatting.h"
#include "base/message_loop.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "printing/print_settings_initializer_win.h"
#include "printing/printed_document.h"
#include "skia/ext/platform_device_win.h"

using base::Time;

namespace printing {

class PrintingContextWin::CallbackHandler : public IPrintDialogCallback,
                                            public IObjectWithSite {
 public:
  CallbackHandler(PrintingContextWin& owner, HWND owner_hwnd)
      : owner_(owner),
        owner_hwnd_(owner_hwnd),
        services_(NULL) {
  }

  ~CallbackHandler() {
    if (services_)
      services_->Release();
  }

  IUnknown* ToIUnknown() {
    return static_cast<IUnknown*>(static_cast<IPrintDialogCallback*>(this));
  }

  // IUnknown
  virtual HRESULT WINAPI QueryInterface(REFIID riid, void**object) {
    if (riid == IID_IUnknown) {
      *object = ToIUnknown();
    } else if (riid == IID_IPrintDialogCallback) {
      *object = static_cast<IPrintDialogCallback*>(this);
    } else if (riid == IID_IObjectWithSite) {
      *object = static_cast<IObjectWithSite*>(this);
    } else {
      return E_NOINTERFACE;
    }
    return S_OK;
  }

  // No real ref counting.
  virtual ULONG WINAPI AddRef() {
    return 1;
  }
  virtual ULONG WINAPI Release() {
    return 1;
  }

  // IPrintDialogCallback methods
  virtual HRESULT WINAPI InitDone() {
    return S_OK;
  }

  virtual HRESULT WINAPI SelectionChange() {
    if (services_) {
      // TODO(maruel): Get the devmode for the new printer with
      // services_->GetCurrentDevMode(&devmode, &size), send that information
      // back to our client and continue. The client needs to recalculate the
      // number of rendered pages and send back this information here.
    }
    return S_OK;
  }

  virtual HRESULT WINAPI HandleMessage(HWND dialog,
                                       UINT message,
                                       WPARAM wparam,
                                       LPARAM lparam,
                                       LRESULT* result) {
    // Cheap way to retrieve the window handle.
    if (!owner_.dialog_box_) {
      // The handle we receive is the one of the groupbox in the General tab. We
      // need to get the grand-father to get the dialog box handle.
      owner_.dialog_box_ = GetAncestor(dialog, GA_ROOT);
      // Trick to enable the owner window. This can cause issues with navigation
      // events so it may have to be disabled if we don't fix the side-effects.
      EnableWindow(owner_hwnd_, TRUE);
    }
    return S_FALSE;
  }

  virtual HRESULT WINAPI SetSite(IUnknown* site) {
    if (!site) {
      DCHECK(services_);
      services_->Release();
      services_ = NULL;
      // The dialog box is destroying, PrintJob::Worker don't need the handle
      // anymore.
      owner_.dialog_box_ = NULL;
    } else {
      DCHECK(services_ == NULL);
      HRESULT hr = site->QueryInterface(IID_IPrintDialogServices,
                                        reinterpret_cast<void**>(&services_));
      DCHECK(SUCCEEDED(hr));
    }
    return S_OK;
  }

  virtual HRESULT WINAPI GetSite(REFIID riid, void** site) {
    return E_NOTIMPL;
  }

 private:
  PrintingContextWin& owner_;
  HWND owner_hwnd_;
  IPrintDialogServices* services_;

  DISALLOW_COPY_AND_ASSIGN(CallbackHandler);
};

// static
PrintingContext* PrintingContext::Create(const std::string& app_locale) {
  return static_cast<PrintingContext*>(new PrintingContextWin(app_locale));
}

PrintingContextWin::PrintingContextWin(const std::string& app_locale)
    : PrintingContext(app_locale),
      context_(NULL),
      dialog_box_(NULL),
      print_dialog_func_(&PrintDlgEx) {
}

PrintingContextWin::~PrintingContextWin() {
  ReleaseContext();
}

void PrintingContextWin::AskUserForSettings(HWND view,
                                            int max_pages,
                                            bool has_selection,
                                            PrintSettingsCallback* callback) {
  DCHECK(!in_print_job_);
  dialog_box_dismissed_ = false;

  HWND window;
  if (!view || !IsWindow(view)) {
    // TODO(maruel):  bug 1214347 Get the right browser window instead.
    window = GetDesktopWindow();
  } else {
    window = GetAncestor(view, GA_ROOTOWNER);
  }
  DCHECK(window);

  // Show the OS-dependent dialog box.
  // If the user press
  // - OK, the settings are reset and reinitialized with the new settings. OK is
  //   returned.
  // - Apply then Cancel, the settings are reset and reinitialized with the new
  //   settings. CANCEL is returned.
  // - Cancel, the settings are not changed, the previous setting, if it was
  //   initialized before, are kept. CANCEL is returned.
  // On failure, the settings are reset and FAILED is returned.
  PRINTDLGEX dialog_options = { sizeof(PRINTDLGEX) };
  dialog_options.hwndOwner = window;
  // Disable options we don't support currently.
  // TODO(maruel):  Reuse the previously loaded settings!
  dialog_options.Flags = PD_RETURNDC | PD_USEDEVMODECOPIESANDCOLLATE  |
                         PD_NOCURRENTPAGE | PD_HIDEPRINTTOFILE;
  if (!has_selection)
    dialog_options.Flags |= PD_NOSELECTION;

  PRINTPAGERANGE ranges[32];
  dialog_options.nStartPage = START_PAGE_GENERAL;
  if (max_pages) {
    // Default initialize to print all the pages.
    memset(ranges, 0, sizeof(ranges));
    ranges[0].nFromPage = 1;
    ranges[0].nToPage = max_pages;
    dialog_options.nPageRanges = 1;
    dialog_options.nMaxPageRanges = arraysize(ranges);
    dialog_options.nMinPage = 1;
    dialog_options.nMaxPage = max_pages;
    dialog_options.lpPageRanges = ranges;
  } else {
    // No need to bother, we don't know how many pages are available.
    dialog_options.Flags |= PD_NOPAGENUMS;
  }

  if ((*print_dialog_func_)(&dialog_options) != S_OK) {
    ResetSettings();
    callback->Run(FAILED);
  }

  // TODO(maruel):  Support PD_PRINTTOFILE.
  callback->Run(ParseDialogResultEx(dialog_options));
}

PrintingContext::Result PrintingContextWin::UseDefaultSettings() {
  DCHECK(!in_print_job_);

  PRINTDLG dialog_options = { sizeof(PRINTDLG) };
  dialog_options.Flags = PD_RETURNDC | PD_RETURNDEFAULT;
  if (PrintDlg(&dialog_options) == 0) {
    ResetSettings();
    return FAILED;
  }
  return ParseDialogResult(dialog_options);
}

PrintingContext::Result PrintingContextWin::InitWithSettings(
    const PrintSettings& settings) {
  DCHECK(!in_print_job_);

  settings_ = settings;

  // TODO(maruel): settings_.ToDEVMODE()
  HANDLE printer;
  if (!OpenPrinter(const_cast<wchar_t*>(settings_.device_name().c_str()),
                   &printer,
                   NULL))
    return FAILED;

  Result status = OK;

  if (!GetPrinterSettings(printer, settings_.device_name()))
    status = FAILED;

  // Close the printer after retrieving the context.
  ClosePrinter(printer);

  if (status != OK)
    ResetSettings();
  return status;
}

PrintingContext::Result PrintingContextWin::NewDocument(
    const string16& document_name) {
  DCHECK(!in_print_job_);
  if (!context_)
    return OnError();

  // Set the flag used by the AbortPrintJob dialog procedure.
  abort_printing_ = false;

  in_print_job_ = true;

  // Register the application's AbortProc function with GDI.
  if (SP_ERROR == SetAbortProc(context_, &AbortProc))
    return OnError();

  DOCINFO di = { sizeof(DOCINFO) };
  const std::wstring& document_name_wide = UTF16ToWide(document_name);
  di.lpszDocName = document_name_wide.c_str();

  // Is there a debug dump directory specified? If so, force to print to a file.
  FilePath debug_dump_path = PrintedDocument::debug_dump_path();
  if (!debug_dump_path.empty()) {
    // Create a filename.
    std::wstring filename;
    Time now(Time::Now());
    filename = base::TimeFormatShortDateNumeric(now);
    filename += L"_";
    filename += base::TimeFormatTimeOfDay(now);
    filename += L"_";
    filename += UTF16ToWide(document_name);
    filename += L"_";
    filename += L"buffer.prn";
    file_util::ReplaceIllegalCharactersInPath(&filename, '_');
    debug_dump_path.Append(filename);
    di.lpszOutput = debug_dump_path.value().c_str();
  }

  // No message loop running in unit tests.
  DCHECK(!MessageLoop::current() ? true :
      !MessageLoop::current()->NestableTasksAllowed());

  // Begin a print job by calling the StartDoc function.
  // NOTE: StartDoc() starts a message loop. That causes a lot of problems with
  // IPC. Make sure recursive task processing is disabled.
  if (StartDoc(context_, &di) <= 0)
    return OnError();

  return OK;
}

PrintingContext::Result PrintingContextWin::NewPage() {
  if (abort_printing_)
    return CANCEL;

  DCHECK(context_);
  DCHECK(in_print_job_);

  // Inform the driver that the application is about to begin sending data.
  if (StartPage(context_) <= 0)
    return OnError();

  return OK;
}

PrintingContext::Result PrintingContextWin::PageDone() {
  if (abort_printing_)
    return CANCEL;
  DCHECK(in_print_job_);

  if (EndPage(context_) <= 0)
    return OnError();
  return OK;
}

PrintingContext::Result PrintingContextWin::DocumentDone() {
  if (abort_printing_)
    return CANCEL;
  DCHECK(in_print_job_);
  DCHECK(context_);

  // Inform the driver that document has ended.
  if (EndDoc(context_) <= 0)
    return OnError();

  ResetSettings();
  return OK;
}

void PrintingContextWin::Cancel() {
  abort_printing_ = true;
  in_print_job_ = false;
  if (context_)
    CancelDC(context_);
  DismissDialog();
}

void PrintingContextWin::DismissDialog() {
  if (dialog_box_) {
    DestroyWindow(dialog_box_);
    dialog_box_dismissed_ = true;
  }
}

void PrintingContextWin::ReleaseContext() {
  if (context_) {
    DeleteDC(context_);
    context_ = NULL;
  }
}

gfx::NativeDrawingContext PrintingContextWin::context() const {
  return context_;
}

// static
BOOL PrintingContextWin::AbortProc(HDC hdc, int nCode) {
  if (nCode) {
    // TODO(maruel):  Need a way to find the right instance to set. Should
    // leverage PrintJobManager here?
    // abort_printing_ = true;
  }
  return true;
}

bool PrintingContextWin::InitializeSettings(const DEVMODE& dev_mode,
                                            const std::wstring& new_device_name,
                                            const PRINTPAGERANGE* ranges,
                                            int number_ranges,
                                            bool selection_only) {
  skia::PlatformDevice::InitializeDC(context_);
  DCHECK(GetDeviceCaps(context_, CLIPCAPS));
  DCHECK(GetDeviceCaps(context_, RASTERCAPS) & RC_STRETCHDIB);
  DCHECK(GetDeviceCaps(context_, RASTERCAPS) & RC_BITMAP64);
  // Some printers don't advertise these.
  // DCHECK(GetDeviceCaps(context_, RASTERCAPS) & RC_SCALING);
  // DCHECK(GetDeviceCaps(context_, SHADEBLENDCAPS) & SB_CONST_ALPHA);
  // DCHECK(GetDeviceCaps(context_, SHADEBLENDCAPS) & SB_PIXEL_ALPHA);

  // StretchDIBits() support is needed for printing.
  if (!(GetDeviceCaps(context_, RASTERCAPS) & RC_STRETCHDIB) ||
      !(GetDeviceCaps(context_, RASTERCAPS) & RC_BITMAP64)) {
    NOTREACHED();
    ResetSettings();
    return false;
  }

  DCHECK(!in_print_job_);
  DCHECK(context_);
  PageRanges ranges_vector;
  if (!selection_only) {
    // Convert the PRINTPAGERANGE array to a PrintSettings::PageRanges vector.
    ranges_vector.reserve(number_ranges);
    for (int i = 0; i < number_ranges; ++i) {
      PageRange range;
      // Transfer from 1-based to 0-based.
      range.from = ranges[i].nFromPage - 1;
      range.to = ranges[i].nToPage - 1;
      ranges_vector.push_back(range);
    }
  }

  PrintSettingsInitializerWin::InitPrintSettings(context_,
                                                 dev_mode,
                                                 ranges_vector,
                                                 new_device_name,
                                                 selection_only,
                                                 &settings_);

  return true;
}

bool PrintingContextWin::GetPrinterSettings(HANDLE printer,
                                            const std::wstring& device_name) {
  DCHECK(!in_print_job_);
  scoped_array<uint8> buffer;

  // A PRINTER_INFO_9 structure specifying the per-user default printer
  // settings.
  GetPrinterHelper(printer, 9, &buffer);
  if (buffer.get()) {
    PRINTER_INFO_9* info_9 = reinterpret_cast<PRINTER_INFO_9*>(buffer.get());
    if (info_9->pDevMode != NULL) {
      if (!AllocateContext(device_name, info_9->pDevMode, &context_)) {
        ResetSettings();
        return false;
      }
      return InitializeSettings(*info_9->pDevMode, device_name, NULL, 0, false);
    }
    buffer.reset();
  }

  // A PRINTER_INFO_8 structure specifying the global default printer settings.
  GetPrinterHelper(printer, 8, &buffer);
  if (buffer.get()) {
    PRINTER_INFO_8* info_8 = reinterpret_cast<PRINTER_INFO_8*>(buffer.get());
    if (info_8->pDevMode != NULL) {
      if (!AllocateContext(device_name, info_8->pDevMode, &context_)) {
        ResetSettings();
        return false;
      }
      return InitializeSettings(*info_8->pDevMode, device_name, NULL, 0, false);
    }
    buffer.reset();
  }

  // A PRINTER_INFO_2 structure specifying the driver's default printer
  // settings.
  GetPrinterHelper(printer, 2, &buffer);
  if (buffer.get()) {
    PRINTER_INFO_2* info_2 = reinterpret_cast<PRINTER_INFO_2*>(buffer.get());
    if (info_2->pDevMode != NULL) {
      if (!AllocateContext(device_name, info_2->pDevMode, &context_)) {
        ResetSettings();
        return false;
      }
      return InitializeSettings(*info_2->pDevMode, device_name, NULL, 0, false);
    }
    buffer.reset();
  }
  // Failed to retrieve the printer settings.
  ResetSettings();
  return false;
}

// static
bool PrintingContextWin::AllocateContext(const std::wstring& printer_name,
                                         const DEVMODE* dev_mode,
                                         gfx::NativeDrawingContext* context) {
  *context = CreateDC(L"WINSPOOL", printer_name.c_str(), NULL, dev_mode);
  DCHECK(*context);
  return *context != NULL;
}

PrintingContext::Result PrintingContextWin::ParseDialogResultEx(
    const PRINTDLGEX& dialog_options) {
  // If the user clicked OK or Apply then Cancel, but not only Cancel.
  if (dialog_options.dwResultAction != PD_RESULT_CANCEL) {
    // Start fresh.
    ResetSettings();

    DEVMODE* dev_mode = NULL;
    if (dialog_options.hDevMode) {
      dev_mode =
          reinterpret_cast<DEVMODE*>(GlobalLock(dialog_options.hDevMode));
      DCHECK(dev_mode);
    }

    std::wstring device_name;
    if (dialog_options.hDevNames) {
      DEVNAMES* dev_names =
          reinterpret_cast<DEVNAMES*>(GlobalLock(dialog_options.hDevNames));
      DCHECK(dev_names);
      if (dev_names) {
        device_name =
            reinterpret_cast<const wchar_t*>(
                reinterpret_cast<const wchar_t*>(dev_names) +
                    dev_names->wDeviceOffset);
        GlobalUnlock(dialog_options.hDevNames);
      }
    }

    bool success = false;
    if (dev_mode && !device_name.empty()) {
      context_ = dialog_options.hDC;
      PRINTPAGERANGE* page_ranges = NULL;
      DWORD num_page_ranges = 0;
      bool print_selection_only = false;
      if (dialog_options.Flags & PD_PAGENUMS) {
        page_ranges = dialog_options.lpPageRanges;
        num_page_ranges = dialog_options.nPageRanges;
      }
      if (dialog_options.Flags & PD_SELECTION) {
        print_selection_only = true;
      }
      success = InitializeSettings(*dev_mode,
                                   device_name,
                                   page_ranges,
                                   num_page_ranges,
                                   print_selection_only);
    }

    if (!success && dialog_options.hDC) {
      DeleteDC(dialog_options.hDC);
      context_ = NULL;
    }

    if (dev_mode) {
      GlobalUnlock(dialog_options.hDevMode);
    }
  } else {
    if (dialog_options.hDC) {
      DeleteDC(dialog_options.hDC);
    }
  }

  if (dialog_options.hDevMode != NULL)
    GlobalFree(dialog_options.hDevMode);
  if (dialog_options.hDevNames != NULL)
    GlobalFree(dialog_options.hDevNames);

  switch (dialog_options.dwResultAction) {
    case PD_RESULT_PRINT:
      return context_ ? OK : FAILED;
    case PD_RESULT_APPLY:
      return context_ ? CANCEL : FAILED;
    case PD_RESULT_CANCEL:
      return CANCEL;
    default:
      return FAILED;
  }
}

PrintingContext::Result PrintingContextWin::ParseDialogResult(
    const PRINTDLG& dialog_options) {
  // If the user clicked OK or Apply then Cancel, but not only Cancel.
  // Start fresh.
  ResetSettings();

  DEVMODE* dev_mode = NULL;
  if (dialog_options.hDevMode) {
    dev_mode =
        reinterpret_cast<DEVMODE*>(GlobalLock(dialog_options.hDevMode));
    DCHECK(dev_mode);
  }

  std::wstring device_name;
  if (dialog_options.hDevNames) {
    DEVNAMES* dev_names =
        reinterpret_cast<DEVNAMES*>(GlobalLock(dialog_options.hDevNames));
    DCHECK(dev_names);
    if (dev_names) {
      device_name =
          reinterpret_cast<const wchar_t*>(
              reinterpret_cast<const wchar_t*>(dev_names) +
                  dev_names->wDeviceOffset);
      GlobalUnlock(dialog_options.hDevNames);
    }
  }

  bool success = false;
  if (dev_mode && !device_name.empty()) {
    context_ = dialog_options.hDC;
    success = InitializeSettings(*dev_mode, device_name, NULL, 0, false);
  }

  if (!success && dialog_options.hDC) {
    DeleteDC(dialog_options.hDC);
    context_ = NULL;
  }

  if (dev_mode) {
    GlobalUnlock(dialog_options.hDevMode);
  }

  if (dialog_options.hDevMode != NULL)
    GlobalFree(dialog_options.hDevMode);
  if (dialog_options.hDevNames != NULL)
    GlobalFree(dialog_options.hDevNames);

  return context_ ? OK : FAILED;
}

// static
void PrintingContextWin::GetPrinterHelper(HANDLE printer, int level,
                                          scoped_array<uint8>* buffer) {
  DWORD buf_size = 0;
  GetPrinter(printer, level, NULL, 0, &buf_size);
  if (buf_size) {
    buffer->reset(new uint8[buf_size]);
    memset(buffer->get(), 0, buf_size);
    if (!GetPrinter(printer, level, buffer->get(), buf_size, &buf_size)) {
      buffer->reset();
    }
  }
}

}  // namespace printing
