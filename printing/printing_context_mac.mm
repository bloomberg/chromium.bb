// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/printing_context_mac.h"

#import <ApplicationServices/ApplicationServices.h>
#import <AppKit/AppKit.h>

#include "base/logging.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/sys_string_conversions.h"
#include "base/values.h"
#include "printing/print_job_constants.h"
#include "printing/print_settings_initializer_mac.h"

namespace printing {

// static
PrintingContext* PrintingContext::Create(const std::string& app_locale) {
  return static_cast<PrintingContext*>(new PrintingContextMac(app_locale));
}

PrintingContextMac::PrintingContextMac(const std::string& app_locale)
    : PrintingContext(app_locale),
      context_(NULL) {
}

PrintingContextMac::~PrintingContextMac() {
  ReleaseContext();
}

void PrintingContextMac::AskUserForSettings(gfx::NativeView parent_view,
                                            int max_pages,
                                            bool has_selection,
                                            PrintSettingsCallback* callback) {
  DCHECK([NSThread isMainThread]);

  // We deliberately don't feed max_pages into the dialog, because setting
  // NSPrintLastPage makes the print dialog pre-select the option to only print
  // a range.

  // TODO(stuartmorgan): implement 'print selection only' (probably requires
  // adding a new custom view to the panel on 10.5; 10.6 has
  // NSPrintPanelShowsPrintSelection).
  NSPrintPanel* panel = [NSPrintPanel printPanel];
  NSPrintInfo* printInfo = [NSPrintInfo sharedPrintInfo];

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
    ParsePrintInfo([panel printInfo]);
    callback->Run(OK);
  } else {
    callback->Run(CANCEL);
  }
}

PrintingContext::Result PrintingContextMac::UseDefaultSettings() {
  DCHECK(!in_print_job_);

  ParsePrintInfo([NSPrintInfo sharedPrintInfo]);

  return OK;
}

PrintingContext::Result PrintingContextMac::UpdatePrintSettings(
    const DictionaryValue& job_settings, const PageRanges& ranges) {
  DCHECK(!in_print_job_);

  ResetSettings();
  print_info_.reset([[NSPrintInfo sharedPrintInfo] copy]);

  bool landscape;
  std::string printer_name;
  int copies;
  bool collate;
  bool two_sided;
  if (!job_settings.GetBoolean(kSettingLandscape, &landscape) ||
      !job_settings.GetString(kSettingPrinterName, &printer_name) ||
      !job_settings.GetInteger(kSettingCopies, &copies) ||
      !job_settings.GetBoolean(kSettingCollate, &collate) ||
      !job_settings.GetBoolean(kSettingTwoSided, &two_sided)) {
    return OnError();
  }

  if (!SetPrinter(printer_name))
    return OnError();

  if (!SetCopiesInPrintSettings(copies))
    return OnError();

  if (!SetCollateInPrintSettings(collate))
    return OnError();

  if (!SetOrientationIsLandscape(landscape))
    return OnError();

  if (!SetDuplexModeIsTwoSided(two_sided))
    return OnError();

  [print_info_.get() updateFromPMPrintSettings];

  InitPrintSettingsFromPrintInfo(ranges);
  return OK;
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

bool PrintingContextMac::SetPrinter(const std::string& printer_name) {
  NSString* new_printer_name = base::SysUTF8ToNSString(printer_name);
  if (!new_printer_name)
    return false;

  if (![[[print_info_.get() printer] name] isEqualToString:new_printer_name]) {
    NSPrinter* new_printer = [NSPrinter printerWithName:new_printer_name];
    if (new_printer == nil)
      return false;

    [print_info_.get() setPrinter:new_printer];
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

  [print_info_.get() updateFromPMPageFormat];
  return true;
}

bool PrintingContextMac::SetDuplexModeIsTwoSided(bool two_sided) {
  PMDuplexMode duplexSetting = two_sided ? kPMDuplexNoTumble : kPMDuplexNone;
  PMPrintSettings pmPrintSettings =
      static_cast<PMPrintSettings>([print_info_.get() PMPrintSettings]);
  return PMSetDuplex(pmPrintSettings, duplexSetting) == noErr;
}

void PrintingContextMac::ParsePrintInfo(NSPrintInfo* print_info) {
  ResetSettings();
  print_info_.reset([print_info retain]);
  PageRanges page_ranges;
  NSDictionary* print_info_dict = [print_info_.get() dictionary];
  if (![[print_info_dict objectForKey:NSPrintAllPages] boolValue]) {
    PageRange range;
    range.from = [[print_info_dict objectForKey:NSPrintFirstPage] intValue] - 1;
    range.to = [[print_info_dict objectForKey:NSPrintLastPage] intValue] - 1;
    page_ranges.push_back(range);
  }
  InitPrintSettingsFromPrintInfo(page_ranges);
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
