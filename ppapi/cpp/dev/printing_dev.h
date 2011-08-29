// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_CPP_DEV_PRINTING_DEV_H_
#define PPAPI_CPP_DEV_PRINTING_DEV_H_

#include "ppapi/c/dev/ppp_printing_dev.h"
#include "ppapi/cpp/resource.h"

namespace pp {

class Instance;

// You would typically use this either via inheritance on your instance or
// by composition: see find_dev.h for an example.
class Printing_Dev {
 public:
  // The instance parameter must outlive this class.
  explicit Printing_Dev(Instance* instance);
  virtual ~Printing_Dev();

  // PPP_Printing_Dev functions exposed as virtual functions for you to
  // override.
  virtual uint32_t QuerySupportedPrintOutputFormats() = 0;
  virtual int32_t PrintBegin(const PP_PrintSettings_Dev& print_settings) = 0;
  virtual Resource PrintPages(const PP_PrintPageNumberRange_Dev* page_ranges,
                              uint32_t page_range_count) = 0;
  virtual void PrintEnd() = 0;

 private:
  Instance* associated_instance_;
};

}  // namespace pp

#endif  // PPAPI_CPP_DEV_PRINTING_DEV_H_
