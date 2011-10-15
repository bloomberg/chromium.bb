// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/url_request/fraudulent_certificate_reporter.h"

namespace net {

// This is necessary to build the net.dll shared library on Windows.
FraudulentCertificateReporter::~FraudulentCertificateReporter() {
}

}  // namespace net

