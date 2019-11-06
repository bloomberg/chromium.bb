// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_PRINTING_CUPS_PROXY_SERVICE_MANAGER_H_
#define CHROME_BROWSER_CHROMEOS_PRINTING_CUPS_PROXY_SERVICE_MANAGER_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/services/cups_proxy/public/mojom/proxy.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace chromeos {

// Responsible for lazily starting the CupsProxyService once notified that the
// CupsProxyDaemon has started.
//
// Note: This manager is not currently fault-tolerant, i.e. should the
// service/daemon fail, we do not try to restart.
class CupsProxyServiceManager {
 public:
  CupsProxyServiceManager();
  ~CupsProxyServiceManager();

 private:
  void OnDaemonAvailable(bool daemon_available);

  mojo::Remote<printing::mojom::StartCupsProxyService> service_handle_;
  base::WeakPtrFactory<CupsProxyServiceManager> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(CupsProxyServiceManager);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_PRINTING_CUPS_PROXY_SERVICE_MANAGER_H_
