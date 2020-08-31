# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os.path
import sys


# Set up |sys.path| so that this module works without user-side setup of
# PYTHONPATH assuming Chromium's directory tree structure.
def _setup_sys_path():
    expected_path = 'third_party/blink/renderer/bindings/scripts/bind_gen/'

    this_dir = os.path.dirname(__file__)
    root_dir = os.path.abspath(
        os.path.join(this_dir, *(['..'] * expected_path.count('/'))))

    module_dirs = (
        # //third_party/blink/renderer/bindings/scripts/web_idl
        os.path.join(root_dir, 'third_party', 'blink', 'renderer', 'bindings',
                     'scripts'),
        # //third_party/blink/renderer/build/scripts/blinkbuild
        os.path.join(root_dir, 'third_party', 'blink', 'renderer', 'build',
                     'scripts'),
        # //third_party/mako/mako
        os.path.join(root_dir, 'third_party', 'mako'),
    )
    for module_dir in reversed(module_dirs):
        # Preserve sys.path[0] as is.
        # https://docs.python.org/3/library/sys.html?highlight=path[0]#sys.path
        sys.path.insert(1, module_dir)


_setup_sys_path()

from .dictionary import generate_dictionaries
from .enumeration import generate_enumerations
from .interface import generate_interfaces
from .union import generate_unions


def init(root_src_dir, root_gen_dir, component_reldirs):
    """
    Args:
        root_src_dir: Project's root directory, which corresponds to "//" in GN.
        root_gen_dir: Root directory of generated files, which corresponds to
            "//out/Default/gen" in GN.
        component_reldirs: Pairs of component and output directory.
    """

    from . import package_initializer
    package_initializer.init(
        root_src_dir=root_src_dir,
        root_gen_dir=root_gen_dir,
        component_reldirs=component_reldirs)
