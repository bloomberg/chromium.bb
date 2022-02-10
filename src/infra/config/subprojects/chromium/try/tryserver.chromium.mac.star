# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Definitions of builders in the tryserver.chromium.mac builder group."""

load("//lib/branches.star", "branches")
load("//lib/builders.star", "cpu", "goma", "os", "xcode")
load("//lib/try.star", "try_")
load("//lib/consoles.star", "consoles")
load("//project.star", "settings")

try_.defaults.set(
    builder_group = "tryserver.chromium.mac",
    builderless = True,
    orchestrator_cores = 2,
    executable = try_.DEFAULT_EXECUTABLE,
    execution_timeout = try_.DEFAULT_EXECUTION_TIMEOUT,
    goma_backend = goma.backend.RBE_PROD,
    compilator_goma_jobs = goma.jobs.J150,
    os = os.MAC_ANY,
    pool = try_.DEFAULT_POOL,
    service_account = try_.DEFAULT_SERVICE_ACCOUNT,
    ssd = True,
)

def ios_builder(*, name, **kwargs):
    kwargs.setdefault("builderless", False)
    kwargs.setdefault("os", os.MAC_11)
    kwargs.setdefault("ssd", None)
    kwargs.setdefault("xcode", xcode.x13main)
    return try_.builder(name = name, **kwargs)

consoles.list_view(
    name = "tryserver.chromium.mac",
    branch_selector = branches.DESKTOP_EXTENDED_STABLE_MILESTONE,
)

try_.builder(
    name = "mac-osxbeta-rel",
    os = os.MAC_DEFAULT,
)

try_.builder(
    name = "mac-inverse-fieldtrials-fyi-rel",
    os = os.MAC_DEFAULT,
)

try_.builder(
    name = "mac-rel",
    branch_selector = branches.DESKTOP_EXTENDED_STABLE_MILESTONE,
    builderless = not settings.is_main,
    use_clang_coverage = True,
    goma_jobs = goma.jobs.J150,
    main_list_view = "try",
    os = os.MAC_DEFAULT,
    tryjob = try_.job(),
)

try_.orchestrator_builder(
    name = "mac-rel-orchestrator",
    compilator = "mac-rel-compilator",
    main_list_view = "try",
    use_clang_coverage = True,
    tryjob = try_.job(
        experiment_percentage = 1,
    ),
)

try_.compilator_builder(
    name = "mac-rel-compilator",
    main_list_view = "try",
    os = os.MAC_DEFAULT,
)

try_.orchestrator_builder(
    name = "mac11-arm64-rel",
    compilator = "mac11-arm64-rel-compilator",
    main_list_view = "try",
    tryjob = try_.job(
        experiment_percentage = 100,
    ),
)

try_.compilator_builder(
    name = "mac11-arm64-rel-compilator",
    main_list_view = "try",
    os = os.MAC_11,
    # TODO (crbug.com/1245171): Revert when root issue is fixed
    grace_period = 4 * time.minute,
)

# NOTE: the following trybots aren't sensitive to Mac version on which
# they are built, hence no additional dimension is specified.
# The 10.xx version translates to which bots will run isolated tests.
try_.builder(
    name = "mac_chromium_10.11_rel_ng",
)

try_.builder(
    name = "mac_chromium_10.12_rel_ng",
)

try_.builder(
    name = "mac_chromium_10.13_rel_ng",
)

try_.builder(
    name = "mac_chromium_10.14_rel_ng",
)

try_.builder(
    name = "mac_chromium_10.15_rel_ng",
)

try_.builder(
    name = "mac_chromium_11.0_rel_ng",
    builderless = False,
)

try_.builder(
    name = "mac_chromium_archive_rel_ng",
)

try_.builder(
    name = "mac_chromium_asan_rel_ng",
    goma_jobs = goma.jobs.J150,
)

try_.builder(
    name = "mac_chromium_compile_dbg_ng",
    branch_selector = branches.DESKTOP_EXTENDED_STABLE_MILESTONE,
    goma_jobs = goma.jobs.J150,
    os = os.MAC_DEFAULT,
    main_list_view = "try",
    tryjob = try_.job(),
)

try_.builder(
    name = "mac_chromium_compile_rel_ng",
)

try_.builder(
    name = "mac_chromium_dbg_ng",
)

try_.builder(
    name = "mac_upload_clang",
    builderless = False,
    executable = "recipe:chromium_upload_clang",
    execution_timeout = 6 * time.hour,
    goma_backend = None,  # Does not use Goma.
)

try_.builder(
    name = "mac_upload_clang_arm",
    builderless = False,
    executable = "recipe:chromium_upload_clang",
    execution_timeout = 6 * time.hour,
    goma_backend = None,  # Does not use Goma.
)

ios_builder(
    name = "ios-catalyst",
)

ios_builder(
    name = "ios-device",
)

ios_builder(
    name = "ios-simulator",
    branch_selector = branches.STANDARD_MILESTONE,
    main_list_view = "try",
    use_clang_coverage = True,
    coverage_exclude_sources = "ios_test_files_and_test_utils",
    coverage_test_types = ["overall", "unit"],
    tryjob = try_.job(),
)

ios_builder(
    name = "ios-simulator-cronet",
    branch_selector = branches.STANDARD_MILESTONE,
    check_for_flakiness = True,
    main_list_view = "try",
    tryjob = try_.job(
        location_regexp = [
            ".+/[+]/components/cronet/.+",
            ".+/[+]/components/grpc_support/.+",
            ".+/[+]/ios/.+",
        ],
        location_regexp_exclude = [
            ".+/[+]/components/cronet/android/.+",
        ],
    ),
)

ios_builder(
    name = "ios-simulator-full-configs",
    branch_selector = branches.STANDARD_MILESTONE,
    check_for_flakiness = True,
    main_list_view = "try",
    use_clang_coverage = True,
    coverage_exclude_sources = "ios_test_files_and_test_utils",
    coverage_test_types = ["overall", "unit"],
    tryjob = try_.job(
        location_regexp = [
            ".+/[+]/ios/.+",
        ],
    ),
)

ios_builder(
    name = "ios-simulator-inverse-fieldtrials-fyi",
)

ios_builder(
    name = "ios-simulator-multi-window",
)

ios_builder(
    name = "ios-simulator-noncq",
    xcode = xcode.x13main,
    tryjob = try_.job(
        location_regexp = [
            ".+/[+]/third_party/crashpad/crashpad/.+",
        ],
    ),
)

ios_builder(
    name = "ios14-beta-simulator",
    os = os.MAC_11,
)

ios_builder(
    name = "ios14-sdk-simulator",
    os = os.MAC_11,
    cpu = cpu.ARM64,
)

ios_builder(
    name = "ios15-beta-simulator",
)

ios_builder(
    name = "ios15-sdk-simulator",
    xcode = xcode.x13latestbeta,
)

try_.gpu.optional_tests_builder(
    name = "mac_optional_gpu_tests_rel",
    branch_selector = branches.DESKTOP_EXTENDED_STABLE_MILESTONE,
    main_list_view = "try",
    ssd = None,
    tryjob = try_.job(
        location_regexp = [
            ".+/[+]/chrome/browser/vr/.+",
            ".+/[+]/content/browser/xr/.+",
            ".+/[+]/content/test/gpu/.+",
            ".+/[+]/gpu/.+",
            ".+/[+]/media/audio/.+",
            ".+/[+]/media/base/.+",
            ".+/[+]/media/capture/.+",
            ".+/[+]/media/filters/.+",
            ".+/[+]/media/gpu/.+",
            ".+/[+]/media/mojo/.+",
            ".+/[+]/media/renderers/.+",
            ".+/[+]/media/video/.+",
            ".+/[+]/services/shape_detection/.+",
            ".+/[+]/testing/buildbot/chromium.gpu.fyi.json",
            ".+/[+]/testing/trigger_scripts/.+",
            ".+/[+]/third_party/blink/renderer/modules/mediastream/.+",
            ".+/[+]/third_party/blink/renderer/modules/webcodecs/.+",
            ".+/[+]/third_party/blink/renderer/modules/webgl/.+",
            ".+/[+]/third_party/blink/renderer/platform/graphics/gpu/.+",
            ".+/[+]/tools/clang/scripts/update.py",
            ".+/[+]/ui/gl/.+",
        ],
    ),
)

# RTS builders

try_.builder(
    name = "mac-rel-rts",
    builderless = False,
    goma_jobs = goma.jobs.J150,
    use_clang_coverage = True,
)

ios_builder(
    name = "ios-simulator-rts",
    builderless = False,
    coverage_exclude_sources = "ios_test_files_and_test_utils",
    coverage_test_types = ["unit"],
    use_clang_coverage = True,
)
