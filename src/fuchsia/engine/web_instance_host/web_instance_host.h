// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_ENGINE_WEB_INSTANCE_HOST_WEB_INSTANCE_HOST_H_
#define FUCHSIA_ENGINE_WEB_INSTANCE_HOST_WEB_INSTANCE_HOST_H_

#include <fuchsia/io/cpp/fidl.h>
#include <fuchsia/sys/cpp/fidl.h>
#include <fuchsia/web/cpp/fidl.h>
#include <lib/fidl/cpp/interface_request.h>

#include "base/values.h"

namespace cr_fuchsia {

// Helper class that allows web_instance Components to be launched based on
// caller-supplied |CreateContextParams|.
//
// Note that Components using this class must:
// 1. Include the "web_instance.cmx" in their package, for the implementation
//    to read the sandbox services from.
// 2. List the fuchsia.sys.Environment & .Loader services in their sandbox.
//
// To ensure proper product data registration, Components using the class must:
// * Have the same version and channel as WebEngine.
// * Include the following services in their manifest:
//   * "fuchsia.feedback.ComponentDataRegister"
//   * "fuchsia.feedback.CrashReportingProductRegister"
// * Instantiate the class on a thread with an async_dispatcher.
// TODO(crbug.com/1211174): Remove these requirements.
class WebInstanceHost {
 public:
  // Component URL used to launch WebEngine instances to host Contexts.
  static const char kComponentUrl[];

  WebInstanceHost();
  ~WebInstanceHost();

  WebInstanceHost(const WebInstanceHost&) = delete;
  WebInstanceHost& operator=(const WebInstanceHost&) = delete;

  // Creates a new web_instance Component and connects |services_request| to it.
  // Returns ZX_OK if |params| were valid, and the Component was launched.
  zx_status_t CreateInstanceForContext(
      fuchsia::web::CreateContextParams params,
      fidl::InterfaceRequest<fuchsia::io::Directory> services_request);

  // Enables/disables remote debugging mode in instances created by this host.
  // This may be called at any time, and will not affect pre-existing instances.
  void set_enable_remote_debug_mode(bool enable) {
    enable_remote_debug_mode_ = enable;
  }

  // Sets a set of config-data to use when launching instances, instead of any
  // system-provided config-data. May be called at any time, and will not
  // affect pre-existing instances.
  void set_config_for_test(base::Value config) {
    config_for_test_ = std::move(config);
  }

 private:
  // Returns the Launcher for the isolated Environment in which web instances
  // should run. If the Environment does not presently exist then it will be
  // created.
  fuchsia::sys::Launcher* IsolatedEnvironmentLauncher();

  // Used to manage the isolated Environment that web instances run in.
  fuchsia::sys::LauncherPtr isolated_environment_launcher_;
  fuchsia::sys::EnvironmentControllerPtr isolated_environment_controller_;

  // If true then new instances will have remote debug mode enabled.
  bool enable_remote_debug_mode_ = false;

  // Set by configuration tests.
  base::Value config_for_test_;
};

}  // namespace cr_fuchsia

#endif  // FUCHSIA_ENGINE_WEB_INSTANCE_HOST_WEB_INSTANCE_HOST_H_
