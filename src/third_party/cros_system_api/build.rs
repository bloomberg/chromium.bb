// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Generates the Rust D-Bus bindings for system_api. The generated bindings are included in the
// published crate since the source XML files are only available from the original path or the
// ebuild.

use std::path::Path;

use chromeos_dbus_bindings::{self, generate_module};

// The parent path of system_api.
const SOURCE_DIR: &str = "..";

// (<module name>, <relative path to source xml>)
// When adding additional bindings, remember to include the source project and subtree in the
// ebuild. Otherwise, the source files will not be accessible when building system_api-rust.
const BINDINGS_TO_GENERATE: &[(&str, &str)] = &[
    (
        "org_chromium_authpolicy",
        "authpolicy/dbus_bindings/org.chromium.AuthPolicy.xml",
    ),
    (
        "org_chromium_debugd",
        "debugd/dbus_bindings/org.chromium.debugd.xml",
    ),
    (
        "org_chromium_sessionmanagerinterface",
        "login_manager/dbus_bindings/org.chromium.SessionManagerInterface.xml",
    ),
];

fn main() {
    generate_module(Path::new(SOURCE_DIR), BINDINGS_TO_GENERATE).unwrap();
}
