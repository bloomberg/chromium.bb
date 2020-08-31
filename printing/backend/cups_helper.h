// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PRINTING_BACKEND_CUPS_HELPER_H_
#define PRINTING_BACKEND_CUPS_HELPER_H_

#include <cups/cups.h>
#include <cups/ppd.h>

#include "base/strings/string_piece.h"
#include "printing/printing_export.h"

class GURL;

// These are helper functions for dealing with CUPS.
namespace printing {

struct PrinterSemanticCapsAndDefaults;

// Helper wrapper around http_t structure, with connection and cleanup
// functionality.
class PRINTING_EXPORT HttpConnectionCUPS {
 public:
  HttpConnectionCUPS(const GURL& print_server_url,
                     http_encryption_t encryption,
                     bool blocking);
  ~HttpConnectionCUPS();

  http_t* http();

 private:
  http_t* http_;
};

// Helper function to parse and convert PPD capabilitites to
// semantic options.
PRINTING_EXPORT bool ParsePpdCapabilities(
    base::StringPiece printer_name,
    base::StringPiece locale,
    base::StringPiece printer_capabilities,
    PrinterSemanticCapsAndDefaults* printer_info);

}  // namespace printing

#endif  // PRINTING_BACKEND_CUPS_HELPER_H_
