#!/usr/bin/env python
# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import os
import subprocess
import sys
import tempfile
import zipfile

BINARY_FOR_PLATFORM = {
    "linux-x64" : "mojo_shell",
    "android-arm" : "MojoShell.apk"
}

if not sys.platform.startswith("linux"):
  print "Not supported for your platform"
  sys.exit(0)

CURRENT_PATH = os.path.dirname(os.path.realpath(__file__))
PREBUILT_FILE_PATH = os.path.join(CURRENT_PATH, "prebuilt")


def download(tools_directory):
  stamp_path = os.path.join(PREBUILT_FILE_PATH, "VERSION")

  version_path = os.path.join(CURRENT_PATH, "../VERSION")
  with open(version_path) as version_file:
    version = version_file.read().strip()

  try:
    with open(stamp_path) as stamp_file:
      current_version = stamp_file.read().strip()
      if current_version == version:
        return 0  # Already have the right version.
  except IOError:
    pass  # If the stamp file does not exist we need to download new binaries.

  for platform in ["linux-x64", "android-arm"]:
    download_version_for_platform(version, platform, tools_directory)

  with open(stamp_path, 'w') as stamp_file:
    stamp_file.write(version)
  return 0

def download_version_for_platform(version, platform, tools_directory):
  find_depot_tools_path = os.path.join(CURRENT_PATH, tools_directory)
  sys.path.insert(0, find_depot_tools_path)
  # pylint: disable=F0401
  import find_depot_tools

  basename = platform + ".zip"
  gs_path = "gs://mojo/shell/" + version + "/" + basename

  depot_tools_path = find_depot_tools.add_depot_tools_to_path()
  gsutil_exe = os.path.join(depot_tools_path, "third_party", "gsutil", "gsutil")

  with tempfile.NamedTemporaryFile() as temp_zip_file:
    # We're downloading from a public bucket which does not need authentication,
    # but the user might have busted credential files somewhere such as ~/.boto
    # that the gsutil script will try (and fail) to use. Setting these
    # environment variables convinces gsutil not to attempt to use these, but
    # also generates a useless warning about failing to load the file. We want
    # to discard this warning but still preserve all output in the case of an
    # actual failure. So, we run the script and capture all output and then
    # throw the output away if the script succeeds (return code 0).
    env = os.environ.copy()
    env["AWS_CREDENTIAL_FILE"] = ""
    env["BOTO_CONFIG"] = ""
    try:
      subprocess.check_output(
          [gsutil_exe,
           "--bypass_prodaccess",
           "cp",
           gs_path,
           temp_zip_file.name],
          stderr=subprocess.STDOUT,
          env=env)
    except subprocess.CalledProcessError as e:
      print e.output
      sys.exit(1)

    binary_name = BINARY_FOR_PLATFORM[platform]
    with zipfile.ZipFile(temp_zip_file.name) as z:
      zi = z.getinfo(binary_name)
      mode = zi.external_attr >> 16
      z.extract(zi, PREBUILT_FILE_PATH)
      os.chmod(os.path.join(PREBUILT_FILE_PATH, binary_name), mode)


def main():
  parser = argparse.ArgumentParser(description="Download mojo_shell binaries "
                                   "from google storage")
  parser.add_argument("--tools-directory",
                      dest="tools_directory",
                      metavar="<tools-directory>",
                      type=str,
                      help="Path to the directory containing"
                           " find_depot_tools.py, specified as a relative path"
                           " from the location of this file.")
  args = parser.parse_args()
  if not args.tools_directory:
    print "Must specify --tools_directory; please see help message."
    sys.exit(1)
  return download(args.tools_directory)

if __name__ == "__main__":
  sys.exit(main())
