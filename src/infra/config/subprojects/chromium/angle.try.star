# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

load("//lib/builders.star", "cpu", "goma", "os", "xcode")
load("//lib/try.star", "try_")

try_.defaults.set(
    bucket = "try",
    build_numbers = True,
    builder_group = "tryserver.chromium.angle",
    caches = [
        swarming.cache(
            name = "win_toolchain",
            path = "win_toolchain",
        ),
    ],
    cores = 8,
    cpu = cpu.X86_64,
    cq_group = "cq",
    executable = "recipe:angle_chromium_trybot",
    execution_timeout = 2 * time.hour,
    # Max. pending time for builds. CQ considers builds pending >2h as timed
    # out: http://shortn/_8PaHsdYmlq. Keep this in sync.
    expiration_timeout = 2 * time.hour,
    goma_backend = goma.backend.RBE_PROD,
    os = os.LINUX_DEFAULT,
    pool = "luci.chromium.try",
    service_account = "chromium-try-gpu-builder@chops-service-accounts.iam.gserviceaccount.com",
    subproject_list_view = "luci.chromium.try",
    task_template_canary_percentage = 5,
)

def angle_mac_builder(*, name, **kwargs):
    kwargs.setdefault("builderless", True)
    kwargs.setdefault("cores", None)
    kwargs.setdefault("os", os.MAC_ANY)
    kwargs.setdefault("ssd", None)
    return try_.builder(name = name, **kwargs)

def angle_ios_builder(*, name, **kwargs):
    kwargs.setdefault("xcode", xcode.x12a7209)
    return angle_mac_builder(name = name, **kwargs)

angle_ios_builder(
    name = "ios-angle-try-intel",
    pool = "luci.chromium.gpu.mac.mini.intel.try",
)
