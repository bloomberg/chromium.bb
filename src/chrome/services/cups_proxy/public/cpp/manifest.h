// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_CUPS_PROXY_PUBLIC_CPP_MANIFEST_H_
#define CHROME_SERVICES_CUPS_PROXY_PUBLIC_CPP_MANIFEST_H_

#include "services/service_manager/public/cpp/manifest.h"

namespace chromeos {
namespace printing {

const service_manager::Manifest& GetCupsProxyManifest();

}  // namespace printing
}  // namespace chromeos

#endif  // CHROME_SERVICES_CUPS_PROXY_PUBLIC_CPP_MANIFEST_H_
