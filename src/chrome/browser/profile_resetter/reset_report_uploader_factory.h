// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILE_RESETTER_RESET_REPORT_UPLOADER_FACTORY_H_
#define CHROME_BROWSER_PROFILE_RESETTER_RESET_REPORT_UPLOADER_FACTORY_H_

#include "base/macros.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace base {
template <typename T>
struct DefaultSingletonTraits;
}

namespace content {
class BrowserContext;
}

class ResetReportUploader;

class ResetReportUploaderFactory : public BrowserContextKeyedServiceFactory {
 public:
  // Returns singleton instance of ResetReportUploaderFactory.
  static ResetReportUploaderFactory* GetInstance();

  // Returns the ResetReportUploader associated with |context|.
  static ResetReportUploader* GetForBrowserContext(
      content::BrowserContext* context);

 private:
  friend struct base::DefaultSingletonTraits<ResetReportUploaderFactory>;

  ResetReportUploaderFactory();
  ~ResetReportUploaderFactory() override;

  // BrowserContextKeyedServiceFactory overrides:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;

  DISALLOW_COPY_AND_ASSIGN(ResetReportUploaderFactory);
};

#endif  // CHROME_BROWSER_PROFILE_RESETTER_RESET_REPORT_UPLOADER_FACTORY_H_
