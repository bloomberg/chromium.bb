# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Definitions of builders in the chromium.dawn builder group."""

load("//lib/builders.star", "goma")
load("//lib/branches.star", "branches")
load("//lib/ci.star", "ci", "rbe_instance", "rbe_jobs")
load("//lib/consoles.star", "consoles")

ci.defaults.set(
    builder_group = "chromium.dawn",
    executable = ci.DEFAULT_EXECUTABLE,
    execution_timeout = ci.DEFAULT_EXECUTION_TIMEOUT,
    goma_backend = goma.backend.RBE_PROD,
    pool = ci.gpu.POOL,
    service_account = ci.gpu.SERVICE_ACCOUNT,
    thin_tester_cores = 2,
)

consoles.console_view(
    name = "chromium.dawn",
    branch_selector = branches.DESKTOP_EXTENDED_STABLE_MILESTONE,
    ordering = {
        None: ["ToT"],
        "*builder*": ["Builder"],
        "*cpu*": consoles.ordering(short_names = ["x86"]),
        "ToT|Mac": "*builder*",
        "ToT|Windows|Builder": "*cpu*",
        "ToT|Windows|Intel": "*cpu*",
        "ToT|Windows|Nvidia": "*cpu*",
        "DEPS|Mac": "*builder*",
        "DEPS|Windows|Builder": "*cpu*",
        "DEPS|Windows|Intel": "*cpu*",
        "DEPS|Windows|Nvidia": "*cpu*",
    },
)

ci.gpu.linux_builder(
    name = "Dawn Linux x64 Builder",
    console_view_entry = consoles.console_view_entry(
        category = "ToT|Linux|Builder",
        short_name = "x64",
    ),
    goma_backend = None,
    reclient_jobs = rbe_jobs.DEFAULT,
    reclient_instance = rbe_instance.DEFAULT,
)

ci.gpu.linux_builder(
    name = "Dawn Linux x64 DEPS Builder",
    branch_selector = branches.STANDARD_MILESTONE,
    console_view_entry = consoles.console_view_entry(
        category = "DEPS|Linux|Builder",
        short_name = "x64",
    ),
    cq_mirrors_console_view = "mirrors",
    goma_backend = None,
    reclient_jobs = rbe_jobs.DEFAULT,
    reclient_instance = rbe_instance.DEFAULT,
)

ci.thin_tester(
    name = "Dawn Linux x64 DEPS Release (Intel HD 630)",
    branch_selector = branches.STANDARD_MILESTONE,
    console_view_entry = consoles.console_view_entry(
        category = "DEPS|Linux|Intel",
        short_name = "x64",
    ),
    cq_mirrors_console_view = "mirrors",
    triggered_by = ["ci/Dawn Linux x64 DEPS Builder"],
)

ci.thin_tester(
    name = "Dawn Linux x64 DEPS Release (NVIDIA)",
    branch_selector = branches.STANDARD_MILESTONE,
    console_view_entry = consoles.console_view_entry(
        category = "DEPS|Linux|Nvidia",
        short_name = "x64",
    ),
    cq_mirrors_console_view = "mirrors",
    triggered_by = ["ci/Dawn Linux x64 DEPS Builder"],
)

ci.thin_tester(
    name = "Dawn Linux x64 Release (Intel HD 630)",
    console_view_entry = consoles.console_view_entry(
        category = "ToT|Linux|Intel",
        short_name = "x64",
    ),
    triggered_by = ["Dawn Linux x64 Builder"],
)

ci.thin_tester(
    name = "Dawn Linux x64 Release (NVIDIA)",
    console_view_entry = consoles.console_view_entry(
        category = "ToT|Linux|Nvidia",
        short_name = "x64",
    ),
    triggered_by = ["Dawn Linux x64 Builder"],
)

ci.gpu.mac_builder(
    name = "Dawn Mac x64 Builder",
    console_view_entry = consoles.console_view_entry(
        category = "ToT|Mac|Builder",
        short_name = "x64",
    ),
)

ci.gpu.mac_builder(
    name = "Dawn Mac x64 DEPS Builder",
    branch_selector = branches.DESKTOP_EXTENDED_STABLE_MILESTONE,
    console_view_entry = consoles.console_view_entry(
        category = "DEPS|Mac|Builder",
        short_name = "x64",
    ),
    cq_mirrors_console_view = "mirrors",
)

# Note that the Mac testers are all thin Linux VMs, triggering jobs on the
# physical Mac hardware in the Swarming pool which is why they run on linux
ci.thin_tester(
    name = "Dawn Mac x64 DEPS Release (AMD)",
    branch_selector = branches.DESKTOP_EXTENDED_STABLE_MILESTONE,
    console_view_entry = consoles.console_view_entry(
        category = "DEPS|Mac|AMD",
        short_name = "x64",
    ),
    cq_mirrors_console_view = "mirrors",
    triggered_by = ["ci/Dawn Mac x64 DEPS Builder"],
)

ci.thin_tester(
    name = "Dawn Mac x64 DEPS Release (Intel)",
    branch_selector = branches.DESKTOP_EXTENDED_STABLE_MILESTONE,
    console_view_entry = consoles.console_view_entry(
        category = "DEPS|Mac|Intel",
        short_name = "x64",
    ),
    cq_mirrors_console_view = "mirrors",
    triggered_by = ["ci/Dawn Mac x64 DEPS Builder"],
)

ci.thin_tester(
    name = "Dawn Mac x64 Experimental Release (AMD)",
    console_view_entry = consoles.console_view_entry(
        category = "ToT|Mac|AMD",
        short_name = "exp",
    ),
    triggered_by = ["Dawn Mac x64 Builder"],
)

ci.thin_tester(
    name = "Dawn Mac x64 Experimental Release (Intel)",
    console_view_entry = consoles.console_view_entry(
        category = "ToT|Mac|Intel",
        short_name = "exp",
    ),
    triggered_by = ["Dawn Mac x64 Builder"],
)

ci.thin_tester(
    name = "Dawn Mac x64 Release (AMD)",
    console_view_entry = consoles.console_view_entry(
        category = "ToT|Mac|AMD",
        short_name = "x64",
    ),
    triggered_by = ["Dawn Mac x64 Builder"],
)

ci.thin_tester(
    name = "Dawn Mac x64 Release (Intel)",
    console_view_entry = consoles.console_view_entry(
        category = "ToT|Mac|Intel",
        short_name = "x64",
    ),
    triggered_by = ["Dawn Mac x64 Builder"],
)

ci.gpu.windows_builder(
    name = "Dawn Win10 x64 ASAN Release",
    console_view_entry = consoles.console_view_entry(
        category = "ToT|Windows|ASAN",
        short_name = "x64",
    ),
)

ci.gpu.windows_builder(
    name = "Dawn Win10 x64 Builder",
    console_view_entry = consoles.console_view_entry(
        category = "ToT|Windows|Builder",
        short_name = "x64",
    ),
)

ci.gpu.windows_builder(
    name = "Dawn Win10 x64 DEPS Builder",
    branch_selector = branches.DESKTOP_EXTENDED_STABLE_MILESTONE,
    console_view_entry = consoles.console_view_entry(
        category = "DEPS|Windows|Builder",
        short_name = "x64",
    ),
    cq_mirrors_console_view = "mirrors",
)

# Note that the Win testers are all thin Linux VMs, triggering jobs on the
# physical Win hardware in the Swarming pool, which is why they run on linux
ci.thin_tester(
    name = "Dawn Win10 x64 DEPS Release (Intel HD 630)",
    branch_selector = branches.DESKTOP_EXTENDED_STABLE_MILESTONE,
    console_view_entry = consoles.console_view_entry(
        category = "DEPS|Windows|Intel",
        short_name = "x64",
    ),
    cq_mirrors_console_view = "mirrors",
    triggered_by = ["ci/Dawn Win10 x64 DEPS Builder"],
)

ci.thin_tester(
    name = "Dawn Win10 x64 DEPS Release (NVIDIA)",
    branch_selector = branches.DESKTOP_EXTENDED_STABLE_MILESTONE,
    console_view_entry = consoles.console_view_entry(
        category = "DEPS|Windows|Nvidia",
        short_name = "x64",
    ),
    cq_mirrors_console_view = "mirrors",
    triggered_by = ["ci/Dawn Win10 x64 DEPS Builder"],
)

ci.thin_tester(
    name = "Dawn Win10 x64 Release (Intel HD 630)",
    console_view_entry = consoles.console_view_entry(
        category = "ToT|Windows|Intel",
        short_name = "x64",
    ),
    triggered_by = ["Dawn Win10 x64 Builder"],
)

ci.thin_tester(
    name = "Dawn Win10 x64 Release (NVIDIA)",
    console_view_entry = consoles.console_view_entry(
        category = "ToT|Windows|Nvidia",
        short_name = "x64",
    ),
    triggered_by = ["Dawn Win10 x64 Builder"],
)

ci.gpu.windows_builder(
    name = "Dawn Win10 x86 Builder",
    console_view_entry = consoles.console_view_entry(
        category = "ToT|Windows|Builder",
        short_name = "x86",
    ),
)

ci.gpu.windows_builder(
    name = "Dawn Win10 x86 DEPS Builder",
    branch_selector = branches.DESKTOP_EXTENDED_STABLE_MILESTONE,
    console_view_entry = consoles.console_view_entry(
        category = "DEPS|Windows|Builder",
        short_name = "x86",
    ),
    cq_mirrors_console_view = "mirrors",
)

# Note that the Win testers are all thin Linux VMs, triggering jobs on the
# physical Win hardware in the Swarming pool, which is why they run on linux
ci.thin_tester(
    name = "Dawn Win10 x86 DEPS Release (Intel HD 630)",
    branch_selector = branches.DESKTOP_EXTENDED_STABLE_MILESTONE,
    console_view_entry = consoles.console_view_entry(
        category = "DEPS|Windows|Intel",
        short_name = "x86",
    ),
    cq_mirrors_console_view = "mirrors",
    triggered_by = ["ci/Dawn Win10 x86 DEPS Builder"],
)

ci.thin_tester(
    name = "Dawn Win10 x86 DEPS Release (NVIDIA)",
    branch_selector = branches.DESKTOP_EXTENDED_STABLE_MILESTONE,
    console_view_entry = consoles.console_view_entry(
        category = "DEPS|Windows|Nvidia",
        short_name = "x86",
    ),
    cq_mirrors_console_view = "mirrors",
    triggered_by = ["ci/Dawn Win10 x86 DEPS Builder"],
)

ci.thin_tester(
    name = "Dawn Win10 x86 Release (Intel HD 630)",
    console_view_entry = consoles.console_view_entry(
        category = "ToT|Windows|Intel",
        short_name = "x86",
    ),
    triggered_by = ["Dawn Win10 x86 Builder"],
)

ci.thin_tester(
    name = "Dawn Win10 x86 Release (NVIDIA)",
    console_view_entry = consoles.console_view_entry(
        category = "ToT|Windows|Nvidia",
        short_name = "x86",
    ),
    triggered_by = ["Dawn Win10 x86 Builder"],
)
