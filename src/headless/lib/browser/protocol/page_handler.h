// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HEADLESS_LIB_BROWSER_PROTOCOL_PAGE_HANDLER_H_
#define HEADLESS_LIB_BROWSER_PROTOCOL_PAGE_HANDLER_H_

#include "headless/lib/browser/protocol/domain_handler.h"
#include "headless/lib/browser/protocol/dp_page.h"
#include "printing/buildflags/buildflags.h"

#if BUILDFLAG(ENABLE_PRINTING)
#include "headless/lib/browser/headless_print_manager.h"
#include "headless/public/headless_export.h"
#endif

namespace content {
class WebContents;
}

namespace headless {
namespace protocol {

class PageHandler : public DomainHandler, public Page::Backend {
 public:
  PageHandler(base::WeakPtr<HeadlessBrowserImpl> browser,
              content::WebContents* web_contents);
  ~PageHandler() override;

  void Wire(UberDispatcher* dispatcher) override;

  // Page::Backend implementation
  void PrintToPDF(Maybe<bool> landscape,
                  Maybe<bool> display_header_footer,
                  Maybe<bool> print_background,
                  Maybe<double> scale,
                  Maybe<double> paper_width,
                  Maybe<double> paper_height,
                  Maybe<double> margin_top,
                  Maybe<double> margin_bottom,
                  Maybe<double> margin_left,
                  Maybe<double> margin_right,
                  Maybe<String> page_ranges,
                  Maybe<bool> ignore_invalid_page_ranges,
                  Maybe<String> header_template,
                  Maybe<String> footer_template,
                  Maybe<bool> prefer_css_page_size,
                  std::unique_ptr<PrintToPDFCallback> callback) override;

 private:
  content::WebContents* web_contents_;
  DISALLOW_COPY_AND_ASSIGN(PageHandler);
};

}  // namespace protocol
}  // namespace headless

#endif  // HEADLESS_LIB_BROWSER_PROTOCOL_PAGE_HANDLER_H_
