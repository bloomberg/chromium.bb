#!/usr/bin/env python
# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Code generator for Ozone platform list.

This script takes as arguments a list of platform names and generates a C++
source file containing a list of those platforms. Each list entry contains the
name and a function pointer to the initializer for that platform.

Example Output: ./generate_ozone_platform_list.py --default wayland dri wayland

  #include "ui/ozone/ozone_platform_list.h"

  namespace ui {

  OzonePlatform* CreateOzonePlatformDri();
  OzonePlatform* CreateOzonePlatformWayland();

  const OzonePlatformListEntry kOzonePlatforms[] = {
    { "wayland", &CreateOzonePlatformWayland },
    { "dri", &CreateOzonePlatformDri },
  };

  const int kOzonePlatformCount = 2;

  }  // namespace ui
"""

import optparse
import os
import collections
import re
import sys
import string


def GetConstructorName(name):
  """Determine name of static constructor function from platform name.

  We just capitalize the platform name and prepend "CreateOzonePlatform".
  """

  return 'CreateOzonePlatform' + string.capitalize(name)


def GeneratePlatformList(out, platforms):
  """Generate static array containing a list of ozone platforms."""

  out.write('#include "ui/ozone/ozone_platform_list.h"\n')
  out.write('\n')

  out.write('namespace ui {\n')
  out.write('\n')

  # Prototypes for platform initializers.
  for platform in platforms:
    out.write('OzonePlatform* %s();\n' % GetConstructorName(platform))
  out.write('\n')

  # List of platform names and initializers.
  out.write('const OzonePlatformListEntry kOzonePlatforms[] = {\n')
  for platform in platforms:
    out.write('  { "%s", &%s },\n' % (platform, GetConstructorName(platform)))
  out.write('};\n')
  out.write('\n')

  out.write('const int kOzonePlatformCount = %d;\n' % len(platforms))
  out.write('\n')

  out.write('}  // namespace ui\n')


def main(argv):
  parser = optparse.OptionParser()
  parser.add_option('--output_file')
  parser.add_option('--default')
  options, platforms = parser.parse_args(argv)

  # Write to standard output or file specified by --output_file.
  out = sys.stdout
  if options.output_file:
    out = open(options.output_file, 'wb')

  # Reorder the platforms when --default is specified.
  # The default platform must appear first in the platform list.
  if options.default and options.default in platforms:
    platforms.remove(options.default)
    platforms.insert(0, options.default)

  GeneratePlatformList(out, platforms)

  if options.output_file:
    out.close()

  return 0


if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))
