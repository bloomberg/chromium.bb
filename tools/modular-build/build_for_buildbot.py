#!/usr/bin/python

# Copyright 2010 The Native Client Authors.  All rights reserved.
# Use of this source code is governed by a BSD-style license that can
# be found in the LICENSE file.

import subprocess
import sys

import build


# This script exists so that Buildbot can invoke build.py without
# requiring the Buildbot configuration to know about which build.py
# options to use, since the Buildbot configuration lives in a
# different SVN repository and is more awkward to change.
#
# For a further discussion, see:
# http://lackingrhoticity.blogspot.com/2010/05/trouble-with-buildbot.html


def Main(args):
  sys.stderr.write("Running the unit tests...\n")
  subprocess.check_call(["python", "test_all.py"])

  if args == ["trybot"] or args == ["buildbot_incremental"]:
    # We use --allow-overwrite because the source trees are not
    # modified by hand on the trybots/buildbots, but if there are
    # any badly-behaved build steps that write to source trees
    # this cannot be corrected by hand.
    build.Main(["--allow-overwrite", "-b"])
  elif args == ["buildbot_from_scratch"]:
    # --allow-overwrite would not hurt here, but is not needed
    # because we build from scratch.
    sys.stderr.write("Deleting the 'out' directory...\n")
    subprocess.check_call(["rm", "-rf", "out"])
    sys.stderr.write("Running the build...\n")
    build.Main(["-b"])
  else:
    print "Unrecognised arguments: %r" % args
    print ("Usage: %s [trybot | "
           "buildbot_incremental | "
           "buildbot_from_scratch]" % sys.argv[0])
    sys.exit(1)


if __name__ == "__main__":
  Main(sys.argv[1:])
