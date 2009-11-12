#!/usr/bin/env python
# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# Checks if the user is running as admin

import os
import os.path
import sys
import subprocess
import platform
import re

def IsAdminNT():
  # (1, 4, 0): '95',
  # (1, 4, 10): '98',
  # (1, 4, 90): 'ME',
  # (2, 4, 0): 'NT',
  # (2, 5, 0): '2000',
  # (2, 5, 1): 'XP',
  # (2, 5, 2): '2003',
  v = sys.getwindowsversion()
  v = v[3], v[0], v[1]
  # check that we are in vista or greater so we know that
  # 'whoami.exe' exists.
  if v[0] > 2 or (v[0] == 2 and v[1] >= 6):
    output = subprocess.Popen(['whoami.exe', '/all'],
                              stdout=subprocess.PIPE,
                              stderr=subprocess.PIPE).communicate()[0]
    elevated_sid = "S-1-16-12288"
    admin_sid = "S-1-5-32-544"

    if re.search(elevated_sid, output) and re.search(admin_sid, output):
      return True
    return False
  else:
    # Where in XP or 2000. For now just assume we are admin.
    return True

def IsAdmin():
  if os.name == 'nt':
    is_admin = IsAdminNT()
  elif platform.system() == 'Darwin':
    is_admin = False
  elif platform.system() == 'Linux':
    is_admin = False
  else:
    is_admin = False
  return is_admin


def main(args):
  sys.stdout.write(str(IsAdmin()))


if __name__ == "__main__":
  main(sys.argv)
  sys.exit(0)
