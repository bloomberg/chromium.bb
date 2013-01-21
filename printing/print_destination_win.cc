// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/print_destination_interface.h"

#include "base/safe_numerics.h"
#include "base/win/metro.h"
#include "win8/util/win8_util.h"

namespace printing {

class PrintDestinationWin : public PrintDestinationInterface {
 public:
  PrintDestinationWin()
      : metro_set_print_page_count_(NULL),
        metro_set_print_page_content_(NULL) {
    HMODULE metro_module = base::win::GetMetroModule();
    if (metro_module != NULL) {
      metro_set_print_page_count_ =
          reinterpret_cast<MetroSetPrintPageCount>(
              ::GetProcAddress(metro_module, "MetroSetPrintPageCount"));
      metro_set_print_page_content_ =
          reinterpret_cast<MetroSetPrintPageContent>(
              ::GetProcAddress(metro_module, "MetroSetPrintPageContent"));
    }
  }
  virtual void SetPageCount(int page_count) {
    if (metro_set_print_page_count_)
      metro_set_print_page_count_(page_count);
  }

  virtual void SetPageContent(int page_number,
                              void* content,
                              size_t content_size) {
    if (metro_set_print_page_content_)
      metro_set_print_page_content_(page_number - 1, content,
          base::checked_numeric_cast<UINT32>(content_size));
  }
 private:
  typedef void (*MetroSetPrintPageCount)(INT);
  typedef void (*MetroSetPrintPageContent)(INT, VOID*, UINT32);
  MetroSetPrintPageCount metro_set_print_page_count_;
  MetroSetPrintPageContent metro_set_print_page_content_;
};

PrintDestinationInterface* CreatePrintDestination() {
  // We currently only support the Metro print destination.
  if (win8::IsSingleWindowMetroMode())
    return new PrintDestinationWin;
  else
    return NULL;
}

}  // namespace printing
