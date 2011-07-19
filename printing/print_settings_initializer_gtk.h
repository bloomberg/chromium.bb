// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PRINTING_PRINT_SETTINGS_INITIALIZER_GTK_H_
#define PRINTING_PRINT_SETTINGS_INITIALIZER_GTK_H_

#include "base/logging.h"
#include "printing/page_range.h"

typedef struct _GtkPrintSettings GtkPrintSettings;
typedef struct _GtkPageSetup GtkPageSetup;

namespace printing {

class PrintSettings;

// Initializes a PrintSettings object from the provided Gtk printer objects.
class PrintSettingsInitializerGtk {
 public:
  static void InitPrintSettings(GtkPrintSettings* settings,
                                GtkPageSetup* page_setup,
                                const PageRanges& new_ranges,
                                bool print_selection_only,
                                PrintSettings* print_settings);

  // The default margins, in points. These values are based on 72 dpi,
  // with 0.25 margins on top, left, and right, and 0.56 on bottom.
  static const double kTopMarginInInch;
  static const double kRightMarginInInch;
  static const double kBottomMarginInInch;
  static const double kLeftMarginInInch;

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(PrintSettingsInitializerGtk);
};

}  // namespace printing

#endif  // PRINTING_PRINT_SETTINGS_INITIALIZER_GTK_H_
