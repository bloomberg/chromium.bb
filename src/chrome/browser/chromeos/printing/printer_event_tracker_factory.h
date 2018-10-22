// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_PRINTING_PRINTER_EVENT_TRACKER_FACTORY_H_
#define CHROME_BROWSER_CHROMEOS_PRINTING_PRINTER_EVENT_TRACKER_FACTORY_H_

#include "base/macros.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace chromeos {

class PrinterEventTracker;

class PrinterEventTrackerFactory : public BrowserContextKeyedServiceFactory {
 public:
  static PrinterEventTrackerFactory* GetInstance();
  static PrinterEventTracker* GetForBrowserContext(
      content::BrowserContext* browser_context);

  PrinterEventTrackerFactory();
  ~PrinterEventTrackerFactory() override;

  // Enables/Disables logging for all trackers. Trackers created in the future
  // are created with the current logging state. Logging is enabled if |enabled|
  // is true.
  void SetLogging(bool enabled);

 private:
  bool logging_enabled_ = true;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;

  DISALLOW_COPY_AND_ASSIGN(PrinterEventTrackerFactory);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_PRINTING_PRINTER_EVENT_TRACKER_FACTORY_H_
