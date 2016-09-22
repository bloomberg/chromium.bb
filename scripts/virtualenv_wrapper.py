#!/usr/bin/env python2
# Copyright 2016 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Wrapper around chromite executable scripts that use virtualenv."""

from __future__ import print_function

import os
import sys
import wrapper

# TODO(akeshet): This transitively imports a bunch of other dependencies,
# including cbuildbot.contants. Ideally we wouldn't need that much junk in this
# wrapper, and importing all that prior to entering the virtualenv might
# actually cause issues.
from chromite.lib import cros_build_lib

# TODO(akeshet): Since we are using the above lib which imports
# cbuildbot.constants anyway, we might as well make use of it in determining
# CHROMITE_PATH. If we want to eliminate this import, we can duplicate the
# chromite path finding code. It would look something like this:
#   path = os.path.dirname(os.path.realpath(__file__))
#   while not os.path.exists(os.path.join(path, 'PRESUBMIT.cfg')):
#     path = os.path.dirname(path)
from chromite.cbuildbot import constants

_CHROMITE_DIR = constants.CHROMITE_DIR
_IN_VENV = 'IN_CHROMITE_VENV'


if __name__ == '__main__':
  if _IN_VENV in os.environ:
    wrapper.DoMain()
  else:
    create_cmd = os.path.join(_CHROMITE_DIR, 'venv', 'create_env.sh')
    cros_build_lib.RunCommand([create_cmd])
    python_cmd = os.path.join(_CHROMITE_DIR, 'venv', 'venv', 'bin', 'python')
    cmd = [python_cmd] + sys.argv
    o = cros_build_lib.RunCommand(
        cmd, extra_env={_IN_VENV: '1'},
        mute_output=False, error_code_ok=True)
    exit(o.returncode)
