#!/bin/bash
# Copyright 2010 The Native Client Authors.  All rights reserved.
# Use of this source code is governed by a BSD-style license that can
# be found in the LICENSE file.

# simple script to generate unsandboxed x86-32 images which can still
# be executed by a "disarmed" sel_ldr
# invocation is just like regular gcc except that most std header and
# libs are not available by default.

set -o nounset
set -o errexit

# the linker script was obtained via
# src/third_party/nacl_sdk/linux/sdk/nacl-sdk/bin/nacl-ld --verbose
readonly LD_SCRIPT=$(dirname $0)/ld_script_x86_untrusted
readonly INC_PATH=src/third_party/nacl_sdk/linux/sdk/nacl-sdk/nacl/include

exec gcc -m32 -nostdinc -nostdlib -I ${INC_PATH} -Wl,-T -Wl,${LD_SCRIPT} "$@"
