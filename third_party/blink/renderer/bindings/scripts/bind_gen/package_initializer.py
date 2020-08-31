# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from . import style_format
from .path_manager import PathManager


def init(**kwargs):
    """
    Initializes this package.  See PackageInitializer.__init__ for details
    about the arguments.
    """
    the_instance = PackageInitializer(**kwargs)
    the_instance.init()
    assert the_instance is PackageInitializer.the_instance()


def package_initializer():
    """
    Returns the instance of PackageInitializer that actually initialized this
    package.
    """
    the_instance = PackageInitializer.the_instance()
    assert the_instance
    return the_instance


class PackageInitializer(object):
    """
    PackageInitializer is designed to support 'multiprocessing' package so that
    users can initialize this package in another process with the same
    settings.

    When the 'start method' of 'multiprocessing' package is 'spawn', the global
    environment (e.g. module variables, class variables, etc.) will not be
    inherited.  See also https://docs.python.org/3/library/multiprocessing.html

    PackageInitializer helps reproduce the same runtime environment of this
    process in other processes.  PackageInitializer.init() initializes this
    package in the same way as it was originally initialized iff the current
    process' runtime environment has not yet been initialized.  In other words,
    PackageInitializer.init() works with any start method of multiprocessing
    package.
    """

    # The instance of PackageInitializer that actually initialized this
    # package.
    _the_instance = None

    @classmethod
    def the_instance(cls):
        return cls._the_instance

    def __init__(self, root_src_dir, root_gen_dir, component_reldirs):
        """
        Args:
            root_src_dir: Project's root directory, which corresponds to "//"
                in GN.
            root_gen_dir: Root directory of generated files, which corresponds
                to "//out/Default/gen" in GN.
            component_reldirs: Pairs of component and output directory.
        """

        self._root_src_dir = root_src_dir
        self._root_gen_dir = root_gen_dir
        self._component_reldirs = component_reldirs

    def init(self):
        if PackageInitializer._the_instance:
            return
        PackageInitializer._the_instance = self

        self._init()

    def _init(self):
        style_format.init(self._root_src_dir)

        PathManager.init(
            root_src_dir=self._root_src_dir,
            root_gen_dir=self._root_gen_dir,
            component_reldirs=self._component_reldirs)
