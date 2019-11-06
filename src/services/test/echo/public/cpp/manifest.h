// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_TEST_ECHO_PUBLIC_CPP_MANIFEST_H_
#define SERVICES_TEST_ECHO_PUBLIC_CPP_MANIFEST_H_

#include "services/service_manager/public/cpp/manifest.h"

namespace echo {

service_manager::Manifest GetManifest(
    service_manager::Manifest::ExecutionMode execution_mode);

}  // namespace echo

#endif  // SERVICES_TEST_ECHO_PUBLIC_CPP_MANIFEST_H_
