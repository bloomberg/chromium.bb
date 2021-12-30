# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Definitions of builders in the chromium.mac builder group."""

load("//lib/branches.star", "branches")
load("//lib/builders.star", "cpu", "goma", "os", "sheriff_rotations", "xcode")
load("//lib/ci.star", "ci")
load("//lib/consoles.star", "consoles")

ci.defaults.set(
    builder_group = "chromium.mac",
    executable = ci.DEFAULT_EXECUTABLE,
    execution_timeout = ci.DEFAULT_EXECUTION_TIMEOUT,
    goma_backend = goma.backend.RBE_PROD,
    main_console_view = "main",
    os = os.MAC_11,
    pool = ci.DEFAULT_POOL,
    service_account = ci.DEFAULT_SERVICE_ACCOUNT,
    sheriff_rotations = sheriff_rotations.CHROMIUM,
    thin_tester_cores = 8,
    tree_closing = True,
)

consoles.console_view(
    name = "chromium.mac",
    branch_selector = branches.DESKTOP_EXTENDED_STABLE_MILESTONE,
    ordering = {
        None: ["release"],
        "release": consoles.ordering(short_names = ["bld"]),
        "debug": consoles.ordering(short_names = ["bld"]),
        "ios|default": consoles.ordering(short_names = ["dev", "sim"]),
    },
)

consoles.console_view(
    name = "sheriff.ios",
    title = "iOS Sheriff Console",
    ordering = {
        "*type*": consoles.ordering(short_names = ["dev", "sim"]),
        None: ["chromium.mac", "chromium.fyi"],
        "chromium.mac": "*type*",
        "chromium.fyi|13": "*type*",
    },
)

def ios_builder(*, name, **kwargs):
    kwargs.setdefault("sheriff_rotations", sheriff_rotations.IOS)
    kwargs.setdefault("xcode", xcode.x13main)
    return ci.builder(name = name, **kwargs)

ci.builder(
    name = "Mac Builder",
    branch_selector = branches.DESKTOP_EXTENDED_STABLE_MILESTONE,
    console_view_entry = consoles.console_view_entry(
        category = "release",
        short_name = "bld",
    ),
    cq_mirrors_console_view = "mirrors",
    os = os.MAC_DEFAULT,
)

ci.builder(
    name = "Mac Builder (dbg)",
    branch_selector = branches.DESKTOP_EXTENDED_STABLE_MILESTONE,
    console_view_entry = consoles.console_view_entry(
        category = "debug",
        short_name = "bld",
    ),
    cq_mirrors_console_view = "mirrors",
    os = os.MAC_ANY,
)

ci.builder(
    name = "mac-arm64-on-arm64-rel",

    # TODO(crbug.com/1186823): Expand to more branches when all M1 bots are
    # rosettaless.
    branch_selector = branches.MAIN,
    console_view_entry = consoles.console_view_entry(
        category = "release|arm64",
        short_name = "a64",
    ),
    cpu = cpu.ARM64,
    os = os.MAC_11,
)

ci.builder(
    name = "mac-arm64-rel",
    branch_selector = branches.DESKTOP_EXTENDED_STABLE_MILESTONE,
    console_view_entry = consoles.console_view_entry(
        category = "release|arm64",
        short_name = "bld",
    ),
    os = os.MAC_ANY,
)

ci.thin_tester(
    name = "mac11-arm64-rel-tests",
    branch_selector = branches.DESKTOP_EXTENDED_STABLE_MILESTONE,
    console_view_entry = consoles.console_view_entry(
        category = "release|arm64",
        short_name = "11",
    ),
    tree_closing = False,
    triggered_by = ["ci/mac-arm64-rel"],
)

ci.thin_tester(
    name = "Mac10.11 Tests",
    branch_selector = branches.DESKTOP_EXTENDED_STABLE_MILESTONE,
    console_view_entry = consoles.console_view_entry(
        category = "release",
        short_name = "11",
    ),
    cq_mirrors_console_view = "mirrors",
    triggered_by = ["ci/Mac Builder"],
)

ci.thin_tester(
    name = "Mac10.12 Tests",
    branch_selector = branches.DESKTOP_EXTENDED_STABLE_MILESTONE,
    console_view_entry = consoles.console_view_entry(
        category = "release",
        short_name = "12",
    ),
    cq_mirrors_console_view = "mirrors",
    triggered_by = ["ci/Mac Builder"],
)

ci.thin_tester(
    name = "Mac10.13 Tests",
    branch_selector = branches.DESKTOP_EXTENDED_STABLE_MILESTONE,
    console_view_entry = consoles.console_view_entry(
        category = "release",
        short_name = "13",
    ),
    cq_mirrors_console_view = "mirrors",
    triggered_by = ["ci/Mac Builder"],
)

ci.thin_tester(
    name = "Mac10.14 Tests",
    branch_selector = branches.DESKTOP_EXTENDED_STABLE_MILESTONE,
    console_view_entry = consoles.console_view_entry(
        category = "release",
        short_name = "14",
    ),
    cq_mirrors_console_view = "mirrors",
    triggered_by = ["ci/Mac Builder"],
)

ci.thin_tester(
    name = "Mac10.15 Tests",
    branch_selector = branches.DESKTOP_EXTENDED_STABLE_MILESTONE,
    console_view_entry = consoles.console_view_entry(
        category = "release",
        short_name = "15",
    ),
    cq_mirrors_console_view = "mirrors",
    triggered_by = ["ci/Mac Builder"],
)

ci.thin_tester(
    name = "Mac11 Tests",
    branch_selector = branches.DESKTOP_EXTENDED_STABLE_MILESTONE,
    console_view_entry = consoles.console_view_entry(
        category = "mac",
        short_name = "11",
    ),
    triggered_by = ["ci/Mac Builder"],
)

ci.thin_tester(
    name = "Mac11 Tests (dbg)",
    branch_selector = branches.DESKTOP_EXTENDED_STABLE_MILESTONE,
    console_view_entry = consoles.console_view_entry(
        category = "debug",
        short_name = "11",
    ),
    cq_mirrors_console_view = "mirrors",
    triggered_by = ["ci/Mac Builder (dbg)"],
)

ios_builder(
    name = "ios-device",
    console_view_entry = [
        consoles.console_view_entry(
            category = "ios|default",
            short_name = "dev",
        ),
        consoles.console_view_entry(
            branch_selector = branches.MAIN,
            console_view = "sheriff.ios",
            category = "chromium.mac",
            short_name = "dev",
        ),
    ],
    # We don't have necessary capacity to run this configuration in CQ, but it
    # is part of the main waterfall
)

ios_builder(
    name = "ios-simulator",
    branch_selector = branches.STANDARD_MILESTONE,
    console_view_entry = [
        consoles.console_view_entry(
            category = "ios|default",
            short_name = "sim",
        ),
        consoles.console_view_entry(
            branch_selector = branches.MAIN,
            console_view = "sheriff.ios",
            category = "chromium.mac",
            short_name = "sim",
        ),
    ],
    cq_mirrors_console_view = "mirrors",
)

ios_builder(
    name = "ios-simulator-full-configs",
    branch_selector = branches.STANDARD_MILESTONE,
    console_view_entry = [
        consoles.console_view_entry(
            category = "ios|default",
            short_name = "ful",
        ),
        consoles.console_view_entry(
            branch_selector = branches.MAIN,
            console_view = "sheriff.ios",
            category = "chromium.mac",
            short_name = "ful",
        ),
    ],
    cq_mirrors_console_view = "mirrors",
)

ios_builder(
    name = "ios-simulator-noncq",
    console_view_entry = [
        consoles.console_view_entry(
            category = "ios|default",
            short_name = "non",
        ),
        consoles.console_view_entry(
            branch_selector = branches.MAIN,
            console_view = "sheriff.ios",
            category = "chromium.mac",
            short_name = "non",
        ),
    ],
    # We don't have necessary capacity to run this configuration in CQ, but it
    # is part of the main waterfall
    xcode = xcode.x13main,
)
