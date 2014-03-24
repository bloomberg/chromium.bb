# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'sources': [
    'src/celt/arm/arm_celt_map.c',
    'src/celt/arm/armcpu.c',
    'src/celt/arm/armcpu.h',
    'src/celt/arm/fixed_armv4.h',
    'src/celt/arm/fixed_armv5e.h',
    'src/celt/arm/kiss_fft_armv4.h',
    'src/celt/arm/kiss_fft_armv5e.h',
    'src/celt/pitch_arm.h',
    'src/silk/arm/macro_armv4.h',
    'src/silk/arm/macro_armv5e.h',
    'src/silk/arm/SigProc_FIX_armv4.h',
    'src/silk/arm/SigProc_FIX_armv5e.h',
    '<(INTERMEDIATE_DIR)/celt_pitch_xcorr_arm_gnu.S',
  ],
  'actions': [
    {
      'action_name': 'convert_assembler',
      'inputs': [
        'src/celt/arm/arm2gnu.pl',
        'src/celt/arm/celt_pitch_xcorr_arm.s',
      ],
      'outputs': [
        '<(INTERMEDIATE_DIR)/celt_pitch_xcorr_arm_gnu.S',
      ],
      'action': [
        'bash',
        '-c',
        'perl src/celt/arm/arm2gnu.pl src/celt/arm/celt_pitch_xcorr_arm.s | sed "s/OPUS_ARM_MAY_HAVE_[A-Z]*/1/g" | sed "/.include/d" > <(INTERMEDIATE_DIR)/celt_pitch_xcorr_arm_gnu.S',
      ],
      'message': 'Convert Opus assembler for ARM.'
    },
  ],
}
