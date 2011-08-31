#!/usr/bin/python
# Copyright (c) 2011 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import re
import subprocess
import sys


def Call(command):
  print command
  subprocess.check_call(command)


PLATFORM_MAP = {
    "linux2": "linux",
    "linux3": "linux",
    "darwin": "mac",
    "win32": "win",
    }

def GetPlatform():
  if sys.platform == "linux2":
    # This is not accurate if we are running a 32-bit userland on a
    # 64-bit kernel, but it is good enough for the existing bots.
    if os.uname()[4] == "x86_64":
      return "linux_x86-64"
    elif re.match("i.86$", os.uname()[4]) is not None:
      return "linux_x86-32"
  return PLATFORM_MAP.get(sys.platform, sys.platform)


def Main():
  sys.stdout.write("@@@BUILD_STEP publish tarball@@@\n")
  # TODO(mseaborn): Include the following in the pathname:
  #  * The SVN revision number.
  #  * The name of the Buildbot build slave (possibly as an alternative
  #    to discovering the OS and architecture).
  # This awaits a Buildbot config change to pipe that info through via
  # environment variables.
  tarball = "nacl_glibc_toolchain_%s.tar.gz" % GetPlatform()
  dest_path = "nativeclient-archive2/glibc_toolchain/latest/%s" % tarball
  googlestorage_dest = "gs://%s" % dest_path
  download_url = "http://commondatastorage.googleapis.com/%s" % dest_path
  download_url2 = "http://gsdview.appspot.com/%s" % dest_path
  sys.stdout.write("We are creating a tarball which will be uploaded to:\n%s\n"
                   % googlestorage_dest)
  sys.stdout.write("This will be downloadable from the following URL:\n%s\n"
                   % download_url)
  sys.stdout.write("This will also be downloadable at the following "
                   "site, which adds a directory index:\n%s\n\n"
                   % download_url2)

  Call(["tar", "-cvzf", tarball, "-C", "out/input-prefix", "glibc_toolchain"])

  # This depends on the following on the Buildbots/trybots:
  #  * the directory layout (/b/build/...);
  #  * the ~/.boto file which contains credentials for gsutil.
  Call(["/b/build/scripts/slave/gsutil",
        "-h", "Cache-Control:no-cache",
        "cp", "-a", "public-read", tarball, googlestorage_dest])

  # Print the links in Buildbot only after we have successfully
  # uploaded to them, to avoid potential confusion.
  sys.stdout.write("@@@STEP_LINK@download tarball@%s@@@\n" % download_url)
  sys.stdout.write("@@@STEP_LINK@download tarball (indexed site)@%s@@@\n"
                   % download_url2)


if __name__ == "__main__":
  Main()
