# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This file configures tests to disable for particular CrOS source trees.
# After modifying this file, please run the following command to update
# the generated config JSON file. lucicfg is assumed to be on your PATH, since
# it is part of depot_tools.
#
# lucicfg generate ./target_test_requirements_config.star

load('//target_test_requirements_config_helper.star',
     'set_up_graph',
     'standard_bvt_arc',
     'standard_bvt_cq',
     'standard_bvt_inline',
     'standard_bvt_tast_cq',
     'target_test_requirements',
     'vm_smoke_test_config')

set_up_graph()

# NOTE: This current list of rules comes right out of chromeos_config.py.
# Reference designs are used wherever possible, i.e. when a MAINBOARD_FAMILY
# has been specified in the corresponding coreboot Kconfig file for the build
# target. Build targets are used when those MAINBOARD_FAMILY values are not
# available.
# https://chromium.git.corp.google.com/chromiumos/chromite/+/0efc1dfd96f998d837c105bc6036fdb0bc580c1f/config/chromeos_config.py#2107

# daisy (Exynos5250)
target_test_requirements(
  build_target = 'daisy',
  hw_test_configs = [],
)

# slippy (HSW)
target_test_requirements(
  reference_design = 'Google_Slippy',
  hw_test_configs = [
      standard_bvt_inline(),
      standard_bvt_cq(),
      standard_bvt_tast_cq(),
  ],
)

# peach (Exynos5420)
target_test_requirements(
  build_target = 'peach_pit',
  hw_test_configs = [
      standard_bvt_inline(),
      standard_bvt_tast_cq(),
  ],
)

# rambi (BYT)
target_test_requirements(
  build_target = 'winky',
  hw_test_configs = [
      standard_bvt_inline(),
      standard_bvt_tast_cq(),
  ],
)
target_test_requirements(
  build_target = 'kip',
  hw_test_configs = [
      standard_bvt_cq(),
  ],
)

# nyan (K1)
target_test_requirements(
  build_target = 'nyan_big',
  hw_test_configs = [
      standard_bvt_inline(),
      standard_bvt_tast_cq(),
  ],
)
target_test_requirements(
  build_target = 'nyan_kitty',
  hw_test_configs = [
      standard_bvt_cq(),
  ],
)

# auron (BDW)
target_test_requirements(
  build_target = 'auron_paine',
  hw_test_configs = [
      standard_bvt_inline(),
      standard_bvt_tast_cq(),
  ],
)
target_test_requirements(
  build_target = 'tidus',
  hw_test_configs = [
      standard_bvt_cq(),
  ],
)
target_test_requirements(
  build_target = 'auron_yuna',
  hw_test_configs = [
      standard_bvt_arc(),
  ],
)

# pinky (RK3288)
target_test_requirements(
  build_target = 'veyron_mighty',
  hw_test_configs = [
      standard_bvt_inline(),
      standard_bvt_tast_cq(),
  ],
)
target_test_requirements(
  build_target = 'veyron_speedy',
  hw_test_configs = [
      standard_bvt_cq(),
  ],
)
target_test_requirements(
  build_target = 'veyron_minnie',
  hw_test_configs = [
      standard_bvt_arc(),
  ],
)

# strago (BSW)
target_test_requirements(
  build_target = 'wizpig',
  hw_test_configs = [
      standard_bvt_inline(),
      standard_bvt_tast_cq(),
  ],
)
target_test_requirements(
  build_target = 'edgar',
  hw_test_configs = [
      standard_bvt_cq(),
  ],
)
target_test_requirements(
  build_target = 'cyan',
  hw_test_configs = [
      standard_bvt_arc(),
  ],
)

# glados (SKL)
target_test_requirements(
  reference_design = 'Google_Glados',
  hw_test_configs = [
      standard_bvt_inline(),
      standard_bvt_cq(),
      standard_bvt_tast_cq(),
  ],
)

# oak (MTK8173)
target_test_requirements(
  build_target = 'elm',
  hw_test_configs = [
      standard_bvt_inline(),
      standard_bvt_tast_cq(),
  ],
)
target_test_requirements(
  build_target = 'hana',
  hw_test_configs = [
      standard_bvt_arc(),
  ],
)

# gru (RK3399)
target_test_requirements(
  build_target = 'bob',
  hw_test_configs = [
      standard_bvt_inline(),
      standard_bvt_tast_cq(),
  ],
)
target_test_requirements(
  build_target = 'kevin',
  hw_test_configs = [
      standard_bvt_arc(),
  ],
)

# reef (APL)
target_test_requirements(
  reference_design = 'Google_Reef',
  hw_test_configs = [
      standard_bvt_inline(),
      standard_bvt_tast_cq(),
  ],
)

# coral (APL)
target_test_requirements(
  reference_design = 'Google_Coral',
  hw_test_configs = [
      standard_bvt_inline(),
      standard_bvt_tast_cq(),
  ],
)

# poppy (KBL)
target_test_requirements(
  reference_design = 'Google_Poppy',
  hw_test_configs = [
      standard_bvt_cq(),
      standard_bvt_arc(),
      standard_bvt_tast_cq(),
  ],
)

# Nocturne (KBL)
target_test_requirements(
  reference_design = 'Google_Nocturne',
  hw_test_configs = [
      standard_bvt_inline(),
      standard_bvt_tast_cq(),
  ],
)

# gru + arcnext
target_test_requirements(
  build_target = 'kevin-arcnext',
  hw_test_configs = [
      standard_bvt_arc(),
      standard_bvt_tast_cq(),
  ],
)

# arcnext
target_test_requirements(
  build_target = 'caroline-arcnext',
  hw_test_configs = [
      standard_bvt_arc(),
      standard_bvt_tast_cq(),
  ],
)

# Add for Skylab test
target_test_requirements(
  build_target = 'nyan_blaze',
  hw_test_configs = [
      standard_bvt_inline(),
      standard_bvt_tast_cq(),
  ],
)

# scarlet (RK3399 unibuild)
target_test_requirements(
  build_target = 'scarlet',
  hw_test_configs = [
      standard_bvt_inline(),
      standard_bvt_tast_cq(),
  ],
)

# grunt (AMD unibuild)
target_test_requirements(
  build_target = 'grunt',
  hw_test_configs = [
      standard_bvt_inline(),
      standard_bvt_arc(),
      standard_bvt_tast_cq(),
  ],
)

target_test_requirements(
  build_target = 'amd64-generic-asan',
  vm_test_configs = [
      vm_smoke_test_config(use_ctest=True),
  ],
)

target_test_requirements(
  build_target = 'amd64-generic-ubsan',
  vm_test_configs = [
      vm_smoke_test_config(use_ctest=True),
  ],
)

target_test_requirements(
  build_target = 'betty-arc64',
  vm_test_configs = [
      vm_smoke_test_config(use_ctest=True),
  ],
)

target_test_requirements(
  build_target = 'betty-arcnext',
  vm_test_configs = [
      vm_smoke_test_config(use_ctest=False),
  ],
)

target_test_requirements(
  build_target = 'betty-asan',
  vm_test_configs = [
      vm_smoke_test_config(use_ctest=True),
  ],
)

target_test_requirements(
  build_target = 'betty',
  vm_test_configs = [
      vm_smoke_test_config(use_ctest=False),
  ],
)

target_test_requirements(
  build_target = 'lakitu-gpu',
  vm_test_configs = [
      vm_smoke_test_config(use_ctest=True),
  ],
)

target_test_requirements(
  build_target = 'lakitu-nc',
  vm_test_configs = [
      vm_smoke_test_config(use_ctest=True),
  ],
)

target_test_requirements(
  build_target = 'lakitu',
  vm_test_configs = [
      vm_smoke_test_config(use_ctest=True),
  ],
)

target_test_requirements(
  build_target = 'lakitu-st',
  vm_test_configs = [
      vm_smoke_test_config(use_ctest=True),
  ],
)

target_test_requirements(
  build_target = 'lakitu_next',
  vm_test_configs = [
      vm_smoke_test_config(use_ctest=True),
  ],
)

