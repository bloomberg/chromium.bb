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
     'gce_sanity_test_config',
     'hw_bvt_arc',
     'hw_bvt_cq',
     'hw_bvt_inline',
     'hw_bvt_tast_cq',
     'moblab_smoke_test_config',
     'set_up_graph',
     'target_test_requirements',
     'tast_vm_cq_test_config',
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
  requirements = [],
)

# slippy (HSW)
target_test_requirements(
  reference_design = 'Google_Slippy',
  requirements = [
      hw_bvt_inline(),
      hw_bvt_cq(),
      hw_bvt_tast_cq(),
  ],
)

# peach (Exynos5420)
target_test_requirements(
  build_target = 'peach_pit',
  requirements = [
      hw_bvt_inline(),
      hw_bvt_tast_cq(),
  ],
)

# rambi (BYT)
target_test_requirements(
  build_target = 'winky',
  requirements = [
      hw_bvt_inline(),
      hw_bvt_tast_cq(),
  ],
)
target_test_requirements(
  build_target = 'kip',
  requirements = [
      hw_bvt_cq(),
  ],
)

# nyan (K1)
target_test_requirements(
  build_target = 'nyan_big',
  requirements = [
      hw_bvt_inline(),
      hw_bvt_tast_cq(),
  ],
)
target_test_requirements(
  build_target = 'nyan_kitty',
  requirements = [
      hw_bvt_cq(),
  ],
)

# auron (BDW)
target_test_requirements(
  build_target = 'auron_paine',
  requirements = [
      hw_bvt_inline(),
      hw_bvt_tast_cq(),
  ],
)
target_test_requirements(
  build_target = 'tidus',
  requirements = [
      hw_bvt_cq(),
  ],
)
target_test_requirements(
  build_target = 'auron_yuna',
  requirements = [
      hw_bvt_arc(),
  ],
)

# pinky (RK3288)
target_test_requirements(
  build_target = 'veyron_mighty',
  requirements = [
      hw_bvt_inline(),
      hw_bvt_tast_cq(),
  ],
)
target_test_requirements(
  build_target = 'veyron_speedy',
  requirements = [
      hw_bvt_cq(),
  ],
)
target_test_requirements(
  build_target = 'veyron_minnie',
  requirements = [
      hw_bvt_arc(),
  ],
)

# strago (BSW)
target_test_requirements(
  build_target = 'wizpig',
  requirements = [
      hw_bvt_inline(),
      hw_bvt_tast_cq(),
  ],
)
target_test_requirements(
  build_target = 'edgar',
  requirements = [
      hw_bvt_cq(),
  ],
)
target_test_requirements(
  build_target = 'cyan',
  requirements = [
      hw_bvt_arc(),
  ],
)

# glados (SKL)
target_test_requirements(
  reference_design = 'Google_Glados',
  requirements = [
      hw_bvt_inline(),
      hw_bvt_cq(),
      hw_bvt_tast_cq(),
  ],
)

# oak (MTK8173)
target_test_requirements(
  build_target = 'elm',
  requirements = [
      hw_bvt_inline(),
      hw_bvt_tast_cq(),
  ],
)
target_test_requirements(
  build_target = 'hana',
  requirements = [
      hw_bvt_arc(),
  ],
)

# gru (RK3399)
target_test_requirements(
  build_target = 'bob',
  requirements = [
      hw_bvt_inline(),
      hw_bvt_tast_cq(),
  ],
)
target_test_requirements(
  build_target = 'kevin',
  requirements = [
      hw_bvt_arc(),
  ],
)

# reef (APL)
target_test_requirements(
  reference_design = 'Google_Reef',
  requirements = [
      hw_bvt_inline(),
      hw_bvt_tast_cq(),
  ],
)

# coral (APL)
target_test_requirements(
  reference_design = 'Google_Coral',
  requirements = [
      hw_bvt_inline(),
      hw_bvt_tast_cq(),
  ],
)

# poppy (KBL)
target_test_requirements(
  reference_design = 'Google_Poppy',
  requirements = [
      hw_bvt_cq(),
      hw_bvt_arc(),
      hw_bvt_tast_cq(),
  ],
)

# Nocturne (KBL)
target_test_requirements(
  reference_design = 'Google_Nocturne',
  requirements = [
      hw_bvt_inline(),
      hw_bvt_tast_cq(),
  ],
)

# gru + arcnext
target_test_requirements(
  build_target = 'kevin-arcnext',
  requirements = [
      hw_bvt_arc(),
      hw_bvt_tast_cq(),
  ],
)

# arcnext
target_test_requirements(
  build_target = 'caroline-arcnext',
  requirements = [
      hw_bvt_arc(),
      hw_bvt_tast_cq(),
  ],
)

# Add for Skylab test
target_test_requirements(
  build_target = 'nyan_blaze',
  requirements = [
      hw_bvt_inline(),
      hw_bvt_tast_cq(),
  ],
)

# scarlet (RK3399 unibuild)
target_test_requirements(
  build_target = 'scarlet',
  requirements = [
      hw_bvt_inline(),
      hw_bvt_tast_cq(),
  ],
)

# grunt (AMD unibuild)
target_test_requirements(
  build_target = 'grunt',
  requirements = [
      hw_bvt_inline(),
      hw_bvt_arc(),
      hw_bvt_tast_cq(),
  ],
)

target_test_requirements(
  build_target = 'amd64-generic-asan',
  requirements = [
      vm_smoke_test_config(use_ctest=True),
  ],
)

target_test_requirements(
  build_target = 'amd64-generic-ubsan',
  requirements = [
      vm_smoke_test_config(use_ctest=True),
  ],
)

target_test_requirements(
  build_target = 'betty-arc64',
  requirements = [
      tast_vm_cq_test_config(),
      vm_smoke_test_config(use_ctest=True),
  ],
)

target_test_requirements(
  build_target = 'betty-arcnext',
  requirements = [
      vm_smoke_test_config(use_ctest=False),
  ],
)

target_test_requirements(
  build_target = 'betty-asan',
  requirements = [
      vm_smoke_test_config(use_ctest=True),
  ],
)

target_test_requirements(
  build_target = 'betty',
  requirements = [
      tast_vm_cq_test_config(),
      vm_smoke_test_config(use_ctest=False),
  ],
)

target_test_requirements(
  build_target = 'lakitu-gpu',
  requirements = [
      gce_sanity_test_config(),
      vm_smoke_test_config(use_ctest=True),
  ],
)

target_test_requirements(
  build_target = 'lakitu-nc',
  requirements = [
      gce_sanity_test_config(),
      vm_smoke_test_config(use_ctest=True),
  ],
)

target_test_requirements(
  build_target = 'lakitu',
  requirements = [
      gce_sanity_test_config(),
      vm_smoke_test_config(use_ctest=True),
  ],
)

target_test_requirements(
  build_target = 'lakitu-st',
  requirements = [
      gce_sanity_test_config(),
      vm_smoke_test_config(use_ctest=True),
  ],
)

target_test_requirements(
  build_target = 'lakitu_next',
  requirements = [
      gce_sanity_test_config(),
      vm_smoke_test_config(use_ctest=True),
  ],
)

target_test_requirements(
  build_target = 'amd64-generic',
  requirements = [
      tast_vm_cq_test_config(),
  ],
)

target_test_requirements(
  build_target = 'moblab-generic-vm',
  requirements = [
      moblab_smoke_test_config(),
  ],
)
