// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/native_metafile_factory.h"

#include "base/scoped_ptr.h"

#if defined(OS_WIN)
#include "printing/emf_win.h"
#elif defined(OS_MACOSX)
#include "printing/pdf_metafile_mac.h"
#elif defined(OS_POSIX)
#include "printing/pdf_ps_metafile_cairo.h"
#endif

namespace printing {

// static
NativeMetafile* NativeMetafileFactory::Create() {
  scoped_ptr<NativeMetafile> metafile(CreateNewMetafile());
  if (!metafile->Init())
    return NULL;
  return metafile.release();
}

// static
NativeMetafile* NativeMetafileFactory::CreateFromData(
    const void* src_buffer, uint32 src_buffer_size) {
  scoped_ptr<NativeMetafile> metafile(CreateNewMetafile());
  if (!metafile->InitFromData(src_buffer, src_buffer_size))
    return NULL;
  return metafile.release();
}

// static
NativeMetafile* NativeMetafileFactory::CreateNewMetafile(){
#if defined(OS_WIN)
  return new printing::Emf;
#elif defined(OS_MACOSX)
  return new printing::PdfMetafile;
#elif defined(OS_POSIX)
  return new printing::PdfPsMetafile;
#endif
}

}  // namespace printing
