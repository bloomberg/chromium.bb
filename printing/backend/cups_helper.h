// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PRINTING_BACKEND_CUPS_HELPER_H_
#define PRINTING_BACKEND_CUPS_HELPER_H_
#pragma once

#include <cups/cups.h>

#include "printing/printing_export.h"

class GURL;

// These are helper functions for dealing with CUPS.
namespace printing {

// Helper wrapper around http_t structure, with connection and cleanup
// functionality.
class PRINTING_EXPORT HttpConnectionCUPS {
 public:
  HttpConnectionCUPS(const GURL& print_server_url,
                     http_encryption_t encryption);
  ~HttpConnectionCUPS();

  void SetBlocking(bool blocking);

  http_t* http();

 private:
  http_t* http_;
};

}  // namespace printing

#endif  // PRINTING_BACKEND_CUPS_HELPER_H_
