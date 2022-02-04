#!/usr/bin/env python3
# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Rewrite incompatible default symbols in glibc.
"""

import re
import subprocess
import sys

# This constant comes from https://crbug.com/580892
MAX_ALLOWED_GLIBC_VERSION = [2, 17]

VERSION_PATTERN = re.compile('GLIBC_([0-9\.]+)')
SECTION_PATTERN = re.compile(r'^ *\[ *[0-9]+\] +(\S+) +\S+ + ([0-9a-f]+) .*$')

# The two dictionaries below map from symbol name to
# (symbol version, symbol index).
#
# The default version for a given symbol (which may be unsupported).
default_version = {}
# The max supported symbol version for a given symbol.
supported_version = {}

# The file name of the binary we're going to rewrite.
BIN_FILE = sys.argv[1]

# Populate |default_version| and |supported_version| with data from readelf.
stdout = subprocess.check_output(['readelf', '--dyn-syms', '--wide', BIN_FILE])
for line in stdout.decode("utf-8").split('\n'):
    cols = re.split('\s+', line)
    # Skip the preamble.
    if len(cols) < 9:
        continue

    index = cols[1].rstrip(':')
    # Skip the header.
    if not index.isdigit():
        continue

    index = int(index)
    name = cols[8].split('@')
    # Ignore unversioned symbols.
    if len(name) < 2:
        continue

    base_name = name[0]
    version = name[-1]
    # The default version will have '@@' in the name.
    is_default = len(name) > 2

    match = re.match(VERSION_PATTERN, version)
    # Ignore symbols versioned with GLIBC_PRIVATE.
    if not match:
        continue

    version = [int(part) for part in match.group(1).split('.')]
    if version < MAX_ALLOWED_GLIBC_VERSION:
        old_supported_version = supported_version.get(base_name, ([-1], -1))
        supported_version[base_name] = max((version, index),
                                           old_supported_version)
    if is_default:
        default_version[base_name] = (version, index)

# Get the offset into the binary of the .gnu.version section from readelf.
stdout = subprocess.check_output(['readelf', '--sections', '--wide', BIN_FILE])
for line in stdout.decode("utf-8").split('\n'):
    if match := SECTION_PATTERN.match(line):
        section_name, address = match.groups()
        if section_name == '.gnu.version':
            gnu_version_addr = int(address, base=16)
            break
else:
    print('No .gnu.version section found', file=sys.stderr)
    sys.exit(1)

# Rewrite the binary.
bin_data = bytearray(open(BIN_FILE, 'rb').read())
for name, (version, index) in default_version.items():
    # No need to rewrite the default if it's already an allowed version.
    if version <= MAX_ALLOWED_GLIBC_VERSION:
        continue

    # If there's no replacement for this symbol, then skip it.
    if name not in supported_version:
        continue

    _, supported_index = supported_version[name]
    # The .gnu.version section is divided into 16-bit chunks that give the
    # symbol versions.  The 16th bit is a flag that's false for the default
    # version.  The data is stored in little-endian so we need to add 1 to
    # get the address of the byte we want to flip.
    old_default = gnu_version_addr + 2 * index + 1
    new_default = gnu_version_addr + 2 * supported_index + 1
    assert (bin_data[old_default] & 0x80) == 0
    assert (bin_data[new_default] & 0x80) == 0x80
    bin_data[old_default] ^= 0x80
    bin_data[new_default] ^= 0x80
open(BIN_FILE, 'wb').write(bin_data)
