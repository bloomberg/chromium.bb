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


def Execute(args):
  """Executes an external program."""
  # Comment the next line in for debugging.
  # print "Execute: ", ' '.join(args)
  print " ".join(args)
  if subprocess.call(args) > 0:
    raise RuntimeError('FAILED: ' + ' '.join(args))


def main(args):
  os.chdir('build')
  if os.name == 'nt':
    Execute(['msbuild',
             os.path.abspath('all.sln')] + args[1:])
  elif platform.system() == 'Darwin':
    Execute(['xcodebuild',
             '-project', 'all.xcodeproj'])
  elif platform.system() == 'Linux':
    Execute(['hammer',
             '-f', 'all_main.scons'])
  else:
    print "Error: Unknown platform", os.name

if __name__ == '__main__':
  main(sys.argv)

