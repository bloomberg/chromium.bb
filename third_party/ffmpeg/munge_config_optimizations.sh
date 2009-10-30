# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This script munges the config.h to mark EBP unavailable so that the ffmpeg
# library can be compiled as WITHOUT -fomit-frame-pointer allowing for
# breakpad to work.
#
# Without this, building without -fomit-frame-pointer on ia32 will result in
# the the inclusion of a number of inline assembly blocks that use too many
# registers for its input/output operands.  This will cause gcc to barf with:
#
#   error: can't find a register in class ‘GENERAL_REGS’ while reloading ‘asm’
#
# This modification should only be required on ia32, and not x64.
#
# Note that HAVE_EBX_AVAILABLE is another flag available in config.h.  One would
# think that setting this to 0 would allow for ffmpeg to be built with -fPIC.
# However, not all the assembly blocks requiring 6 registers are excluded by
# this flag.

set -e

sed -i.orig -e '
/HAVE_EBP_AVAILABLE/ {
c\
#define HAVE_EBP_AVAILABLE 0
}
' \
$1
