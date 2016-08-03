// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/shell/public/c/main.h"
#include "services/shell/public/cpp/service_runner.h"
#include "services/tracing/service.h"

MojoResult ServiceMain(MojoHandle service_request_handle) {
  return shell::ServiceRunner(new tracing::Service).Run(service_request_handle);
}
