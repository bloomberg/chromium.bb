// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PRINTING_BROWSER_PRINT_MANAGER_H_
#define COMPONENTS_PRINTING_BROWSER_PRINT_MANAGER_H_

#include "base/macros.h"
#include "build/build_config.h"
#include "content/public/browser/web_contents_observer.h"

#if defined(OS_ANDROID)
#include "base/callback.h"
#endif

namespace printing {

class PrintManager : public content::WebContentsObserver {
 public:
  ~PrintManager() override;

#if defined(OS_ANDROID)
  // TODO(timvolodine): consider introducing PrintManagerAndroid (crbug/500960)
  using PdfWritingDoneCallback = base::Callback<void(int /* page count */)>;

  virtual void PdfWritingDone(int page_count) = 0;
#endif

 protected:
  explicit PrintManager(content::WebContents* contents);

  // Terminates or cancels the print job if one was pending.
  void PrintingRenderFrameDeleted();

  // content::WebContentsObserver
  bool OnMessageReceived(const IPC::Message& message,
                         content::RenderFrameHost* render_frame_host) override;

  // IPC handlers
  virtual void OnDidGetPrintedPagesCount(int cookie, int number_pages);
  virtual void OnPrintingFailed(int cookie);

  int number_pages_ = 0;  // Number of pages to print in the print job.
  int cookie_ = 0;        // The current document cookie.

#if defined(OS_ANDROID)
  // Callback to execute when done writing pdf.
  PdfWritingDoneCallback pdf_writing_done_callback_;
#endif

 private:
  void OnDidGetDocumentCookie(int cookie);

  DISALLOW_COPY_AND_ASSIGN(PrintManager);
};

}  // namespace printing

#endif  // COMPONENTS_PRINTING_BROWSER_PRINT_MANAGER_H_
