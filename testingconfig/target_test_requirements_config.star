# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This file configures tests to disable for particular CrOS source trees.
# After modifying this file, please run the following command to update
# the generated config JSON file. lucicfg is assumed to be on your PATH, since
# it is part of depot_tools.
#
# lucicfg generate ./target_test_requirements_config.star

load("//target_test_requirements_config_helper.star",
     "set_up_graph",
     "target_test_requirements")

set_up_graph()

# NOTE: This current list of rules comes right out of chromeos_config.py.
# Reference designs are used wherever possible, i.e. when a MAINBOARD_FAMILY
# has been specified in the corresponding coreboot Kconfig file for the build
# target. Build targets are used when those MAINBOARD_FAMILY values are not
# available.
# https://chromium.git.corp.google.com/chromiumos/chromite/+/0efc1dfd96f998d837c105bc6036fdb0bc580c1f/config/chromeos_config.py#2107

# daisy (Exynos5250)
target_test_requirements(
  build_target = "daisy",
  test_suites = [],
)

# slippy (HSW)
target_test_requirements(
  reference_design = "Google_Slippy",
  test_suites = [
      "bvt-inline",
      "bvt-cq",
      "bvt-tast-cq",
  ],
)

# peach (Exynos5420)
target_test_requirements(
  build_target = "peach_pit",
  test_suites = [
      "bvt-inline",
      "bvt-tast-cq",
  ],
)

# rambi (BYT)
target_test_requirements(
  build_target = "winky",
  test_suites = [
      "bvt-inline",
      "bvt-tast-cq",
  ],
)
target_test_requirements(
  build_target = "kip",
  test_suites = [
      "bvt-cq",
  ],
)

# nyan (K1)
target_test_requirements(
  build_target = "nyan_big",
  test_suites = [
      "bvt-inline",
      "bvt-tast-cq",
  ],
)
target_test_requirements(
  build_target = "nyan_kitty",
  test_suites = [
      "bvt-cq",
  ],
)

# auron (BDW)
target_test_requirements(
  build_target = "auron_paine",
  test_suites = [
      "bvt-inline",
      "bvt-tast-cq",
  ],
)
target_test_requirements(
  build_target = "tidus",
  test_suites = [
      "bvt-cq",
  ],
)
target_test_requirements(
  build_target = "auron_yuna",
  test_suites = [
      "bvt-arc",
  ],
)

# pinky (RK3288)
target_test_requirements(
  build_target = "veyron_mighty",
  test_suites = [
      "bvt-inline",
      "bvt-tast-cq",
  ],
)
target_test_requirements(
  build_target = "veyron_speedy",
  test_suites = [
      "bvt-cq",
  ],
)
target_test_requirements(
  build_target = "veyron_minnie",
  test_suites = [
      "bvt-arc",
  ],
)

# strago (BSW)
target_test_requirements(
  build_target = "wizpig",
  test_suites = [
      "bvt-inline",
      "bvt-tast-cq",
  ],
)
target_test_requirements(
  build_target = "edgar",
  test_suites = [
      "bvt-cq",
  ],
)
target_test_requirements(
  build_target = "cyan",
  test_suites = [
      "bvt-arc",
  ],
)

# glados (SKL)
target_test_requirements(
  reference_design = "Google_Glados",
  test_suites = [
      "bvt-inline",
      "bvt-cq",
      "bvt-tast-cq",
  ],
)

# oak (MTK8173)
target_test_requirements(
  build_target = "elm",
  test_suites = [
      "bvt-inline",
      "bvt-tast-cq",
  ],
)
target_test_requirements(
  build_target = "hana",
  test_suites = [
      "bvt-arc",
  ],
)

# gru (RK3399)
target_test_requirements(
  build_target = "bob",
  test_suites = [
      "bvt-inline",
      "bvt-tast-cq",
  ],
)
target_test_requirements(
  build_target = "kevin",
  test_suites = [
      "bvt-arc",
  ],
)

# reef (APL)
target_test_requirements(
  reference_design = "Google_Reef",
  test_suites = [
      "bvt-inline",
      "bvt-tast-cq",
  ],
)

# coral (APL)
target_test_requirements(
  reference_design = "Google_Coral",
  test_suites = [
      "bvt-inline",
      "bvt-tast-cq",
  ],
)

# poppy (KBL)
target_test_requirements(
  reference_design = "Google_Poppy",
  test_suites = [
      "bvt-cq",
      "bvt-arc",
      "bvt-tast-cq",
  ],
)

# Nocturne (KBL)
target_test_requirements(
  reference_design = "Google_Nocturne",
  test_suites = [
      "bvt-inline",
      "bvt-tast-cq",
  ],
)

# gru + arcnext
target_test_requirements(
  build_target = "kevin-arcnext",
  test_suites = [
      "bvt-arc",
      "bvt-tast-cq",
  ],
)

# arcnext
target_test_requirements(
  build_target = "caroline-arcnext",
  test_suites = [
      "bvt-arc",
      "bvt-tast-cq",
  ],
)

# Add for Skylab test
target_test_requirements(
  build_target = "nyan_blaze",
  test_suites = [
      "bvt-inline",
      "bvt-tast-cq",
  ],
)

# scarlet (RK3399 unibuild)
target_test_requirements(
  build_target = "scarlet",
  test_suites = [
      "bvt-inline",
      "bvt-tast-cq",
  ],
)

# grunt (AMD unibuild)
target_test_requirements(
  build_target = "grunt",
  test_suites = [
      "bvt-inline",
      "bvt-arc",
      "bvt-tast-cq",
  ],
)
