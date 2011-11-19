// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/printing_context_win.h"

#include <winspool.h>

#include <algorithm>

#include "base/i18n/file_util_icu.h"
#include "base/i18n/time_formatting.h"
#include "base/message_loop.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "printing/print_job_constants.h"
#include "printing/print_settings_initializer_win.h"
#include "printing/printed_document.h"
#include "printing/units.h"
#include "skia/ext/platform_device.h"

using base::Time;

namespace {

// Constants for setting default PDF settings.
const int kPDFDpi = 300;  // 300 dpi
// LETTER: 8.5 x 11 inches
const int kPDFLetterWidth = 8.5 * kPDFDpi;
const int kPDFLetterHeight = 11 * kPDFDpi;
// LEGAL: 8.5 x 14 inches
const int kPDFLegalWidth = 8.5 * kPDFDpi;
const int kPDFLegalHeight = 14 * kPDFDpi;
// A4: 8.27 x 11.69 inches
const int kPDFA4Width = 8.27 * kPDFDpi;
const int kPDFA4Height = 11.69 * kPDFDpi;
// A3: 11.69 x 16.54 inches
const int kPDFA3Width = 11.69 * kPDFDpi;
const int kPDFA3Height = 16.54 * kPDFDpi;

// Retrieves the printer's PRINTER_INFO_* structure.
// Output |level| can be 9 (user-default), 8 (admin-default), or 2
// (printer-default).
// |devmode| is a pointer points to the start of DEVMODE structure in
// |buffer|.
bool GetPrinterInfo(HANDLE printer,
                    const std::wstring &device_name,
                    int* level,
                    scoped_array<uint8>* buffer,
                    DEVMODE** dev_mode) {
  DCHECK(buffer);

  // A PRINTER_INFO_9 structure specifying the per-user default printer
  // settings.
  printing::PrintingContextWin::GetPrinterHelper(printer, 9, buffer);
  if (buffer->get()) {
    PRINTER_INFO_9* info_9 = reinterpret_cast<PRINTER_INFO_9*>(buffer->get());
    if (info_9->pDevMode != NULL) {
      *level = 9;
      *dev_mode = info_9->pDevMode;
      return true;
    }
    buffer->reset();
  }

  // A PRINTER_INFO_8 structure specifying the global default printer settings.
  printing::PrintingContextWin::GetPrinterHelper(printer, 8, buffer);
  if (buffer->get()) {
    PRINTER_INFO_8* info_8 = reinterpret_cast<PRINTER_INFO_8*>(buffer->get());
    if (info_8->pDevMode != NULL) {
      *level = 8;
      *dev_mode = info_8->pDevMode;
      return true;
    }
    buffer->reset();
  }

  // A PRINTER_INFO_2 structure specifying the driver's default printer
  // settings.
  printing::PrintingContextWin::GetPrinterHelper(printer, 2, buffer);
  if (buffer->get()) {
    PRINTER_INFO_2* info_2 = reinterpret_cast<PRINTER_INFO_2*>(buffer->get());
    if (info_2->pDevMode != NULL) {
      *level = 2;
      *dev_mode = info_2->pDevMode;
      return true;
    }
    buffer->reset();
  }

  return false;
}

}  // anonymous namespace

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

void PrintingContextWin::AskUserForSettings(
    gfx::NativeView view, int max_pages, bool has_selection,
    const PrintSettingsCallback& callback) {
#if !defined(USE_AURA)
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
    callback.Run(FAILED);
  }

  // TODO(maruel):  Support PD_PRINTTOFILE.
  callback.Run(ParseDialogResultEx(dialog_options));
#endif
}

PrintingContext::Result PrintingContextWin::UseDefaultSettings() {
  DCHECK(!in_print_job_);

  PRINTDLG dialog_options = { sizeof(PRINTDLG) };
  dialog_options.Flags = PD_RETURNDC | PD_RETURNDEFAULT;
  if (PrintDlg(&dialog_options))
    return ParseDialogResult(dialog_options);

  // No default printer configured, do we have any printers at all?
  DWORD bytes_needed = 0;
  DWORD count_returned = 0;
  (void)::EnumPrinters(PRINTER_ENUM_LOCAL|PRINTER_ENUM_CONNECTIONS,
                       NULL, 2, NULL, 0, &bytes_needed, &count_returned);
  if (bytes_needed) {
    DCHECK(bytes_needed >= count_returned * sizeof(PRINTER_INFO_2));
    scoped_array<BYTE> printer_info_buffer(new BYTE[bytes_needed]);
    BOOL ret = ::EnumPrinters(PRINTER_ENUM_LOCAL|PRINTER_ENUM_CONNECTIONS,
                              NULL, 2, printer_info_buffer.get(),
                              bytes_needed, &bytes_needed,
                              &count_returned);
    if (ret && count_returned) {  // have printers
      // Open the first successfully found printer.
      for (DWORD count = 0; count < count_returned; ++count) {
        PRINTER_INFO_2* info_2 = reinterpret_cast<PRINTER_INFO_2*>(
            printer_info_buffer.get() + count * sizeof(PRINTER_INFO_2));
        std::wstring printer_name = info_2->pPrinterName;
        if (info_2->pDevMode == NULL || printer_name.length() == 0)
          continue;
        if (!AllocateContext(printer_name, info_2->pDevMode, &context_))
          break;
        if (InitializeSettings(*info_2->pDevMode, printer_name,
                               NULL, 0, false)) {
          break;
        }
        ReleaseContext();
      }
      if (context_)
        return OK;
    }
  }

  ResetSettings();
  return FAILED;
}

PrintingContext::Result PrintingContextWin::UpdatePrinterSettings(
    const DictionaryValue& job_settings,
    const PageRanges& ranges) {
  DCHECK(!in_print_job_);

  bool collate;
  int color;
  bool landscape;
  bool print_to_pdf;
  bool is_cloud_dialog;
  int copies;
  int duplex_mode;
  string16 device_name;

  if (!job_settings.GetBoolean(kSettingLandscape, &landscape) ||
      !job_settings.GetBoolean(kSettingCollate, &collate) ||
      !job_settings.GetInteger(kSettingColor, &color) ||
      !job_settings.GetBoolean(kSettingPrintToPDF, &print_to_pdf) ||
      !job_settings.GetInteger(kSettingDuplexMode, &duplex_mode) ||
      !job_settings.GetInteger(kSettingCopies, &copies) ||
      !job_settings.GetString(kSettingDeviceName, &device_name) ||
      !job_settings.GetBoolean(kSettingCloudPrintDialog, &is_cloud_dialog)) {
    return OnError();
  }

  bool print_to_cloud = job_settings.HasKey(kSettingCloudPrintId);

  if (print_to_pdf || print_to_cloud || is_cloud_dialog) {
    // Default fallback to Letter size.
    gfx::Size paper_size;
    gfx::Rect paper_rect;
    paper_size.SetSize(kPDFLetterWidth, kPDFLetterHeight);

    // Get settings from locale. Paper type buffer length is at most 4.
    const int paper_type_buffer_len = 4;
    wchar_t paper_type_buffer[paper_type_buffer_len] = {0};
    GetLocaleInfo(LOCALE_USER_DEFAULT, LOCALE_IPAPERSIZE, paper_type_buffer,
                  paper_type_buffer_len);
    if (wcslen(paper_type_buffer)) {  // The call succeeded.
      int paper_code = _wtoi(paper_type_buffer);
      switch (paper_code) {
        case DMPAPER_LEGAL:
          paper_size.SetSize(kPDFLegalWidth, kPDFLegalHeight);
          break;
        case DMPAPER_A4:
          paper_size.SetSize(kPDFA4Width, kPDFA4Height);
          break;
        case DMPAPER_A3:
          paper_size.SetSize(kPDFA3Width, kPDFA3Height);
          break;
        default:  // DMPAPER_LETTER is used for default fallback.
          break;
      }
    }
    paper_rect.SetRect(0, 0, paper_size.width(), paper_size.height());
    settings_.SetPrinterPrintableArea(paper_size, paper_rect, kPDFDpi);
    settings_.set_dpi(kPDFDpi);
    settings_.SetOrientation(landscape);
    settings_.ranges = ranges;
    return OK;
  }

  HANDLE printer;
  LPWSTR device_name_wide = const_cast<wchar_t*>(device_name.c_str());
  if (!OpenPrinter(device_name_wide, &printer, NULL))
    return OnError();

  // Make printer changes local to Chrome.
  // See MSDN documentation regarding DocumentProperties.
  scoped_array<uint8> buffer;
  DEVMODE* dev_mode = NULL;
  LONG buffer_size = DocumentProperties(NULL, printer, device_name_wide,
                                        NULL, NULL, 0);
  if (buffer_size > 0) {
    buffer.reset(new uint8[buffer_size]);
    memset(buffer.get(), 0, buffer_size);
    if (DocumentProperties(NULL, printer, device_name_wide,
                           reinterpret_cast<PDEVMODE>(buffer.get()), NULL,
                           DM_OUT_BUFFER) == IDOK) {
      dev_mode = reinterpret_cast<PDEVMODE>(buffer.get());
    }
  }
  if (dev_mode == NULL) {
    buffer.reset();
    ClosePrinter(printer);
    return OnError();
  }

  if (color == GRAY)
    dev_mode->dmColor = DMCOLOR_MONOCHROME;
  else
    dev_mode->dmColor = DMCOLOR_COLOR;

  dev_mode->dmCopies = std::max(copies, 1);
  if (dev_mode->dmCopies > 1)  // do not change collate unless multiple copies
    dev_mode->dmCollate = collate ? DMCOLLATE_TRUE : DMCOLLATE_FALSE;
  switch (duplex_mode) {
    case LONG_EDGE:
      dev_mode->dmDuplex = DMDUP_VERTICAL;
      break;
    case SHORT_EDGE:
      dev_mode->dmDuplex = DMDUP_HORIZONTAL;
      break;
    case SIMPLEX:
      dev_mode->dmDuplex = DMDUP_SIMPLEX;
      break;
    default:  // UNKNOWN_DUPLEX_MODE
      break;
  }
  dev_mode->dmOrientation = landscape ? DMORIENT_LANDSCAPE : DMORIENT_PORTRAIT;

  // Update data using DocumentProperties.
  if (DocumentProperties(NULL, printer, device_name_wide, dev_mode, dev_mode,
                         DM_IN_BUFFER | DM_OUT_BUFFER) != IDOK) {
    ClosePrinter(printer);
    return OnError();
  }

  // Set printer then refresh printer settings.
  if (!AllocateContext(device_name, dev_mode, &context_)) {
    ClosePrinter(printer);
    return OnError();
  }
  PrintSettingsInitializerWin::InitPrintSettings(context_, *dev_mode,
                                                 ranges, device_name,
                                                 false, &settings_);
  ClosePrinter(printer);
  return OK;
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

  // Intentional No-op. NativeMetafile::SafePlayback takes care of calling
  // ::StartPage().

  return OK;
}

PrintingContext::Result PrintingContextWin::PageDone() {
  if (abort_printing_)
    return CANCEL;
  DCHECK(in_print_job_);

  // Intentional No-op. NativeMetafile::SafePlayback takes care of calling
  // ::EndPage().

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
  skia::InitializeDC(context_);
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
  int level = 0;
  DEVMODE* dev_mode = NULL;

  if (GetPrinterInfo(printer, device_name, &level, &buffer, &dev_mode) &&
      AllocateContext(device_name, dev_mode, &context_)) {
    return InitializeSettings(*dev_mode, device_name, NULL, 0, false);
  }

  buffer.reset();
  ResetSettings();
  return false;
}

// static
bool PrintingContextWin::AllocateContext(const std::wstring& device_name,
                                         const DEVMODE* dev_mode,
                                         gfx::NativeDrawingContext* context) {
  *context = CreateDC(L"WINSPOOL", device_name.c_str(), NULL, dev_mode);
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
