# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from typing import Dict

# Fields for use when working with a physical linux device connected locally
linux_device_hostname = "192.168.42.32"
linux_device_user = "potat"

linux_out_dir = "out/default"
fuchsia_out_dir = "out/fuchsia"

# A map of test targets to custom filter files. Do not specify the fuchsia
# filters in testing/buildbot/filters. Those are already incorporated into the
# fuchsia filters. The file specified should use the same format as those files,
# though.
test_targets = [
    "base:base_perftests",
    "net:net_perftests",
]
