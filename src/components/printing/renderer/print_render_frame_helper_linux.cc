// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/printing/renderer/print_render_frame_helper.h"

#include <stddef.h>

#include "base/logging.h"
#include "base/process/process_handle.h"
#include "build/build_config.h"
#include "components/printing/common/print_messages.h"
#include "printing/buildflags/buildflags.h"
#include "printing/metafile_skia.h"

namespace printing {

bool PrintRenderFrameHelper::PrintPagesNative(blink::WebLocalFrame* frame,
                                              int page_count,
                                              bool is_pdf) {
  const PrintMsg_PrintPages_Params& params = *print_pages_params_;
  const PrintMsg_Print_Params& print_params = params.params;

  std::vector<int> printed_pages = GetPrintedPages(params, page_count);
  if (printed_pages.empty())
    return false;

  MetafileSkia metafile(print_params.printed_doc_type,
                        print_params.document_cookie);
  CHECK(metafile.Init());

  for (int page_number : printed_pages) {
    PrintPageInternal(print_params, page_number, page_count,
                      GetScaleFactor(print_params.scale_factor, is_pdf), frame,
                      &metafile, nullptr, nullptr);
  }

  // blink::printEnd() for PDF should be called before metafile is closed.
  FinishFramePrinting();

  metafile.FinishDocument();

  PrintHostMsg_DidPrintDocument_Params page_params;
  if (!CopyMetafileDataToReadOnlySharedMem(metafile, &page_params.content)) {
    return false;
  }

  page_params.document_cookie = print_params.document_cookie;
  Send(new PrintHostMsg_DidPrintDocument(routing_id(), page_params));
  return true;
}

}  // namespace printing
