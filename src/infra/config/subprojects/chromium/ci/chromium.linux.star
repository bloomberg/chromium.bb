# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Definitions of builders in the chromium.linux builder group."""

load("//lib/args.star", "args")
load("//lib/builder_config.star", "builder_config")
load("//lib/builders.star", "goma", "os", "sheriff_rotations")
load("//lib/branches.star", "branches")
load("//lib/ci.star", "ci", "rbe_instance", "rbe_jobs")
load("//lib/consoles.star", "consoles")

ci.defaults.set(
    builder_group = "chromium.linux",
    cores = 8,
    executable = ci.DEFAULT_EXECUTABLE,
    execution_timeout = ci.DEFAULT_EXECUTION_TIMEOUT,
    goma_backend = goma.backend.RBE_PROD,
    goma_jobs = goma.jobs.MANY_JOBS_FOR_CI,
    main_console_view = "main",
    notifies = ["chromium.linux"],
    os = os.LINUX_DEFAULT,
    pool = ci.DEFAULT_POOL,
    service_account = ci.DEFAULT_SERVICE_ACCOUNT,
    sheriff_rotations = sheriff_rotations.CHROMIUM,
    tree_closing = True,
)

consoles.console_view(
    name = "chromium.linux",
    branch_selector = branches.FUCHSIA_LTS_MILESTONE,
    ordering = {
        None: ["release", "debug"],
        "release": consoles.ordering(short_names = ["bld", "tst", "nsl", "gcc"]),
        "cast": consoles.ordering(short_names = ["vid", "aud"]),
    },
)

ci.builder(
    name = "Cast Audio Linux",
    console_view_entry = consoles.console_view_entry(
        category = "cast",
        short_name = "aud",
    ),
    ssd = True,
    goma_backend = None,
    reclient_jobs = rbe_jobs.HIGH_JOBS_FOR_CI,
    reclient_instance = rbe_instance.DEFAULT,
)

ci.builder(
    name = "Cast Linux",
    branch_selector = branches.STANDARD_MILESTONE,
    console_view_entry = consoles.console_view_entry(
        category = "cast",
        short_name = "vid",
    ),
    cq_mirrors_console_view = "mirrors",
    goma_backend = None,
    reclient_jobs = rbe_jobs.HIGH_JOBS_FOR_CI,
    reclient_instance = rbe_instance.DEFAULT,
)

ci.builder(
    name = "Cast Linux Debug",
    branch_selector = branches.STANDARD_MILESTONE,
    console_view_entry = consoles.console_view_entry(
        category = "cast",
        short_name = "dbg",
    ),
    cq_mirrors_console_view = "mirrors",
    os = os.LINUX_BIONIC,
    # TODO(crbug.com/1173333): Make it tree-closing.
    tree_closing = False,
    goma_backend = None,
    reclient_jobs = rbe_jobs.HIGH_JOBS_FOR_CI,
    reclient_instance = rbe_instance.DEFAULT,
)

ci.builder(
    name = "Cast Linux ARM64",
    branch_selector = branches.MAIN,
    console_view_entry = consoles.console_view_entry(
        category = "cast",
        short_name = "arm64",
    ),
    cq_mirrors_console_view = "mirrors",
    os = os.LINUX_BIONIC,
    tree_closing = False,
    goma_backend = None,
    reclient_jobs = rbe_jobs.HIGH_JOBS_FOR_CI,
    reclient_instance = rbe_instance.DEFAULT,
)

ci.builder(
    name = "Deterministic Fuchsia (dbg)",
    console_view_entry = [
        consoles.console_view_entry(
            category = "fuchsia|x64",
            short_name = "det",
        ),
        consoles.console_view_entry(
            branch_selector = branches.MAIN,
            console_view = "sheriff.fuchsia",
            category = "misc",
            short_name = "det",
        ),
    ],
    executable = "recipe:swarming/deterministic_build",
    execution_timeout = 6 * time.hour,
    goma_jobs = None,
)

ci.builder(
    name = "Deterministic Linux",
    console_view_entry = consoles.console_view_entry(
        category = "release",
        short_name = "det",
    ),
    executable = "recipe:swarming/deterministic_build",
    execution_timeout = 6 * time.hour,
    # Set tree_closing to false to disable the defaualt tree closer, which
    # filters by step name, and instead enable tree closing for any step
    # failure.
    tree_closing = False,
    notifies = ["Deterministic Linux", "close-on-any-step-failure"],
    goma_backend = None,
    reclient_jobs = rbe_jobs.DEFAULT,
    reclient_instance = rbe_instance.DEFAULT,
)

ci.builder(
    name = "Deterministic Linux (dbg)",
    console_view_entry = consoles.console_view_entry(
        category = "debug|builder",
        short_name = "det",
    ),
    cores = 32,
    executable = "recipe:swarming/deterministic_build",
    execution_timeout = 7 * time.hour,
    goma_backend = None,
    reclient_jobs = rbe_jobs.DEFAULT,
    reclient_instance = rbe_instance.DEFAULT,
)

ci.builder(
    name = "Fuchsia ARM64",
    branch_selector = branches.FUCHSIA_LTS_MILESTONE,
    console_view_entry = [
        consoles.console_view_entry(
            category = "fuchsia|a64",
            short_name = "rel",
        ),
        consoles.console_view_entry(
            branch_selector = branches.MAIN,
            console_view = "sheriff.fuchsia",
            category = "ci",
            short_name = "arm64",
        ),
    ],
    cq_mirrors_console_view = "mirrors",
    notifies = ["cr-fuchsia"],
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "fuchsia_arm64",
                "fuchsia_arm64_host",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
            target_platform = builder_config.target_platform.FUCHSIA,
        ),
        build_gs_bucket = "chromium-linux-archive",
    ),
)

ci.builder(
    name = "Fuchsia x64",
    branch_selector = branches.FUCHSIA_LTS_MILESTONE,
    console_view_entry = [
        consoles.console_view_entry(
            category = "fuchsia|x64",
            short_name = "rel",
        ),
        consoles.console_view_entry(
            branch_selector = branches.MAIN,
            console_view = "sheriff.fuchsia",
            category = "ci",
            short_name = "x64",
        ),
    ],
    cq_mirrors_console_view = "mirrors",
    notifies = ["cr-fuchsia"],
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "fuchsia_x64",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
            target_platform = builder_config.target_platform.FUCHSIA,
        ),
        build_gs_bucket = "chromium-linux-archive",
    ),
)

ci.builder(
    name = "Leak Detection Linux",
    console_view_entry = consoles.console_view_entry(
        console_view = "chromium.fyi",
        category = "linux",
        short_name = "lk",
    ),
    main_console_view = None,
    notifies = args.ignore_default([]),
    tree_closing = False,
    goma_backend = None,
    reclient_jobs = rbe_jobs.DEFAULT,
    reclient_instance = rbe_instance.DEFAULT,
    sheriff_rotations = args.ignore_default(None),
)

ci.builder(
    name = "Linux Builder",
    branch_selector = branches.STANDARD_MILESTONE,
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "use_clang_coverage",
                "enable_reclient",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
        ),
        build_gs_bucket = "chromium-linux-archive",
    ),
    console_view_entry = consoles.console_view_entry(
        category = "release",
        short_name = "bld",
    ),
    cq_mirrors_console_view = "mirrors",
    goma_backend = None,
    reclient_jobs = rbe_jobs.HIGH_JOBS_FOR_CI,
    reclient_instance = rbe_instance.DEFAULT,
)

ci.builder(
    name = "Linux Builder (dbg)",
    branch_selector = branches.STANDARD_MILESTONE,
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = ["enable_reclient"],
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = ["mb"],
            build_config = builder_config.build_config.DEBUG,
            target_bits = 64,
        ),
        build_gs_bucket = "chromium-linux-archive",
    ),
    console_view_entry = consoles.console_view_entry(
        category = "debug|builder",
        short_name = "64",
    ),
    cq_mirrors_console_view = "mirrors",
    goma_backend = None,
    reclient_jobs = rbe_jobs.DEFAULT,
    reclient_instance = rbe_instance.DEFAULT,
)

ci.builder(
    name = "Linux Builder (Wayland)",
    branch_selector = branches.STANDARD_MILESTONE,
    console_view_entry = consoles.console_view_entry(
        category = "release",
        short_name = "bld-wl",
    ),
    cq_mirrors_console_view = "mirrors",
    goma_backend = None,
    reclient_jobs = rbe_jobs.DEFAULT,
    reclient_instance = rbe_instance.DEFAULT,
)

ci.builder(
    name = "Linux Tests",
    branch_selector = branches.STANDARD_MILESTONE,
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "use_clang_coverage",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
        ),
        build_gs_bucket = "chromium-linux-archive",
    ),
    console_view_entry = consoles.console_view_entry(
        category = "release",
        short_name = "tst",
    ),
    cq_mirrors_console_view = "mirrors",
    goma_backend = None,
    triggered_by = ["ci/Linux Builder"],
    # TODO(crbug.com/1249968): Roll this out more broadly.
    resultdb_bigquery_exports = [
        resultdb.export_text_artifacts(
            bq_table = "chrome-luci-data.chromium.ci_text_artifacts",
            predicate = resultdb.artifact_predicate(
                # Only archive output snippets since some tests can generate
                # very large supplementary files.
                content_type_regexp = "snippet",
            ),
        ),
    ],
)

ci.builder(
    name = "Linux Tests (dbg)(1)",
    branch_selector = branches.STANDARD_MILESTONE,
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = ["enable_reclient"],
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = ["mb"],
            build_config = builder_config.build_config.DEBUG,
            target_bits = 64,
        ),
        build_gs_bucket = "chromium-linux-archive",
    ),
    console_view_entry = consoles.console_view_entry(
        category = "debug|tester",
        short_name = "64",
    ),
    cq_mirrors_console_view = "mirrors",
    triggered_by = ["ci/Linux Builder (dbg)"],
    goma_backend = None,
    reclient_jobs = rbe_jobs.DEFAULT,
    reclient_instance = rbe_instance.DEFAULT,
)

ci.builder(
    name = "Linux Tests (Wayland)",
    branch_selector = branches.STANDARD_MILESTONE,
    console_view_entry = consoles.console_view_entry(
        category = "release",
        short_name = "tst-wl",
    ),
    cq_mirrors_console_view = "mirrors",
    goma_backend = None,
    triggered_by = ["ci/Linux Builder (Wayland)"],
)

ci.builder(
    name = "fuchsia-arm64-cast",
    branch_selector = branches.FUCHSIA_LTS_MILESTONE,
    console_view_entry = [
        consoles.console_view_entry(
            category = "fuchsia|cast",
            short_name = "a64",
        ),
        consoles.console_view_entry(
            branch_selector = branches.MAIN,
            console_view = "sheriff.fuchsia",
            category = "ci",
            short_name = "arm64-cast",
        ),
    ],
    cq_mirrors_console_view = "mirrors",
    # Set tree_closing to false to disable the defaualt tree closer, which
    # filters by step name, and instead enable tree closing for any step
    # failure.
    tree_closing = False,
    notifies = ["cr-fuchsia", "close-on-any-step-failure"],
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "fuchsia_arm64",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
            target_platform = builder_config.target_platform.FUCHSIA,
        ),
        build_gs_bucket = "chromium-linux-archive",
    ),
)

ci.builder(
    name = "Network Service Linux",
    console_view_entry = consoles.console_view_entry(
        category = "release",
        short_name = "nsl",
    ),
    goma_backend = None,
    reclient_jobs = rbe_jobs.DEFAULT,
    reclient_instance = rbe_instance.DEFAULT,
)

ci.builder(
    name = "fuchsia-x64-cast",
    branch_selector = branches.FUCHSIA_LTS_MILESTONE,
    console_view_entry = [
        consoles.console_view_entry(
            category = "fuchsia|cast",
            short_name = "x64",
        ),
        consoles.console_view_entry(
            branch_selector = branches.MAIN,
            console_view = "sheriff.fuchsia",
            category = "ci",
            short_name = "x64-cast",
        ),
    ],
    cq_mirrors_console_view = "mirrors",
    # Set tree_closing to false to disable the defaualt tree closer, which
    # filters by step name, and instead enable tree closing for any step
    # failure.
    tree_closing = False,
    notifies = ["cr-fuchsia", "close-on-any-step-failure"],
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "fuchsia_x64",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
            target_platform = builder_config.target_platform.FUCHSIA,
        ),
        build_gs_bucket = "chromium-linux-archive",
    ),
)

ci.builder(
    name = "fuchsia-x64-dbg",
    console_view_entry = [
        consoles.console_view_entry(
            category = "fuchsia|x64",
            short_name = "dbg",
        ),
        consoles.console_view_entry(
            branch_selector = branches.MAIN,
            console_view = "sheriff.fuchsia",
            category = "ci",
            short_name = "x64-dbg",
        ),
    ],
    notifies = ["cr-fuchsia"],
    goma_backend = None,
    reclient_jobs = rbe_jobs.HIGH_JOBS_FOR_CI,
    reclient_instance = rbe_instance.DEFAULT,
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "fuchsia_x64",
                "enable_reclient",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "mb",
            ],
            build_config = builder_config.build_config.DEBUG,
            target_bits = 64,
            target_platform = builder_config.target_platform.FUCHSIA,
        ),
        build_gs_bucket = "chromium-linux-archive",
    ),
)

ci.builder(
    name = "linux-bfcache-rel",
    console_view_entry = consoles.console_view_entry(
        category = "bfcache",
        short_name = "bfc",
    ),
    goma_backend = None,
    reclient_jobs = rbe_jobs.DEFAULT,
    reclient_instance = rbe_instance.DEFAULT,
)

ci.builder(
    name = "linux-extended-tracing-rel",
    console_view_entry = consoles.console_view_entry(
        category = "release",
        short_name = "trc",
    ),
    goma_backend = None,
    reclient_jobs = rbe_jobs.DEFAULT,
    reclient_instance = rbe_instance.DEFAULT,
)

ci.builder(
    name = "linux-gcc-rel",
    console_view_entry = consoles.console_view_entry(
        category = "release",
        short_name = "gcc",
    ),
    goma_backend = None,
)
