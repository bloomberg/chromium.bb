// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PDF_CHROME_PDF_WEB_CONTENTS_HELPER_CLIENT_H_
#define CHROME_BROWSER_UI_PDF_CHROME_PDF_WEB_CONTENTS_HELPER_CLIENT_H_

#include "base/macros.h"
#include "components/pdf/browser/pdf_web_contents_helper_client.h"

class ChromePDFWebContentsHelperClient
    : public pdf::PDFWebContentsHelperClient {
 public:
  ChromePDFWebContentsHelperClient();
  ~ChromePDFWebContentsHelperClient() override;

 private:
  // pdf::PDFWebContentsHelperClient:
  void UpdateContentRestrictions(content::WebContents* contents,
                                 int content_restrictions) override;
  void OnPDFHasUnsupportedFeature(content::WebContents* contents) override;
  void OnSaveURL(content::WebContents* contents) override;
  void SetPluginCanSave(content::WebContents* contents, bool can_save) override;

  DISALLOW_COPY_AND_ASSIGN(ChromePDFWebContentsHelperClient);
};

#endif  // CHROME_BROWSER_UI_PDF_CHROME_PDF_WEB_CONTENTS_HELPER_CLIENT_H_
