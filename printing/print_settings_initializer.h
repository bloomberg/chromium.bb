// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PRINTING_PRINT_SETTINGS_INITIALIZER_H_
#define PRINTING_PRINT_SETTINGS_INITIALIZER_H_

#include "base/basictypes.h"
#include "base/logging.h"
#include "printing/printing_export.h"

namespace base {
class DictionaryValue;
}

namespace printing {

class PrintSettings;

// Initializes the header footer strings in the PrintSettings object from the
// provided |job_settings|.
class PRINTING_EXPORT PrintSettingsInitializer {
 public:
  static void InitHeaderFooterStrings(
      const base::DictionaryValue& job_settings,
      PrintSettings* print_settings);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(PrintSettingsInitializer);
};

}  // namespace printing

#endif  // PRINTING_PRINT_SETTINGS_INITIALIZER_H_

