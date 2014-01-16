# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'libwebm_sources': [
      'source/mkvmuxer.cpp',
      'source/mkvmuxerutil.cpp',
      'source/mkvwriter.cpp',
    ],
  }
}

if (is_android) {
  arm_use_neon = false
  # Our version of arm_neon_optional from common.gypi. This is not used in the
  # current build so is commented out for now.
  #arm_optionally_use_neon = false
} else {
  arm_use_neon = true
  #arm_optionally_use_neon = true
}

if (arm_version == 6) {
  arm_arch = "armv6"
  arm_tune = ""
  arm_float_abi = "softfp"
  arm_fpu = "vfp"
  # Thumb is a reduced instruction set available on some ARM processors that
  # has increased code density.
  arm_use_thumb = false

} else if (arm_version == 7) {
  arm_arch = "armv7-a"
  arm_tune = "cortex-a8"
  arm_float_abi = "softfp"
  arm_use_thumb = true

  if (arm_use_neon) {
    arm_fpu = "neon"
  } else {
    arm_fpu = "vfpv3-d16"
  }
}
