// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PRINTING_BACKEND_CUPS_HELPER_H_
#define PRINTING_BACKEND_CUPS_HELPER_H_
#pragma once

#include <cups/cups.h>

class GURL;

// These are helper functions for dealing with CUPS.
namespace printing {

// Helper wrapper around http_t structure, with connection and cleanup
// functionality.
class HttpConnectionCUPS {
 public:
  explicit HttpConnectionCUPS(const GURL& print_server_url);
  ~HttpConnectionCUPS();

  http_t* http();

 private:
  http_t* http_;
};

}  // namespace printing

#endif  // PRINTING_BACKEND_CUPS_HELPER_H_
