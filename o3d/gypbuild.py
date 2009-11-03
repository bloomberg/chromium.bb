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
import re
import subprocess
import platform
sys.path.append('build')
import is_admin
from optparse import OptionParser


class GypBuilder(object):
  """A class to help build gyp projects in a cross platform way"""

  class Builder(object):
    """Base Class for building."""

    def __init__(self, builder):
      self.builder = builder

    def Log(self, *args):
      """Prints something if verbose is true."""
      self.builder.Log(args)

    def Execute(self, args):
      """Executes an external program if execute is true."""
      self.builder.Execute(args)

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

    def CleanTargets(self, targets, options):
      """Cleans the targets."""
      print "clean not implemented for this platform."


  class OSXBuilder(Builder):
    """Class for building on OSX."""

    def __init__(self, builder):
      GypBuilder.Builder.__init__(self, builder)

    def GetSolutionPath(self):
      """Gets the solution path."""
      return '%s.xcodeproj' % GypBuilder.base_name

    def CleanTargets(self, targets, options):
      """Cleans the specifed targets."""
      solution = self.GetSolutionPath()
      self.Execute(['xcodebuild',
                    '-project', solution,
                    'clean'])

    def Dobuild(self, targets, options):
      """Builds the specifed targets."""
      solution = self.GetSolutionPath()
      self.Execute(['xcodebuild',
                    '-project', solution])

  class WinBuilder(Builder):
    """Class for building on Windows."""

    def __init__(self, builder):
      GypBuilder.Builder.__init__(self, builder)

    def GetSolutionPath(self):
      """Gets the solution path."""
      return os.path.abspath('%s.sln' % GypBuilder.base_name)

    def CheckVisualStudioVersionVsSolution(self, solution):
      """Checks the solution matches the cl version."""
      f = open(solution, "r")
      line = f.readline()
      f.close()
      m = re.search(r'Format Version (\d+)\.', line)
      if m:
        solution_version = int(m.group(1))
      else:
        print "FAILURE: Unknown solution version in %s" % solution
        sys.exit(1)

      output = subprocess.Popen(['cl.exe'],
                                stdout=subprocess.PIPE,
                                stderr=subprocess.PIPE).communicate()[1]
      m = re.search(r'Compiler Version (\d+)\.', output)
      if m:
        compiler_version = int(m.group(1))
      else:
        print "FAILURE: Unknown cl.exe version."
        sys.exit(1)

      #                        Compiler    Solution
      # Visual Studio .NET 2005   14          9
      # Visual Studio .NET 2008   15         10
      # Visual Studio .NET 2010   ??         ??
      if (compiler_version - 14) > (solution_version - 9):
        vs_map = {
          14: '2005',
          15: '2008',
          16: '2010',
        }
        sln_map = {
          9: '2005',
          10: '2008',
          11: '2010',
        }
        vs_version = vs_map[compiler_version]
        print ("ERROR: solution (%s) version does not match "
               "Visual Studio version (%s)" %
               (sln_map[solution_version], vs_version))
        print "You should 'set GYP_MSVS_VERSION=auto'"
        print "and run 'gclient runhooks --force'"
        sys.exit(1)

    def CleanTargets(self, targets, options):
      """Cleans the targets."""
      solution = self.GetSolutionPath()
      self.Execute(['devenv.com',
                    solution,
                    '/clean',
                    options.version])

    def Dobuild(self, targets, options):
      """Builds the specifed targets."""
      solution = self.GetSolutionPath()
      if not is_admin.IsAdmin():
        print ("WARNING: selenium_ie will not run unless you run as admin "
               "or turn off UAC.\nAfter switching to admin run "
               "'gclient runhooks --force'")
      self.CheckVisualStudioVersionVsSolution(solution)
      self.Execute(['devenv.com',
                    solution,
                    '/build',
                    options.version])
      # TODO(gman): Should I check for devenv and if it does not exist
      # use msbuild? Msbuild is significantly slower than devenv.
      #self.Execute(['msbuild',
      #              solution,
      #              '/p:Configuration=%s' % options.version])

  class LinuxBuilder(Builder):
    """Class for building on Linux."""

    def __init__(self, builder):
      GypBuilder.Builder.__init__(self, builder)

    def GetSolutionPath(self):
      """Gets the solution path."""
      return '%s_main.scons' % GypBuilder.base_name

    def CleanTargets(self, targets, options):
      """Cleans the targets."""
      solution = self.GetSolutionPath()
      self.Execute(['hammer',
                    '-f', solution,
                    '--clean'])

    def Dobuild(self, targets, options):
      """Builds the specifed targets."""
      solution = self.GetSolutionPath()
      self.Execute(['hammer',
                    '-f', solution])

  # Use "o3d" for chrome only build?
  base_name = "o3d_all"

  def __init__(self, args):
    self.execute = True
    self.verbose = False

    modes = ["build", "presubmit", "selenium", "unit_tests"]
    versions = ["Debug", "Release"]

    parser = OptionParser()
    parser.add_option(
        "--list-targets", action="store_true",
        help="lists all available targets.")
    parser.add_option(
        "--no-execute", action="store_true", default=False,
        help="just prints commands that would get executed.")
    parser.add_option(
        "--verbose", action="store_true",
        help="prints more output.")
    parser.add_option(
        "--targets", action="append",
        help="targets to build separated by commas.")
    parser.add_option(
        "--clean", action="store_true",
        help="cleans the targets.")
    parser.add_option(
        "--rebuild", action="store_true",
        help="cleans, then builds targets")
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

    os.chdir("build")

    # Create a platform specific builder.
    if os.name == 'nt':
      builder = self.WinBuilder(self)
    elif platform.system() == 'Darwin':
      builder = self.OSXBuilder(self)
    elif platform.system() == 'Linux':
      builder = self.LinuxBuilder(self)
    else:
      print "ERROR: Unknown platform."
      sys.exit(1)

    # clean if asked.
    if options.clean or options.rebuild:
      builder.CleanTargets(targets, options)
      if not options.rebuild:
        return

    # call a Do method based on the mode.
    func = getattr(builder, "Do%s" % options.mode)
    func(targets, options)

  def Log(self, *args):
    """Prints something if verbose is true."""
    if self.verbose:
      print args

  def Execute(self, args):
    """Executes an external program if execute is true."""
    if self.execute:
      self.Log(" ".join(args))
      if subprocess.call(args) > 0:
        raise RuntimeError("FAILED: " + " ".join(args))
    else:
      print " ".join(args)


def main(args):
  GypBuilder(args[1:])

if __name__ == "__main__":
  main(sys.argv)

