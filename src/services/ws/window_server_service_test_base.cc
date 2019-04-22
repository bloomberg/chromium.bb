// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ws/window_server_service_test_base.h"

#include <memory>

#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "services/service_manager/public/cpp/manifest_builder.h"
#include "services/ws/common/switches.h"
#include "services/ws/public/mojom/constants.mojom.h"
#include "services/ws/public/mojom/window_tree.mojom.h"
#include "services/ws/test_ws/test_manifest.h"
#include "ui/gl/gl_switches.h"

namespace ws {

namespace {

const char kTestAppName[] = "ui_ws2_service_unittests";

void EnsureCommandLineSwitch(const std::string& name) {
  base::CommandLine* cmd_line = base::CommandLine::ForCurrentProcess();
  if (!cmd_line->HasSwitch(name))
    cmd_line->AppendSwitch(name);
}

}  // namespace

WindowServerServiceTestBase::WindowServerServiceTestBase()
    : test_service_manager_(
          {test_ws::GetManifest(),
           service_manager::ManifestBuilder()
               .WithServiceName(kTestAppName)
               .ExposeCapability("ui:window_tree_client",
                                 service_manager::Manifest::InterfaceList<
                                     ws::mojom::WindowTreeClient>())
               .RequireCapability(kTestAppName, "ui:window_tree_client")
               .RequireCapability(ws::mojom::kServiceName, "app")
               .RequireCapability("viz", "viz_host")
               .Build()}),
      test_service_binding_(
          this,
          test_service_manager_.RegisterTestInstance(kTestAppName)) {
  EnsureCommandLineSwitch(switches::kUseTestConfig);
  EnsureCommandLineSwitch(::switches::kOverrideUseSoftwareGLForTests);
}

WindowServerServiceTestBase::~WindowServerServiceTestBase() = default;

const char* WindowServerServiceTestBase::test_name() const {
  return kTestAppName;
}

}  // namespace ws
