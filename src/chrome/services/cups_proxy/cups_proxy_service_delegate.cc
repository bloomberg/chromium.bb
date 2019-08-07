// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/cups_proxy/cups_proxy_service_delegate.h"

#include <memory>
#include <string>

#include "base/memory/weak_ptr.h"

namespace chromeos {
namespace printing {

CupsProxyServiceDelegate::CupsProxyServiceDelegate() : weak_factory_(this) {}
CupsProxyServiceDelegate::~CupsProxyServiceDelegate() = default;

base::WeakPtr<CupsProxyServiceDelegate> CupsProxyServiceDelegate::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

}  // namespace printing
}  // namespace chromeos
