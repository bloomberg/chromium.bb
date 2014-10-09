# Copyright 2014 Google Inc. All rights reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#    http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

import os
import subprocess
import sys
import unittest

# This ensures that absolute imports of typ modules will work when
# running typ/cmdline.py as a script even if typ is not installed.
# We need this entry in addition to the one in __main__.py to ensure
# that typ/cmdline.py works when invoked via subprocess on windows in
# _spawn_main().
dir_above_typ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
if dir_above_typ not in sys.path:  # pragma: untested
    sys.path.append(dir_above_typ)

from typ.host import Host
from typ.runner import Runner


def main(argv=None, host=None, loader=None):
    host = host or Host()
    loader = loader or unittest.loader.TestLoader()
    runner = Runner(host=host, loader=loader)
    return runner.main(argv)


def spawn_main():  # pragma: untested
    # This function is called from __main__.py when running 'python -m typ' on
    # windows.
    #
    # In order to use multiprocessing on windows, the initial module needs
    # to be importable, and __main__.py isn't.
    #
    # This code instead spawns a subprocess that invokes main.py directly,
    # getting around the problem.
    #
    # We don't want to always spawn a subprocess, because doing so is more
    # heavyweight than it needs to be on other platforms (and can make
    # debugging a bit more annoying).
    proc = subprocess.Popen([sys.executable, __file__] + sys.argv[1:])
    try:
        proc.wait()
    except KeyboardInterrupt:
        # We may need a second wait in order to make sure the subprocess exits
        # completely.
        proc.wait()
    return proc.returncode


if __name__ == '__main__':  # pragma: untested
    sys.exit(main())
