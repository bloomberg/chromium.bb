// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_PRINTING_SERVER_PRINTERS_PROVIDER_H_
#define CHROME_BROWSER_CHROMEOS_PRINTING_SERVER_PRINTERS_PROVIDER_H_

#include <map>
#include <memory>
#include <vector>

#include "base/macros.h"
#include "chrome/browser/chromeos/printing/print_server.h"
#include "chrome/browser/chromeos/printing/printer_detector.h"
#include "components/keyed_service/core/keyed_service.h"

namespace chromeos {

// Given a list of external print servers, uses ServerPrintersFetcher to track
// list of printers exposed by them. These printers are called server printers.
// All changes in the final list of available server printers are signaled
// through a callback registered with the method
// RegisterPrintersFoundCallback(...). All methods must be called from the UI
// sequence, the callback is also called from this sequence.
class ServerPrintersProvider {
 public:
  static std::unique_ptr<ServerPrintersProvider> Create();
  virtual ~ServerPrintersProvider() = default;

  using OnPrintersUpdateCallback = base::RepeatingCallback<void(bool complete)>;

  // Register a callback to call. If there is already a callback registered, it
  // will be replaced by the new one. You may also set an empty callback to
  // unregister the current one.
  virtual void RegisterPrintersFoundCallback(OnPrintersUpdateCallback cb) = 0;

  // |servers_are_complete| is true if all policies have been parsed and
  // applied. |servers| contains the current list of print servers to query for
  // printers.
  virtual void OnServersChanged(bool servers_are_complete,
                                const std::map<GURL, PrintServer>& servers) = 0;

  virtual std::vector<PrinterDetector::DetectedPrinter> GetPrinters() = 0;

 protected:
  ServerPrintersProvider() = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(ServerPrintersProvider);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_PRINTING_SERVER_PRINTERS_PROVIDER_H_
