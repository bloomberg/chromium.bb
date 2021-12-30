# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Definitions of builders in the chromium.gpu builder group."""

load("//lib/branches.star", "branches")
load("//lib/builders.star", "goma", "sheriff_rotations")
load("//lib/ci.star", "ci")
load("//lib/consoles.star", "consoles")

ci.defaults.set(
    builder_group = "chromium.gpu",
    executable = ci.DEFAULT_EXECUTABLE,
    execution_timeout = ci.DEFAULT_EXECUTION_TIMEOUT,
    goma_backend = goma.backend.RBE_PROD,
    pool = ci.gpu.POOL,
    service_account = ci.DEFAULT_SERVICE_ACCOUNT,
    sheriff_rotations = sheriff_rotations.CHROMIUM_GPU,
    tree_closing = True,
    tree_closing_notifiers = ci.gpu.TREE_CLOSING_NOTIFIERS,
    thin_tester_cores = 2,
)

consoles.console_view(
    name = "chromium.gpu",
    branch_selector = branches.DESKTOP_EXTENDED_STABLE_MILESTONE,
    ordering = {
        None: ["Windows", "Mac", "Linux"],
    },
)

ci.gpu.linux_builder(
    name = "Android Release (Nexus 5X)",
    branch_selector = branches.STANDARD_MILESTONE,
    console_view_entry = consoles.console_view_entry(
        category = "Android",
    ),
    cq_mirrors_console_view = "mirrors",
)

ci.gpu.linux_builder(
    name = "GPU Linux Builder",
    branch_selector = branches.STANDARD_MILESTONE,
    console_view_entry = consoles.console_view_entry(
        category = "Linux",
    ),
    cq_mirrors_console_view = "mirrors",
)

ci.gpu.linux_builder(
    name = "GPU Linux Builder (dbg)",
    console_view_entry = consoles.console_view_entry(
        category = "Linux",
    ),
    tree_closing = False,
)

ci.gpu.mac_builder(
    name = "GPU Mac Builder",
    branch_selector = branches.DESKTOP_EXTENDED_STABLE_MILESTONE,
    console_view_entry = consoles.console_view_entry(
        category = "Mac",
    ),
    cq_mirrors_console_view = "mirrors",
)

ci.gpu.mac_builder(
    name = "GPU Mac Builder (dbg)",
    console_view_entry = consoles.console_view_entry(
        category = "Mac",
    ),
    tree_closing = False,
)

ci.gpu.windows_builder(
    name = "GPU Win x64 Builder",
    branch_selector = branches.DESKTOP_EXTENDED_STABLE_MILESTONE,
    console_view_entry = consoles.console_view_entry(
        category = "Windows",
    ),
    cq_mirrors_console_view = "mirrors",
)

ci.gpu.windows_builder(
    name = "GPU Win x64 Builder (dbg)",
    console_view_entry = consoles.console_view_entry(
        category = "Windows",
    ),
    tree_closing = False,
)

ci.thin_tester(
    name = "Linux Debug (NVIDIA)",
    console_view_entry = consoles.console_view_entry(
        category = "Linux",
    ),
    triggered_by = ["GPU Linux Builder (dbg)"],
    tree_closing = False,
)

ci.thin_tester(
    name = "Linux Release (NVIDIA)",
    branch_selector = branches.STANDARD_MILESTONE,
    cq_mirrors_console_view = "mirrors",
    console_view_entry = consoles.console_view_entry(
        category = "Linux",
    ),
    triggered_by = ["ci/GPU Linux Builder"],
)

ci.thin_tester(
    name = "Mac Debug (Intel)",
    console_view_entry = consoles.console_view_entry(
        category = "Mac",
    ),
    triggered_by = ["GPU Mac Builder (dbg)"],
    tree_closing = False,
)

ci.thin_tester(
    name = "Mac Release (Intel)",
    branch_selector = branches.DESKTOP_EXTENDED_STABLE_MILESTONE,
    console_view_entry = consoles.console_view_entry(
        category = "Mac",
    ),
    cq_mirrors_console_view = "mirrors",
    triggered_by = ["ci/GPU Mac Builder"],
)

ci.thin_tester(
    name = "Mac Retina Debug (AMD)",
    console_view_entry = consoles.console_view_entry(
        category = "Mac",
    ),
    triggered_by = ["GPU Mac Builder (dbg)"],
    tree_closing = False,
)

ci.thin_tester(
    name = "Mac Retina Release (AMD)",
    branch_selector = branches.DESKTOP_EXTENDED_STABLE_MILESTONE,
    console_view_entry = consoles.console_view_entry(
        category = "Mac",
    ),
    cq_mirrors_console_view = "mirrors",
    triggered_by = ["ci/GPU Mac Builder"],
)

ci.thin_tester(
    name = "Win10 x64 Debug (NVIDIA)",
    console_view_entry = consoles.console_view_entry(
        category = "Windows",
    ),
    triggered_by = ["GPU Win x64 Builder (dbg)"],
    tree_closing = False,
)

ci.thin_tester(
    name = "Win10 x64 Release (NVIDIA)",
    branch_selector = branches.DESKTOP_EXTENDED_STABLE_MILESTONE,
    console_view_entry = consoles.console_view_entry(
        category = "Windows",
    ),
    cq_mirrors_console_view = "mirrors",
    triggered_by = ["ci/GPU Win x64 Builder"],
)
