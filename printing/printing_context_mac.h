// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PRINTING_PRINTING_CONTEXT_MAC_H_
#define PRINTING_PRINTING_CONTEXT_MAC_H_

#include <string>

#include "base/scoped_nsobject.h"
#include "printing/printing_context.h"

#ifdef __OBJC__
@class NSPrintInfo;
#else
class NSPrintInfo;
#endif  // __OBJC__

namespace printing {

class PrintingContextMac : public PrintingContext {
 public:
  explicit PrintingContextMac(const std::string& app_locale);
  ~PrintingContextMac();

  // PrintingContext implementation.
  virtual void AskUserForSettings(gfx::NativeView parent_view,
                                  int max_pages,
                                  bool has_selection,
                                  PrintSettingsCallback* callback);
  virtual Result UseDefaultSettings();
  virtual Result UpdatePrintSettings(const PageRanges& ranges);
  virtual Result InitWithSettings(const PrintSettings& settings);
  virtual Result NewDocument(const string16& document_name);
  virtual Result NewPage();
  virtual Result PageDone();
  virtual Result DocumentDone();
  virtual void Cancel();
  virtual void ReleaseContext();
  virtual gfx::NativeDrawingContext context() const;

 private:
  // Read the settings from the given NSPrintInfo (and cache it for later use).
  void ParsePrintInfo(NSPrintInfo* print_info);

  // The native print info object.
  scoped_nsobject<NSPrintInfo> print_info_;

  // The current page's context; only valid between NewPage and PageDone call
  // pairs.
  CGContext* context_;

  DISALLOW_COPY_AND_ASSIGN(PrintingContextMac);
};

}  // namespace printing

#endif  // PRINTING_PRINTING_CONTEXT_MAC_H_
