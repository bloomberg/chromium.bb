// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/native_metafile_factory.h"

#if defined(OS_WIN)
#include "printing/emf_win.h"
#elif defined(OS_MACOSX)
#include "printing/pdf_metafile_mac.h"
#elif defined(OS_POSIX)
#include "printing/pdf_ps_metafile_cairo.h"
#endif

namespace printing {

// static
printing::NativeMetafile* NativeMetafileFactory::CreateMetafile() {
#if defined(OS_WIN)
  return new printing::Emf;
#elif defined(OS_MACOSX)
  return new printing::PdfMetafile;
#elif defined(OS_POSIX)
  return new printing::PdfPsMetafile;
#endif
}

}  // namespace printing
