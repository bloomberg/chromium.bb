// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBRUNNER_APP_CAST_CAST_RUNNER_H_
#define WEBRUNNER_APP_CAST_CAST_RUNNER_H_

#include "base/callback.h"
#include "base/macros.h"
#include "webrunner/app/common/web_content_runner.h"
#include "webrunner/fidl/chromium/web/cpp/fidl.h"

namespace castrunner {

// sys::Runner which instantiates Cast activities specified via cast/casts URIs.
class CastRunner : public webrunner::WebContentRunner {
 public:
  CastRunner(base::fuchsia::ServiceDirectory* service_directory,
             chromium::web::ContextPtr context,
             base::OnceClosure on_idle_closure);
  ~CastRunner() override;

  // fuchsia::sys::Runner implementation.
  void StartComponent(fuchsia::sys::Package package,
                      fuchsia::sys::StartupInfo startup_info,
                      fidl::InterfaceRequest<fuchsia::sys::ComponentController>
                          controller_request) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(CastRunner);
};

}  // namespace castrunner

#endif  // WEBRUNNER_APP_CAST_CAST_RUNNER_H_
