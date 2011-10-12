// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/printed_document.h"

#include "base/logging.h"

namespace printing {

#if defined(OS_POSIX)
void PrintedDocument::RenderPrintedPage(const PrintedPage& page,
                                        PrintingContext* context) const {
  // TODO(saintlou): This a stub to allow us to build under Aura.
  // See issue: http://crbug.com/99282
  NOTIMPLEMENTED();
}
#endif

}  // namespace printing
