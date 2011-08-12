// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_THUNK_PPB_PDF_API_H_
#define PPAPI_THUNK_PPB_PDF_API_H_

#include "ppapi/c/private/ppb_pdf.h"

namespace ppapi {
namespace thunk {

class PPB_PDFFont_API {
 public:
  virtual ~PPB_PDFFont_API() {}
};

}  // namespace thunk
}  // namespace ppapi

#endif  // PPAPI_THUNK__API_H_
