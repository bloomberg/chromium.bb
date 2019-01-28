# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This file configures tests to disable for particular CrOS source trees.
# After modifying this file, please run the following commands to update
# the generated config JSON file.
#
# go get -u go.chromium.org/luci/lucicfg/cmd/lucicfg
# ${GOBIN}/lucicfg generate ./source_tree_test_config.star

load("//source_tree_test_config_helper.star", "source_tree_test_config")

source_tree_test_config(
    source_trees = [
        "crostools",
        "src/platform/depthcharge",
        "src/platform/factory",
        "src/platform/factory_installer",
        "src/platform/scribe",
        "src/private-overlays/project-goofy-private",
        "src/third_party/coreboot",
        "src/third_party/seabios",
        "src/third_party/toolchain-utils",
    ],
    disable_hw_tests = True,
    disable_vm_tests = True,
)

source_tree_test_config(
    source_trees = [
        "src/platform/ec",
    ],
    disable_hw_tests = True,
    disable_image_tests = True,
    disable_vm_tests = True,
)

