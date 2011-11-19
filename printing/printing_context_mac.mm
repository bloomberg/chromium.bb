// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/printing_context_mac.h"

#import <ApplicationServices/ApplicationServices.h>
#import <AppKit/AppKit.h>

#include "base/logging.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/mac/scoped_nsautorelease_pool.h"
#include "base/mac/scoped_nsexception_enabler.h"
#include "base/sys_string_conversions.h"
#include "base/values.h"
#include "printing/print_settings_initializer_mac.h"

namespace printing {

// static
PrintingContext* PrintingContext::Create(const std::string& app_locale) {
  return static_cast<PrintingContext*>(new PrintingContextMac(app_locale));
}

PrintingContextMac::PrintingContextMac(const std::string& app_locale)
    : PrintingContext(app_locale),
      print_info_([[NSPrintInfo sharedPrintInfo] copy]),
      context_(NULL) {
}

PrintingContextMac::~PrintingContextMac() {
  ReleaseContext();
}

void PrintingContextMac::AskUserForSettings(
    gfx::NativeView parent_view,
    int max_pages,
    bool has_selection,
    const PrintSettingsCallback& callback) {
  // Third-party print drivers seem to be an area prone to raising exceptions.
  // This will allow exceptions to be raised, but does not handle them.  The
  // NSPrintPanel appears to have appropriate NSException handlers.
  base::mac::ScopedNSExceptionEnabler enabler;

  // Exceptions can also happen when the NSPrintPanel is being
  // deallocated, so it must be autoreleased within this scope.
  base::mac::ScopedNSAutoreleasePool pool;

  DCHECK([NSThread isMainThread]);

  // We deliberately don't feed max_pages into the dialog, because setting
  // NSPrintLastPage makes the print dialog pre-select the option to only print
  // a range.

  // TODO(stuartmorgan): implement 'print selection only' (probably requires
  // adding a new custom view to the panel on 10.5; 10.6 has
  // NSPrintPanelShowsPrintSelection).
  NSPrintPanel* panel = [NSPrintPanel printPanel];
  NSPrintInfo* printInfo = print_info_.get();

  NSPrintPanelOptions options = [panel options];
  options |= NSPrintPanelShowsPaperSize;
  options |= NSPrintPanelShowsOrientation;
  options |= NSPrintPanelShowsScaling;
  [panel setOptions:options];

  // Set the print job title text.
  if (parent_view) {
    NSString* job_title = [[parent_view window] title];
    if (job_title) {
      PMPrintSettings printSettings =
          (PMPrintSettings)[printInfo PMPrintSettings];
      PMPrintSettingsSetJobName(printSettings, (CFStringRef)job_title);
      [printInfo updateFromPMPrintSettings];
    }
  }

  // TODO(stuartmorgan): We really want a tab sheet here, not a modal window.
  // Will require restructuring the PrintingContext API to use a callback.
  NSInteger selection = [panel runModalWithPrintInfo:printInfo];
  if (selection == NSOKButton) {
    print_info_.reset([[panel printInfo] retain]);
    InitPrintSettingsFromPrintInfo(GetPageRangesFromPrintInfo());
    callback.Run(OK);
  } else {
    callback.Run(CANCEL);
  }
}

PrintingContext::Result PrintingContextMac::UseDefaultSettings() {
  DCHECK(!in_print_job_);

  print_info_.reset([[NSPrintInfo sharedPrintInfo] copy]);
  InitPrintSettingsFromPrintInfo(GetPageRangesFromPrintInfo());

  return OK;
}

PrintingContext::Result PrintingContextMac::UpdatePrinterSettings(
    const DictionaryValue& job_settings, const PageRanges& ranges) {
  DCHECK(!in_print_job_);

  // NOTE: Reset |print_info_| with a copy of |sharedPrintInfo| so as to start
  // with a clean slate.
  print_info_.reset([[NSPrintInfo sharedPrintInfo] copy]);

  bool collate;
  int color;
  bool landscape;
  bool print_to_pdf;
  bool is_cloud_dialog;
  int copies;
  int duplex_mode;
  std::string device_name;

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
  bool open_pdf_in_preview = job_settings.HasKey(kSettingOpenPDFInPreview);

  if (!print_to_pdf && !print_to_cloud && !is_cloud_dialog) {
    if (!SetPrinter(device_name))
      return OnError();

    if (!SetCopiesInPrintSettings(copies))
      return OnError();

    if (!SetCollateInPrintSettings(collate))
      return OnError();

    if (!SetDuplexModeInPrintSettings(
            static_cast<DuplexMode>(duplex_mode))) {
      return OnError();
    }

    if (!SetOutputColor(color))
      return OnError();
  }
  if (open_pdf_in_preview) {
    if (!SetPrintPreviewJob())
      return OnError();
  }

  if (!UpdatePageFormatWithPaperInfo())
    return OnError();

  if (!SetOrientationIsLandscape(landscape))
    return OnError();

  [print_info_.get() updateFromPMPrintSettings];

  InitPrintSettingsFromPrintInfo(ranges);
  return OK;
}

bool PrintingContextMac::SetPrintPreviewJob() {
  PMPrintSession print_session =
      static_cast<PMPrintSession>([print_info_.get() PMPrintSession]);
  PMPrintSettings print_settings =
      static_cast<PMPrintSettings>([print_info_.get() PMPrintSettings]);
  return PMSessionSetDestination(
      print_session, print_settings, kPMDestinationPreview,
      NULL, NULL) == noErr;
}

void PrintingContextMac::InitPrintSettingsFromPrintInfo(
    const PageRanges& ranges) {
  PMPrintSession print_session =
      static_cast<PMPrintSession>([print_info_.get() PMPrintSession]);
  PMPageFormat page_format =
      static_cast<PMPageFormat>([print_info_.get() PMPageFormat]);
  PMPrinter printer;
  PMSessionGetCurrentPrinter(print_session, &printer);
  PrintSettingsInitializerMac::InitPrintSettings(
      printer, page_format, ranges, false, &settings_);
}

bool PrintingContextMac::SetPrinter(const std::string& device_name) {
  DCHECK(print_info_.get());
  PMPrintSession print_session =
      static_cast<PMPrintSession>([print_info_.get() PMPrintSession]);

  PMPrinter current_printer;
  if (PMSessionGetCurrentPrinter(print_session, &current_printer) != noErr)
    return false;

  CFStringRef current_printer_id = PMPrinterGetID(current_printer);
  if (!current_printer_id)
    return false;

  base::mac::ScopedCFTypeRef<CFStringRef> new_printer_id(
      base::SysUTF8ToCFStringRef(device_name));
  if (!new_printer_id.get())
    return false;

  if (CFStringCompare(new_printer_id.get(), current_printer_id, 0) ==
          kCFCompareEqualTo) {
    return true;
  }

  PMPrinter new_printer = PMPrinterCreateFromPrinterID(new_printer_id.get());
  if (new_printer == NULL)
    return false;

  OSStatus status = PMSessionSetCurrentPMPrinter(print_session, new_printer);
  PMRelease(new_printer);
  return status == noErr;
}

bool PrintingContextMac::UpdatePageFormatWithPaperInfo() {
  PMPrintSession print_session =
      static_cast<PMPrintSession>([print_info_.get() PMPrintSession]);

  PMPageFormat default_page_format =
      static_cast<PMPageFormat>([print_info_.get() PMPageFormat]);

  PMPaper default_paper;
  if (PMGetPageFormatPaper(default_page_format, &default_paper) != noErr)
    return false;

  double default_page_width, default_page_height;
  if (PMPaperGetWidth(default_paper, &default_page_width) != noErr)
    return false;

  if (PMPaperGetHeight(default_paper, &default_page_height) != noErr)
    return false;

  PMPrinter current_printer = NULL;
  if (PMSessionGetCurrentPrinter(print_session, &current_printer) != noErr)
    return false;

  if (current_printer == nil)
    return false;

  CFArrayRef paper_list = NULL;
  if (PMPrinterGetPaperList(current_printer, &paper_list) != noErr)
    return false;

  PMPaper best_matching_paper = kPMNoData;
  int num_papers = CFArrayGetCount(paper_list);
  for (int i = 0; i < num_papers; ++i) {
    PMPaper paper = (PMPaper) [(NSArray* ) paper_list objectAtIndex: i];
    double paper_width, paper_height;
    PMPaperGetWidth(paper, &paper_width);
    PMPaperGetHeight(paper, &paper_height);
    if (default_page_width == paper_width &&
        default_page_height == paper_height) {
      best_matching_paper = paper;
      break;
    }
    // Trying to find the best matching paper.
    if (fabs(default_page_width - paper_width) < 2 &&
        fabs(default_page_height - paper_height) < 2) {
      best_matching_paper = paper;
    }
  }

  if (best_matching_paper == kPMNoData) {
    PMPaper paper = kPMNoData;
    // Create a custom paper for the specified default page size.
    PMPaperMargins default_margins;
    if (PMPaperGetMargins(default_paper, &default_margins) != noErr)
      return false;

    const PMPaperMargins margins =
        {default_margins.top, default_margins.left, default_margins.bottom,
         default_margins.right};
    CFStringRef paper_id = CFSTR("Custom paper ID");
    CFStringRef paper_name = CFSTR("Custom paper");
    if (PMPaperCreateCustom(current_printer, paper_id, paper_name,
            default_page_width, default_page_height, &margins, &paper) !=
            noErr) {
      return false;
    }
    [print_info_.get() updateFromPMPageFormat];
    PMRelease(paper);
  } else {
    PMPageFormat chosen_page_format = NULL;
    if (PMCreatePageFormat((PMPageFormat*) &chosen_page_format) != noErr)
      return false;

    // Create page format from that paper.
    if (PMCreatePageFormatWithPMPaper(&chosen_page_format,
            best_matching_paper) != noErr) {
      PMRelease(chosen_page_format);
      return false;
    }
    // Copy over the original format with the new page format.
    if (PMCopyPageFormat(chosen_page_format, default_page_format) != noErr) {
      PMRelease(chosen_page_format);
      return false;
    }
    [print_info_.get() updateFromPMPageFormat];
    PMRelease(chosen_page_format);
  }
  return true;
}

bool PrintingContextMac::SetCopiesInPrintSettings(int copies) {
  if (copies < 1)
    return false;

  PMPrintSettings pmPrintSettings =
      static_cast<PMPrintSettings>([print_info_.get() PMPrintSettings]);
  return PMSetCopies(pmPrintSettings, copies, false) == noErr;
}

bool PrintingContextMac::SetCollateInPrintSettings(bool collate) {
  PMPrintSettings pmPrintSettings =
      static_cast<PMPrintSettings>([print_info_.get() PMPrintSettings]);
  return PMSetCollate(pmPrintSettings, collate) == noErr;
}

bool PrintingContextMac::SetOrientationIsLandscape(bool landscape) {
  PMPageFormat page_format =
      static_cast<PMPageFormat>([print_info_.get() PMPageFormat]);

  PMOrientation orientation = landscape ? kPMLandscape : kPMPortrait;

  if (PMSetOrientation(page_format, orientation, false) != noErr)
    return false;

  PMPrintSession print_session =
      static_cast<PMPrintSession>([print_info_.get() PMPrintSession]);

  PMSessionValidatePageFormat(print_session, page_format, kPMDontWantBoolean);

  [print_info_.get() updateFromPMPageFormat];
  return true;
}

bool PrintingContextMac::SetDuplexModeInPrintSettings(DuplexMode mode) {
  PMDuplexMode duplexSetting;
  switch (mode) {
    case LONG_EDGE:
      duplexSetting = kPMDuplexNoTumble;
      break;
    case SHORT_EDGE:
      duplexSetting = kPMDuplexTumble;
      break;
    case SIMPLEX:
      duplexSetting = kPMDuplexNone;
      break;
    default:  // UNKNOWN_DUPLEX_MODE
      return true;
  }

  PMPrintSettings pmPrintSettings =
      static_cast<PMPrintSettings>([print_info_.get() PMPrintSettings]);
  return PMSetDuplex(pmPrintSettings, duplexSetting) == noErr;
}

bool PrintingContextMac::SetOutputColor(int color_mode) {
  PMPrintSettings pmPrintSettings =
      static_cast<PMPrintSettings>([print_info_.get() PMPrintSettings]);
  std::string color_setting_name;
  std::string color_value;
  GetColorModelForMode(color_mode, &color_setting_name, &color_value);
  base::mac::ScopedCFTypeRef<CFStringRef> color_setting(
      base::SysUTF8ToCFStringRef(color_setting_name));
  base::mac::ScopedCFTypeRef<CFStringRef> output_color(
      base::SysUTF8ToCFStringRef(color_value));

  return PMPrintSettingsSetValue(pmPrintSettings,
                                 color_setting.get(),
                                 output_color.get(),
                                 false) == noErr;
}

PageRanges PrintingContextMac::GetPageRangesFromPrintInfo() {
  PageRanges page_ranges;
  NSDictionary* print_info_dict = [print_info_.get() dictionary];
  if (![[print_info_dict objectForKey:NSPrintAllPages] boolValue]) {
    PageRange range;
    range.from = [[print_info_dict objectForKey:NSPrintFirstPage] intValue] - 1;
    range.to = [[print_info_dict objectForKey:NSPrintLastPage] intValue] - 1;
    page_ranges.push_back(range);
  }
  return page_ranges;
}

PrintingContext::Result PrintingContextMac::InitWithSettings(
    const PrintSettings& settings) {
  DCHECK(!in_print_job_);

  settings_ = settings;

  NOTIMPLEMENTED();

  return FAILED;
}

PrintingContext::Result PrintingContextMac::NewDocument(
    const string16& document_name) {
  DCHECK(!in_print_job_);

  in_print_job_ = true;

  PMPrintSession print_session =
      static_cast<PMPrintSession>([print_info_.get() PMPrintSession]);
  PMPrintSettings print_settings =
      static_cast<PMPrintSettings>([print_info_.get() PMPrintSettings]);
  PMPageFormat page_format =
      static_cast<PMPageFormat>([print_info_.get() PMPageFormat]);

  base::mac::ScopedCFTypeRef<CFStringRef> job_title(
      base::SysUTF16ToCFStringRef(document_name));
  PMPrintSettingsSetJobName(print_settings, job_title.get());

  OSStatus status = PMSessionBeginCGDocumentNoDialog(print_session,
                                                     print_settings,
                                                     page_format);
  if (status != noErr)
    return OnError();

  return OK;
}

PrintingContext::Result PrintingContextMac::NewPage() {
  if (abort_printing_)
    return CANCEL;
  DCHECK(in_print_job_);
  DCHECK(!context_);

  PMPrintSession print_session =
      static_cast<PMPrintSession>([print_info_.get() PMPrintSession]);
  PMPageFormat page_format =
      static_cast<PMPageFormat>([print_info_.get() PMPageFormat]);
  OSStatus status;
  status = PMSessionBeginPageNoDialog(print_session, page_format, NULL);
  if (status != noErr)
    return OnError();
  status = PMSessionGetCGGraphicsContext(print_session, &context_);
  if (status != noErr)
    return OnError();

  return OK;
}

PrintingContext::Result PrintingContextMac::PageDone() {
  if (abort_printing_)
    return CANCEL;
  DCHECK(in_print_job_);
  DCHECK(context_);

  PMPrintSession print_session =
      static_cast<PMPrintSession>([print_info_.get() PMPrintSession]);
  OSStatus status = PMSessionEndPageNoDialog(print_session);
  if (status != noErr)
    OnError();
  context_ = NULL;

  return OK;
}

PrintingContext::Result PrintingContextMac::DocumentDone() {
  if (abort_printing_)
    return CANCEL;
  DCHECK(in_print_job_);

  PMPrintSession print_session =
      static_cast<PMPrintSession>([print_info_.get() PMPrintSession]);
  OSStatus status = PMSessionEndDocumentNoDialog(print_session);
  if (status != noErr)
    OnError();

  ResetSettings();
  return OK;
}

void PrintingContextMac::Cancel() {
  abort_printing_ = true;
  in_print_job_ = false;
  context_ = NULL;

  PMPrintSession print_session =
      static_cast<PMPrintSession>([print_info_.get() PMPrintSession]);
  PMSessionEndPageNoDialog(print_session);
}

void PrintingContextMac::ReleaseContext() {
  print_info_.reset();
  context_ = NULL;
}

gfx::NativeDrawingContext PrintingContextMac::context() const {
  return context_;
}

}  // namespace printing
