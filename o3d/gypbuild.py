#! /usr/bin/env python
# Copyright 2009 Google Inc.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# Builds a particlar platform so the user does not have to know platform
# specific build commands for every single platform.

# TODO(gman): Add help.
# TODO(gman): Add cross platform modes like "debug", "opt", "test", "docs"
# TODO(gman): Add cross platform switches like "-clean" and "-rebuild".
# TODO(gman): Add cross platform options like "-verbose".
# TODO(gman): Add cross platform options like "-presubmit", "-selenium",
#     "-unit_tests"

import os
import os.path
import sys
import subprocess
import platform
from optparse import OptionParser


class GypBuilder(object):
  """A class to help build gyp projects in a cross platform way"""

  def __init__(self, args):
    self.execute = True
    self.verbose = False

    modes = ["build", "presubmit", "selenium", "unit_tests"]
    versions = ["Debug", "Release"]

    parser = OptionParser()
    parser.add_option(
        "--list-targets", action="store_true",
        help="lists all available targets")
    parser.add_option(
        "--no-execute", action="store_true", default=False,
        help="just print commands that would get executed")
    parser.add_option(
        "--verbose", action="store_true",
        help="prints more output")
    parser.add_option(
        "--targets", action="append",
        help="targets to build separated by commas.")
    parser.add_option(
        "--clean", action="store_true",
        help="clean the targets")
    parser.add_option(
        "--version", choices=versions, default="Debug",
        help="version to build. Versions are '%s'. Default='Debug' " %
             "', '".join(versions))
    parser.add_option(
        "--mode", choices=modes, default="build",
        help="mode to use. Valid modes are '%s'. Default='build' " %
             "', '".join(modes))

    (options, args) = parser.parse_args(args=args)

    self.verbose = options.verbose
    self.execute = not options.no_execute

    if options.list_targets:
      print "Not yet implemented"
      sys.exit(0)

    self.Log("mode:", options.mode)

    targets = options.targets
    if targets:
      # flatten the targets.
      targets = sum([t.split(",") for t in targets], [])

    # call a Do method based on the mode.
    os.chdir("build")
    func = getattr(self, "Do%s" % options.mode)
    func(targets, options)

  def Log(self, *args):
    if self.verbose:
      print args

  def Execute(self, args):
    """Executes an external program."""
    if self.execute:
      self.Log(" ".join(args))
      if subprocess.call(args) > 0:
        raise RuntimeError("FAILED: " + " ".join(args))
    else:
      print " ".join(args)

  def Dobuild(self, targets, options):
    """Builds the specifed targets."""
    if os.name == 'nt':
      self.Execute(['msbuild',
                    os.path.abspath('o3d_all.sln'),
                    '/p:Configuration=%s' % options.version])
    elif platform.system() == 'Darwin':
      self.Execute(['xcodebuild',
                    '-project', 'o3d_all.xcodeproj'])
    elif platform.system() == 'Linux':
      self.Execute(['hammer',
                    '-f', 'all_main.scons'])
    else:
      print "Error: Unknown platform", os.name

  def Dopresubmit(self, targets, options):
    """Builds and runs both the unit tests and selenium."""
    self.Dounit_tests(targets, options)
    self.Doselenium(targets, options)

  def Doselenium(self, targets, options):
    """Builds and runs the selenium tests."""
    print "selenium not yet implemented."

  def Dounit_tests(self, targets, options):
    """Builds and runs the unit tests."""
    print "unit_tests not yet implemented."


def main(args):
  GypBuilder(args[1:])

if __name__ == "__main__":
  main(sys.argv)

