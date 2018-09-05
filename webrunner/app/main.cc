// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fuchsia/ui/viewsv1/cpp/fidl.h>
#include <lib/fidl/cpp/binding.h>
#include <utility>

#include "base/files/file.h"
#include "base/fuchsia/component_context.h"
#include "base/fuchsia/file_utils.h"
#include "base/fuchsia/scoped_service_binding.h"
#include "base/fuchsia/service_directory.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "webrunner/app/web_content_runner.h"
#include "webrunner/fidl/chromium/web/cpp/fidl.h"

namespace {

chromium::web::ContextPtr CreateContext() {
  auto web_context_provider =
      base::fuchsia::ComponentContext::GetDefault()
          ->ConnectToService<chromium::web::ContextProvider>();

  chromium::web::CreateContextParams create_params;

  // Clone /svc to the context.
  create_params.service_directory =
      zx::channel(base::fuchsia::GetHandleFromFile(
          base::File(base::FilePath("/svc"),
                     base::File::FLAG_OPEN | base::File::FLAG_READ)));

  chromium::web::ContextPtr web_context;
  web_context_provider->Create(std::move(create_params),
                               web_context.NewRequest());
  web_context.set_error_handler([]() {
    // If the browser instance died, then exit everything and do not attempt
    // to recover. appmgr will relaunch the runner when it is needed again.
    LOG(ERROR) << "Connection to Context lost.";
    exit(1);
  });
  return web_context;
}

}  // namespace

int main(int argc, char** argv) {
  base::MessageLoopForIO message_loop;

  auto web_context = CreateContext();

  // Run until the WebContentRunner is ready to exit.
  base::RunLoop run_loop;

  webrunner::WebContentRunner runner(std::move(web_context),
                                     run_loop.QuitClosure());

  base::fuchsia::ServiceDirectory* directory =
      base::fuchsia::ServiceDirectory::GetDefault();

  // Bind the Runner FIDL instance.
  base::fuchsia::ScopedServiceBinding<fuchsia::sys::Runner> runner_binding(
      directory, &runner);

  runner_binding.SetOnLastClientCallback(run_loop.QuitClosure());

  // The RunLoop runs until all Components have been closed, at which point the
  // application will terminate.
  run_loop.Run();

  return 0;
}
